#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <stdint.h>

int nthreads;
int8_t stop_calc = 0;
int64_t longest_iteration = 0;
pthread_mutex_t mutex;

typedef struct {
    int32_t index;
    double partial_sum;
    int64_t iterations;
} thread_data;

void handle_sigint(int sig) {
    stop_calc = 1;
}

void mutex_lock_checked(pthread_mutex_t *m, sigset_t* old) {
    int rc;
    if ((rc = pthread_mutex_lock(m)) != 0) {
        fprintf(stderr, "Mutex lock failed: %s\n", strerror(rc));
        pthread_sigmask(SIG_UNBLOCK, &old, NULL);
        pthread_exit(NULL);
    }
}

void mutex_unlock_checked(pthread_mutex_t *m, sigset_t* old) {
    int rc;
    if ((rc = pthread_mutex_unlock(m)) != 0) {
        fprintf(stderr, "Mutex unlock failed: %s\n", strerror(rc));
        pthread_sigmask(SIG_UNBLOCK, &old, NULL);
        pthread_exit(NULL);
    }
}

void* calculate(void* param) {
    sigset_t set;
    sigset_t old;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    pthread_sigmask(SIG_BLOCK, &set, &old);
    thread_data* data = (thread_data*)param;
    int64_t i = data->index;

    for (int64_t j = 0; ; j++) {
        data->partial_sum += 1.0 / (i * 4.0 + 1.0);
        data->partial_sum -= 1.0 / (i * 4.0 + 3.0);
        i += nthreads;

        if (j % 1000000 == 0)
        {
            mutex_lock_checked(&mutex);
            if (j > longest_iteration)
            {
                longest_iteration = j;
            }
            if (longest_iteration <= j && stop_calc)
            {
                data->iterations = j;
                mutex_unlock_checked(&mutex, &old);
                pthread_sigmask(SIG_UNBLOCK, &old, NULL);
                pthread_exit(data);
            }
            mutex_unlock_checked(&mutex, &old);
        }
    }
}

int main(int argc, char** argv) {
    if (argc > 1) {
        nthreads = atol(argv[1]);
    }
    if (nthreads < 1 || nthreads > 100) {
        fprintf(stderr, "Invalid thread count (1-100)\n");
        exit(-1);
    }
    
    int rc;
    if ((rc = pthread_mutex_init(&mutex, NULL)) != 0) {
        fprintf(stderr, "Mutex unlock failed: %s\n", strerror(rc));
        exit(-1);
    }

    signal(SIGINT, handle_sigint);
    double pi = 0.0;
    int code;
    int flag = 1;
    pthread_t* ids = malloc(nthreads * sizeof(pthread_t));
    thread_data* data = malloc(nthreads * sizeof(thread_data));

    for (int i = 0; i < nthreads && flag; i++) {
        data[i].index = i;
        if ((code= pthread_create(&ids[i], NULL, calculate, data + i)) != 0) {
            fprintf(stderr, "%d: creating: %s\n", i, strerror(code));
            flag = 0;
        }
    }


    for (int i = 0; i < nthreads && flag; i++) {
        thread_data* res;
        if ((code = pthread_join(ids[i], (void**)&res)) != 0) {
            fprintf(stderr, "%d: joining: %s\n", i, strerror(code));
            flag = 0;
            continue;
        }
        if (res == NULL) {
            flag = 0;
            continue;
        }
        
        pi += res->partial_sum;
        printf("Thread %d runs %ld\n", data[i].index, data[i].iterations);
        printf("Thread %d partial sum %.16f\n", data[i].index, data[i].partial_sum);
    }

    free(ids);
    free(data);
    pthread_mutex_destroy(&mutex);
    
    if (!flag) {
        return -1;
    }

    pi *= 4.0;
    printf("pi = %.16f\n", pi);

    return 0;
}