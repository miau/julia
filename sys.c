/*
  sys.c
  I/O and operating system utility functions
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <setjmp.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <limits.h>
#include <errno.h>
#include <math.h>
#include <signal.h>
#include <libgen.h>
#include <fcntl.h>
#include <unistd.h>
#ifdef BOEHM_GC
#include <gc.h>
#endif
#include "llt.h"
#include "julia.h"

// --- system word size ---

int jl_word_size()
{
#ifdef BITS64
    return 64;
#else
    return 32;
#endif
}

// --- io and select ---

void jl__not__used__()
{
    // force inclusion of lib/socket.o in executable
    short p=0;
    open_any_tcp_port(&p);
}

DLLEXPORT
int jl_wait_msg(int fd)
{
    fd_set fds, efds;
    FD_ZERO(&fds);
    FD_ZERO(&efds);
    FD_SET(fd, &fds);
    FD_SET(fd, &efds);
    select(fd+1, &fds, NULL, &efds, NULL);
    if (FD_ISSET(fd, &efds)) {
        ios_printf(ios_stderr, "error fd\n");
        return 1;
    }
    if (FD_ISSET(fd, &fds)) {
        return 0;
    }
    return 1;
}

DLLEXPORT
int jl_read_avail(ios_t *s)
{
    int fd = s->fd;
    fd_set fds;
    struct timeval tout;
    tout.tv_sec = 0;
    tout.tv_usec = 0;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    select(fd+1, &fds, NULL, NULL, &tout);
    if (FD_ISSET(fd, &fds))
        return 1;
    return 0;
}

DLLEXPORT uint32_t jl_getutf8(ios_t *s)
{
    uint32_t wc=0;
    ios_getutf8(s, &wc);
    return wc;
}

DLLEXPORT
int32_t jl_nb_available(ios_t *s)
{
    return (int32_t)(s->size - s->bpos);
}

// --- io constructors ---

DLLEXPORT
void *jl_new_fdio(int fd)
{
    ios_t *s = (ios_t*)alloc_cobj(sizeof(ios_t));
    ios_fd(s, fd, 0);
    return s;
}

DLLEXPORT
void *jl_new_fileio(char *fname, int rd, int wr, int create, int trunc)
{
    ios_t *s = (ios_t*)alloc_cobj(sizeof(ios_t));
    if (ios_file(s, fname, rd, wr, create, trunc) == NULL)
        jl_errorf("could not open file %s", fname);
    return s;
}

DLLEXPORT
void *jl_new_memio(uint32_t sz)
{
    ios_t *s = (ios_t*)alloc_cobj(sizeof(ios_t));
    if (ios_mem(s, sz) == NULL)
        jl_errorf("error creating memory I/O stream");
    return s;
}

// --- current output stream ---

ios_t *jl_current_output_stream_noninline()
{
    return jl_current_output_stream();
}

void jl_set_current_output_stream_noninline(ios_t *s)
{
    jl_set_current_output_stream(s);
}

// --- buffer manipulation ---

jl_array_t *jl_takebuf_array(ios_t *s)
{
    size_t n;
    char *b = ios_takebuf(s, &n);
    jl_array_t *a = jl_pchar_to_array(b, n-1);
    LLT_FREE(b);
    return a;
}

jl_value_t *jl_takebuf_string(ios_t *s)
{
    size_t n;
    char *b = ios_takebuf(s, &n);
    jl_value_t *v = jl_pchar_to_string(b, n-1);
    LLT_FREE(b);
    return v;
}

// -- syscall utilities --

int jl_errno()
{
    return errno;
}

jl_value_t *jl_strerror(int errnum)
{
    char *str = strerror(errnum);
    return jl_pchar_to_string((char*)str, strlen(str));
}

// -- child process status --

int jl_process_exited(int status)      { return WIFEXITED(status); }
int jl_process_signaled(int status)    { return WIFSIGNALED(status); }
int jl_process_stopped(int status)     { return WIFSTOPPED(status); }

int jl_process_exit_status(int status) { return WEXITSTATUS(status); }
int jl_process_term_signal(int status) { return WTERMSIG(status); }
int jl_process_stop_signal(int status) { return WSTOPSIG(status); }

// -- access to std filehandles --

int jl_stdin()  { return STDIN_FILENO; }
int jl_stdout() { return STDOUT_FILENO; }
int jl_stderr() { return STDERR_FILENO; }