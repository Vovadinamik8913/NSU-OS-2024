#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <stdint.h>

int nthreads;
volatile int8_t stop_calc = 0;
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

void* calculate(void* param) {
    thread_data* data = (thread_data*)param;
    int64_t i = data->index;

    for (int64_t j = 0; ; j++) {
        data->partial_sum += 1.0 / (i * 4.0 + 1.0);
        data->partial_sum -= 1.0 / (i * 4.0 + 3.0);
        i += nthreads;

        if (j % 1000000 == 0)
        {
            pthread_mutex_lock(&mutex);
            if (j > longest_iteration)
            {
                longest_iteration = j;
            }
            if (longest_iteration <= j && stop_calc)
            {
                data->iterations = j;
                pthread_mutex_unlock(&mutex);
                pthread_exit(data);
            }
            pthread_mutex_unlock(&mutex);
        }
    }
}

int main(int argc, char** argv) {
    if (argc > 1) {
        nthreads = atol(argv[1]);
    }
    if (nthreads < 1 || nthreads > 100) {
        fprintf(stderr, "Invalid thread count (1-100)\n");
        return -1;
    }
    
    pthread_mutex_init(&mutex, NULL);
    signal(SIGINT, handle_sigint);
    double pi = 0.0;
    int code;
    int flag = 1;
    pthread_t* ids = malloc(nthreads * sizeof(pthread_t));
    thread_data* data = malloc(nthreads * sizeof(thread_data));

    for (int i = 0; i < nthreads && flag; i++) {
        data[i].index = i;
        if ((code= pthread_create(&ids[i], NULL, calculate, data + i)) != 0) {
            char* buf = strerror(code);
            fprintf(stderr, "%d: creating: %s\n", i, buf);
            flag = 0;
        }
    }


    for (int i = 0; i < nthreads && flag; i++) {
        thread_data* res;
        if ((code = pthread_join(ids[i], (void**)&res)) != 0) {
            char* buf = strerror(code);
            fprintf(stderr, "%d: joining: %s\n", i, buf);
            flag = 0;
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