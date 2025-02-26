#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <stdint.h>

int nthreads;
volatile int8_t stop_calc = 0;
volatile int8_t isFinished = 0;
volatile int32_t cnt = 0;
pthread_barrier_t barrier;
pthread_mutex_t mutex;

typedef struct {
    int32_t index;
    double partial_sum;
    int64_t iterations;
} thread_data;

void handle_sigint(int sig) {
    stop_calc = 1;
}

void barrier_wait_checked(pthread_barrier_t* b, sigset_t* old) {
    int res = pthread_barrier_wait(b);
    if (res != 0 && res != PTHREAD_BARRIER_SERIAL_THREAD) {
        fprintf(stderr, "Barrier wait failed: %s\n", strerror(res));
        pthread_sigmask(SIG_UNBLOCK, old, NULL);
        exit(EXIT_FAILURE);
    }
}

void mutex_lock_checked(pthread_mutex_t* m, sigset_t* old) {
    int res = pthread_mutex_lock(m);
    if (res != 0) {
        fprintf(stderr, "Mutex lock failed: %s\n", strerror(res));
        pthread_sigmask(SIG_UNBLOCK, old, NULL);
        exit(EXIT_FAILURE);
    }
}

void mutex_unlock_checked(pthread_mutex_t* m, sigset_t* old) {
    int res = pthread_mutex_unlock(m);
    if (res != 0) {
        fprintf(stderr, "Mutex unlock failed: %s\n", strerror(res));
        pthread_sigmask(SIG_UNBLOCK, old, NULL);
        exit(EXIT_FAILURE);
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

        if (j % 1000000 == 0) {
            barrier_wait_checked(&barrier, &old);
            mutex_lock_checked(&mutex, &old);
            if (isFinished) {
                data->iterations = j;
                printf("Thread %d: runs %ld; sum %.15g\n", data->index, data->iterations, data->partial_sum);
                mutex_unlock_checked(&mutex, &old);
                pthread_sigmask(SIG_UNBLOCK, &old, NULL);
                pthread_exit(data);
            }
            cnt++;
            if (cnt == nthreads) {
                cnt = 0;
                if (stop_calc) {
                    isFinished = 1;
                }   
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
        exit(EXIT_FAILURE);
    }
    
    int rc;
    if ((rc = pthread_barrier_init(&barrier, NULL, nthreads)) != 0) {
        fprintf(stderr, "Barrier init failed: %s\n", strerror(rc));
        exit(EXIT_FAILURE);
    }

    if ((rc = pthread_mutex_init(&mutex, NULL)) != 0) {
        fprintf(stderr, "Mutex init failed: %s\n", strerror(rc));
        exit(EXIT_FAILURE);
    }

    signal(SIGINT, handle_sigint);
    double pi = 0.0;
    int code;
    pthread_t* ids = malloc(nthreads * sizeof(pthread_t));
    thread_data* data = malloc(nthreads * sizeof(thread_data));

    int actual_threads = 0;
    for (int i = 0; i < nthreads; i++) {
        data[i].index = i;
        if ((code= pthread_create(&ids[i], NULL, calculate, data + i)) != 0) {
            fprintf(stderr, "%d: creating: %s\n", i, strerror(code));
            exit(EXIT_FAILURE);
        }
        actual_threads++;
    }


    for (int i = 0; i < actual_threads; i++) {
        thread_data* res; 
        pthread_join(ids[i], (void**)&res);
        pi += res->partial_sum;
    }

    free(ids);
    free(data);
    if ((rc = pthread_barrier_destroy(&barrier)) != 0) {
        fprintf(stderr, "Barrier destroy failed: %s\n", strerror(rc));
        exit(EXIT_FAILURE);
    }

    if ((rc = pthread_mutex_destroy(&mutex)) != 0) {
        fprintf(stderr, "Mutex destroy failed: %s\n", strerror(rc));
        exit(EXIT_FAILURE);
    }

    pi = pi * 4.0;
    printf("pi done - %.15g \n", pi);  ;

    exit(EXIT_SUCCESS);
}
