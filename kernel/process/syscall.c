#include "syscall.h"
#include "process.h"
#include "../ipc/pipe.h"
#include "../memory/vmm.h"
#include "../core/console.h"
#include "../core/gdt.h"
#include <stddef.h>

static int64_t syscall_write(uint64_t fd, uint64_t buf, uint64_t len)
{
    if (len == 0) return 0;
    if (len > 4096) return -1;

    struct user_process *p = process_current();
    if (!p) return -1;

    if (fd < 16) {
        if (!vmm_user_range_valid_in(p->space, buf, len, 0)) return -1;
        return pipe_write((int)fd, (const void *)(uintptr_t)buf, (size_t)len);
    }

    if (fd != 1) return -1;
    if (!vmm_user_range_valid_in(p->space, buf, len, 0)) return -1;

    const char *str = (const char *)(uintptr_t)buf;
    for (uint64_t i = 0; i < len; i++) {
        serial_putc(str[i]);
        console_putc(str[i]);
    }
    return (int64_t)len;
}

static int64_t syscall_read(uint64_t fd, uint64_t buf, uint64_t len)
{
    if (len == 0 || len > 4096) return -1;

    struct user_process *p = process_current();
    if (!p) return -1;

    if (!vmm_user_range_valid_in(p->space, buf, len, 1)) return -1;

    if (fd < 16) {
        return pipe_read((int)fd, (void *)(uintptr_t)buf, (size_t)len);
    }

    return -1;
}

static int64_t syscall_pipe(uint64_t read_fd_ptr, uint64_t write_fd_ptr)
{
    struct user_process *p = process_current();
    if (!p) return -1;

    if (!vmm_user_range_valid_in(p->space, read_fd_ptr, sizeof(int), 1))
        return -1;
    if (!vmm_user_range_valid_in(p->space, write_fd_ptr, sizeof(int), 1))
        return -1;

    int read_fd, write_fd;
    int result = pipe_create(p, &read_fd, &write_fd);

    if (result == 0) {
        int *rfd = (int *)(uintptr_t)read_fd_ptr;
        int *wfd = (int *)(uintptr_t)write_fd_ptr;
        *rfd = read_fd;
        *wfd = write_fd;
    }

    return result;
}

static int64_t syscall_close(uint64_t fd)
{
    if (fd >= 16) return -1;
    return pipe_close((int)fd);
}

static int64_t syscall_sleep(uint64_t ms)
{
    struct user_process *p = process_current();
    if (!p) return -1;
    return process_sleep_ms(ms) == 0 ? 0 : -1;
}

static int64_t syscall_wait(uint64_t pid, uint64_t status_ptr)
{
    struct user_process *p = process_current();
    if (!p) return -1;

    int status = 0;

    if (status_ptr &&
        !vmm_user_range_valid_in(p->space, status_ptr, sizeof(int), 1)) {
        return -1;
    }

    int r = process_wait((int)pid, status_ptr ? &status : NULL);
    if (r < 0) return -1;

    if (status_ptr) {
        int *dst = (int *)(uintptr_t)status_ptr;
        *dst = status;
    }
    return r;
}

void syscall_handler(struct interrupt_frame *frame)
{
    switch (frame->rax) {
    case SYS_WRITE:
        frame->rax = (uint64_t)syscall_write(frame->rdi, frame->rsi, frame->rdx);
        break;
    case SYS_READ:
        frame->rax = (uint64_t)syscall_read(frame->rdi, frame->rsi, frame->rdx);
        break;
    case SYS_EXIT:
        process_exit_from_syscall(frame, (int)frame->rdi);
        break;
    case SYS_MMAP:
        frame->rax = (uint64_t)process_mmap(frame->rdi, frame->rsi);
        break;
    case SYS_BRK:
        frame->rax = (uint64_t)process_brk(frame->rdi);
        break;
    case SYS_FORK:
        frame->rax = (uint64_t)process_fork(frame);
        break;
    case SYS_YIELD:
        process_schedule_from_interrupt(frame);
        break;
    case SYS_PIPE:
        frame->rax = (uint64_t)syscall_pipe(frame->rdi, frame->rsi);
        break;
    case SYS_CLOSE:
        frame->rax = (uint64_t)syscall_close(frame->rdi);
        break;
    case SYS_SLEEP:
        frame->rax = (uint64_t)syscall_sleep(frame->rdi);
        break;
    case SYS_WAIT:
        frame->rax = (uint64_t)syscall_wait(frame->rdi, frame->rsi);
        break;
    default:
        frame->rax = (uint64_t)-1;
        break;
    }
}