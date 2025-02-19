#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <stdint.h>

int nthreads;
volatile int8_t stop_calc = 0;
volatile int64_t longest_iteration = 0;
pthread_barrier_t barrier;

typedef struct {
    int32_t index;
    double partial_sum;
    int64_t iterations;
} thread_data;

void handle_sigint(int sig) {
    stop_calc = 1;
}

void barrier_wait_checked(pthread_barrier_t *b, sigset_t* old) {
    int rc = pthread_barrier_wait(b);
    if (rc != 0 && rc != PTHREAD_BARRIER_SERIAL_THREAD) {
        fprintf(stderr, "Barrier wait failed: %s\n", strerror(rc));
        pthread_sigmask(SIG_UNBLOCK, old, NULL);
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

        if (j % 1000000 == 0) {
            barrier_wait_checked(&barrier, &old);
            if (j > longest_iteration) {
                longest_iteration = j;
            }
            if (longest_iteration == j && stop_calc) {
                data->iterations = j;
                printf("Thread %d: runs %ld; sum %.15g\n", data->index, data->iterations, data->partial_sum);
                pthread_sigmask(SIG_UNBLOCK, &old, NULL);
                pthread_exit(data);
            }
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
    if ((rc = pthread_barrier_init(&barrier, NULL, nthreads)) != 0) {
        fprintf(stderr, "Barrier init failed: %s\n", strerror(rc));
        exit(-1);
    }

    signal(SIGINT, handle_sigint);
    double pi = 0.0;
    int code;
    int flag = 1;
    pthread_t* ids = malloc(nthreads * sizeof(pthread_t));
    thread_data* data = malloc(nthreads * sizeof(thread_data));

    int actual_threads = 0;
    for (int i = 0; i < nthreads; i++) {
        data[i].index = i;
        if ((code= pthread_create(&ids[i], NULL, calculate, data + i)) != 0) {
            fprintf(stderr, "%d: creating: %s\n", i, strerror(code));
            flag = 0;
            break;
        }
        actual_threads++;
    }


    for (int i = 0; i < actual_threads; i++) {
        thread_data* res; 
        pthread_join(ids[i], (void**)&res);
        if (res == NULL || !flag) {
            flag = 0;
            continue;
        }
        pi += res->partial_sum;
    }

    free(ids);
    free(data);
    if ((rc = pthread_barrier_destroy(&barrier)) != 0) {
        fprintf(stderr, "Mutex destroy failed: %s\n", strerror(rc));
        exit(-1);
    }
    
    if (!flag) {
        exit(-1);
    }

    pi = pi * 4.0;
    printf("pi done - %.15g \n", pi);  ;

    exit(0);
}
