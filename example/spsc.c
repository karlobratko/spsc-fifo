#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "../spsc-fifo.h"

#define array(T, N) typeof(T[N])
#define ignore (void)

#define NS_IN_MS 1000000

#define PRODUCER_ITERATIONS 10

#define MESSAGE_SIZE 64

struct message {
    uint id;
    array(char, MESSAGE_SIZE) buffer;
};

#define FIFO_SIZE sizeof(struct message) * 2

spsc_fifo *fifo;

atomic_bool producer_done = false;

void *produce(void *arg) {
    ignore arg;
    spsc_fifo_bind_producer(fifo);

    for (uint i = 0; i < PRODUCER_ITERATIONS; ++i) {
        struct message message = {.id = i};
        snprintf(message.buffer, MESSAGE_SIZE, "This is message %d.", i);

        while (!spsc_fifo_write_obj(fifo, &message)) {
            nanosleep(&(struct timespec) {.tv_sec = 0, .tv_nsec = 20 * NS_IN_MS}, NULL);
        }

        nanosleep(&(struct timespec) {.tv_sec = 0, .tv_nsec = 500 * NS_IN_MS}, NULL);
    }

    atomic_store(&producer_done, true);

    return NULL;
}

void *consume(void *arg) {
    ignore arg;
    spsc_fifo_bind_consumer(fifo);

    while (true) {
        struct message message;
        if (spsc_fifo_read_obj(fifo, &message)) {
            printf("Received message %d: %s\n", message.id, message.buffer);
        } else {
            if (atomic_load(&producer_done) == true && spsc_fifo_is_empty(fifo)) {
                break;
            }

            nanosleep(&(struct timespec) {.tv_sec = 0, .tv_nsec = 20 * NS_IN_MS}, NULL);
        }
    }

    return NULL;
}

int main(void) {
    if (spsc_fifo_aligned_alloc(&fifo, FIFO_SIZE, _Alignof(struct message)) != spsc_fifo_alloc_success) {
        fprintf(stderr, "failed to allocate fifo");
        return EXIT_FAILURE;
    }

    pthread_t producer;
    pthread_t consumer;

    if (pthread_create(&producer, NULL, produce, NULL) != 0) {
        fprintf(stderr, "failed to create producer");
        return EXIT_FAILURE;
    }

    if (pthread_create(&consumer, NULL, consume, NULL) != 0) {
        fprintf(stderr, "failed to create consumer");
        return EXIT_FAILURE;
    }

    if (pthread_join(producer, NULL) != 0) {
        fprintf(stderr, "failed to join producer");
        return EXIT_FAILURE;
    }

    if (pthread_join(consumer, NULL) != 0) {
        fprintf(stderr, "failed to join consumer");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}