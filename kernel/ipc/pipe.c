/*
 * Lethal Kernel - Pipe implementation
 * 
 * A circular buffer pipe for inter-process communication.
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "pipe.h"
#include "../core/console.h"
#include "../core/panic.h"
#include "../process/process.h"
#include "../process/task.h"

static struct pipe pipes[MAX_PIPES];
static int next_pipe_id = 1;

/* Per-process file descriptor table - static to this file */
struct pipe_handle process_handles[MAX_USER_PROCESSES][16];

static struct pipe *alloc_pipe(void)
{
    for (int i = 0; i < MAX_PIPES; i++) {
        if (pipes[i].pipe_id == 0) {
            struct pipe *p = &pipes[i];
            p->pipe_id = next_pipe_id++;
            p->head = 0;
            p->tail = 0;
            p->readers = 0;
            p->writers = 0;
            p->closed = 0;
            p->owner = NULL;
            mutex_init(&p->mutex);
            return p;
        }
    }
    return NULL;
}

static int find_free_fd(struct user_process *p)
{
    if (!p) return -1;
    for (int i = 3; i < 16; i++) { /* Skip stdin, stdout, stderr */
        if (process_handles[p->pid][i].pipe == NULL) {
            return i;
        }
    }
    return -1;
}

int pipe_close_internal(int pid, int fd)
{
    if (pid < 0 || pid >= MAX_USER_PROCESSES) return -1;
    if (fd < 0 || fd >= 16) return -1;
    
    struct pipe_handle *h = &process_handles[pid][fd];
    if (!h->pipe) return -1;
    
    struct pipe *pipe = h->pipe;
    
    if (h->is_reader) {
        pipe->readers--;
    } else if (h->is_writer) {
        pipe->writers--;
    }
    
    h->pipe = NULL;
    h->is_reader = 0;
    h->is_writer = 0;
    
    /* If nobody is using this pipe, free it */
    if (pipe->readers == 0 && pipe->writers == 0) {
        pipe->closed = 1;
        pipe->pipe_id = 0;
        serial_print("[pipe] pipe freed\n");
    }
    
    return 0;
}

void close_all_pipes_for_process(struct user_process *p)
{
    if (!p) return;
    for (int i = 0; i < 16; i++) {
        if (process_handles[p->pid][i].pipe) {
            pipe_close_internal(p->pid, i);
        }
    }
}

int is_pipe_fd(int pid, int fd)
{
    if (pid < 0 || pid >= MAX_USER_PROCESSES) return 0;
    if (fd < 0 || fd >= 16) return 0;
    return process_handles[pid][fd].pipe != NULL;
}

int pipe_create(struct user_process *owner, int *read_fd, int *write_fd)
{
    if (!owner) return -1;
    
    struct pipe *p = alloc_pipe();
    if (!p) return -1;
    
    p->owner = owner;
    p->readers = 1;
    p->writers = 1;
    
    /* Find first free fd for reading */
    int rfd = find_free_fd(owner);
    if (rfd < 0) {
        p->pipe_id = 0;
        return -1;
    }
    
    /* Temporarily reserve the read fd so find_free_fd doesn't return it again */
    process_handles[owner->pid][rfd].pipe = (struct pipe*)1; /* marker */
    
    /* Find second free fd for writing */
    int wfd = find_free_fd(owner);
    
    /* Remove the temporary marker */
    process_handles[owner->pid][rfd].pipe = NULL;
    
    if (wfd < 0) {
        p->pipe_id = 0;
        return -1;
    }
    
    /* Set up read fd */
    process_handles[owner->pid][rfd].pipe = p;
    process_handles[owner->pid][rfd].is_reader = 1;
    process_handles[owner->pid][rfd].is_writer = 0;
    
    /* Set up write fd */
    process_handles[owner->pid][wfd].pipe = p;
    process_handles[owner->pid][wfd].is_reader = 0;
    process_handles[owner->pid][wfd].is_writer = 1;
    
    *read_fd = rfd;
    *write_fd = wfd;
    
    serial_print("[pipe] created pipe #");
    serial_print_dec(p->pipe_id);
    serial_print(": read_fd=");
    serial_print_dec(rfd);
    serial_print(" write_fd=");
    serial_print_dec(wfd);
    serial_print("\n");
    
    return 0;
}

int pipe_write(int fd, const void *data, size_t count)
{
    struct user_process *p = process_current();
    if (!p) return -1;
    if (fd < 0 || fd >= 16) return -1;
    
    struct pipe_handle *h = &process_handles[p->pid][fd];
    if (!h->pipe || !h->is_writer) return -1;
    
    struct pipe *pipe = h->pipe;
    if (pipe->closed) return -1;
    if (pipe->readers == 0) return -1;
    
    mutex_lock(&pipe->mutex);
    
    const uint8_t *src = (const uint8_t *)data;
    size_t written = 0;
    
    while (written < count) {
        size_t avail = PIPE_BUFFER_SIZE - (pipe->head - pipe->tail);
        if (avail == 0) {
            if (pipe->readers == 0 || pipe->closed) {
                mutex_unlock(&pipe->mutex);
                return (int)written;
            }
            mutex_unlock(&pipe->mutex);
            task_yield();
            mutex_lock(&pipe->mutex);
            continue;
        }
        
        size_t space = avail;
        if (space > count - written) space = count - written;
        
        size_t idx = pipe->head % PIPE_BUFFER_SIZE;
        size_t first_chunk = PIPE_BUFFER_SIZE - idx;
        if (first_chunk > space) first_chunk = space;
        
        for (size_t i = 0; i < first_chunk; i++) {
            pipe->buffer[idx + i] = src[written + i];
        }
        
        if (first_chunk < space) {
            size_t remaining = space - first_chunk;
            for (size_t i = 0; i < remaining; i++) {
                pipe->buffer[i] = src[written + first_chunk + i];
            }
        }
        
        pipe->head += space;
        written += space;
    }
    
    mutex_unlock(&pipe->mutex);
    return (int)written;
}

int pipe_read(int fd, void *data, size_t count)
{
    struct user_process *p = process_current();
    if (!p) return -1;
    if (fd < 0 || fd >= 16) return -1;
    
    struct pipe_handle *h = &process_handles[p->pid][fd];
    if (!h->pipe || !h->is_reader) return -1;
    
    struct pipe *pipe = h->pipe;
    if (pipe->closed && pipe->head == pipe->tail) return 0;
    
    mutex_lock(&pipe->mutex);
    
    uint8_t *dst = (uint8_t *)data;
    size_t read_bytes = 0;
    
    while (read_bytes < count) {
        size_t avail = pipe->head - pipe->tail;
        if (avail == 0) {
            if (pipe->writers == 0 || pipe->closed) {
                mutex_unlock(&pipe->mutex);
                return (int)read_bytes;
            }
            mutex_unlock(&pipe->mutex);
            task_yield();
            mutex_lock(&pipe->mutex);
            continue;
        }
        
        size_t to_read = avail;
        if (to_read > count - read_bytes) to_read = count - read_bytes;
        
        size_t idx = pipe->tail % PIPE_BUFFER_SIZE;
        size_t first_chunk = PIPE_BUFFER_SIZE - idx;
        if (first_chunk > to_read) first_chunk = to_read;
        
        for (size_t i = 0; i < first_chunk; i++) {
            dst[read_bytes + i] = pipe->buffer[idx + i];
        }
        
        if (first_chunk < to_read) {
            size_t remaining = to_read - first_chunk;
            for (size_t i = 0; i < remaining; i++) {
                dst[read_bytes + first_chunk + i] = pipe->buffer[i];
            }
        }
        
        pipe->tail += to_read;
        read_bytes += to_read;
    }
    
    mutex_unlock(&pipe->mutex);
    return (int)read_bytes;
}

int pipe_close(int fd)
{
    struct user_process *p = process_current();
    if (!p) return -1;
    if (fd < 0 || fd >= 16) return -1;
    
    return pipe_close_internal(p->pid, fd);
}

int pipe_closed(int fd)
{
    struct user_process *p = process_current();
    if (!p) return 1;
    if (fd < 0 || fd >= 16) return 1;
    
    struct pipe_handle *h = &process_handles[p->pid][fd];
    if (!h->pipe) return 1;
    
    return h->pipe->closed;
}

size_t pipe_available(int fd)
{
    struct user_process *p = process_current();
    if (!p) return 0;
    if (fd < 0 || fd >= 16) return 0;
    
    struct pipe_handle *h = &process_handles[p->pid][fd];
    if (!h->pipe || !h->is_reader) return 0;
    
    return h->pipe->head - h->pipe->tail;
}

/* Copy file descriptors from parent to child during fork */
void pipe_copy_fds(struct user_process *parent, struct user_process *child)
{
    if (!parent || !child) return;
    
    for (int i = 0; i < 16; i++) {
        if (process_handles[parent->pid][i].pipe) {
            process_handles[child->pid][i] = process_handles[parent->pid][i];
            struct pipe *p = process_handles[parent->pid][i].pipe;
            if (process_handles[parent->pid][i].is_reader) {
                p->readers++;
            } else if (process_handles[parent->pid][i].is_writer) {
                p->writers++;
            }
        }
    }
}

/* Initialize file descriptors for a new process */
void pipe_init_fds(struct user_process *p)
{
    if (!p) return;
    for (int i = 0; i < 16; i++) {
        process_handles[p->pid][i].pipe = NULL;
        process_handles[p->pid][i].is_reader = 0;
        process_handles[p->pid][i].is_writer = 0;
    }
}