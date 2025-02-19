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
volatile int32_t cnt;
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
            int res = pthread_barrier_wait(&barrier);
            if (stop_calc) {
                data->iterations = j;
                pthread_barrier_destroy(&barrier)
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
    cnt = nthreads;
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
            exit(-1);
        }
        actual_threads++;
    }


    for (int i = 0; i < actual_threads; i++) {
        thread_data* res; 
        pthread_join(ids[i], (void**)&res);
        if (res == NULL) {
            exit(-1);
        }
        printf("Thread %d: runs %ld; sum %.15g\n", res->index, res->iterations, res->partial_sum);
        pi += res->partial_sum;
    }

    free(ids);
    free(data);
    if ((rc = pthread_barrier_destroy(&barrier)) != 0) {
        fprintf(stderr, "Barrier destroy failed: %s\n", strerror(rc));
        exit(-1);
    }

    pi = pi * 4.0;
    printf("pi done - %.15g \n", pi);  ;

    exit(0);
}
