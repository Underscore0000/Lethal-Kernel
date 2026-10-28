/*
 * Lethal Kernel - IPC Pipes
 * 
 * A circular buffer pipe for inter-process communication.
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#ifndef LETHAL_PIPE_H
#define LETHAL_PIPE_H

#include <stdint.h>
#include <stddef.h>
#include "../core/mutex.h"
#include "../process/process.h"

#define PIPE_BUFFER_SIZE 4096
#define MAX_PIPES 64

struct pipe {
    uint8_t buffer[PIPE_BUFFER_SIZE];
    volatile size_t head;
    volatile size_t tail;
    volatile int readers;
    volatile int writers;
    volatile int closed;
    mutex_t mutex;
    int pipe_id;
    struct user_process *owner;
};

struct pipe_handle {
    struct pipe *pipe;
    int is_reader;
    int is_writer;
};

/* Per-process file descriptor table - defined in pipe.c */
extern struct pipe_handle process_handles[MAX_USER_PROCESSES][16];

/* Pipe creation and management */
int pipe_create(struct user_process *owner, int *read_fd, int *write_fd);
int pipe_write(int fd, const void *data, size_t count);
int pipe_read(int fd, void *data, size_t count);
int pipe_close(int fd);
int pipe_closed(int fd);
size_t pipe_available(int fd);

/* Internal functions used by process.c */
int pipe_close_internal(int pid, int fd);
void close_all_pipes_for_process(struct user_process *p);
int is_pipe_fd(int pid, int fd);

#endif