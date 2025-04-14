#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <threads.h>
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

int produce(void *arg) {
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

    return EXIT_SUCCESS;
}

int consume(void *arg) {
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

    return EXIT_SUCCESS;
}

int main(void) {
    int status = EXIT_SUCCESS;

    if (spsc_fifo_aligned_alloc(&fifo, FIFO_SIZE, _Alignof(struct message)) != spsc_fifo_alloc_success) {
        fprintf(stderr, "failed to allocate fifo");
        status = EXIT_FAILURE;
        goto out;
    }

    thrd_t producer;
    thrd_t consumer;

    if (thrd_create(&producer, produce, NULL) != 0) {
        fprintf(stderr, "failed to create producer");
        status = EXIT_FAILURE;
        goto fifo_out;
    }

    if (thrd_create(&consumer, consume, NULL) != 0) {
        fprintf(stderr, "failed to create consumer");
        status = EXIT_FAILURE;
        goto fifo_out;
    }

    if (thrd_join(producer, NULL) != 0) {
        fprintf(stderr, "failed to join producer");
        status = EXIT_FAILURE;
        goto fifo_out;
    }

    if (thrd_join(consumer, NULL) != 0) {
        fprintf(stderr, "failed to join consumer");
        status = EXIT_FAILURE;
        goto fifo_out;
    }

fifo_out:
    spsc_fifo_free(&fifo);
out:
    return status;
}