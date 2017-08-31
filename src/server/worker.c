#include "server/worker.h"
#include "server/handler.h"

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

typedef unsigned char wid_t;
#define WORKERS_CNT 4 

static volatile sig_atomic_t g_doRespawn = 0;
static volatile sig_atomic_t g_doReaping = 0;
static volatile sig_atomic_t g_doShutdown = 0;

static struct worker
{
    wid_t w_id;
    int w_sfd;
    pid_t w_pid;
} g_workers[WORKERS_CNT];

static ssize_t
sock_fd_write(int sock, void* buf, ssize_t buflen, int fd)
{
    ssize_t size;
    struct msghdr msg;
    struct iovec iov;
    union {
        struct cmsghdr cmsghdr;
        char control[CMSG_SPACE(sizeof(int))];
    } cmsgu;
    struct cmsghdr *cmsg;

    iov.iov_base = buf;
    iov.iov_len = buflen;

    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    if(-1 != fd)
    {
        msg.msg_control = cmsgu.control;
        msg.msg_controllen = sizeof(cmsgu.control);

        cmsg = CMSG_FIRSTHDR(&msg);
        cmsg->cmsg_len = CMSG_LEN(sizeof(int));
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;

        printf("[worker] write: passing fd %d\n", fd);
        *((int *) CMSG_DATA(cmsg)) = fd;
    } else {
        msg.msg_control = NULL;
        msg.msg_controllen = 0;
        printf("[worker] write: not passing fd\n");
    }

    size = sendmsg(sock, &msg, 0);

    if(size < 0)
        perror("[worker] sendmsg()");

    return size;
}

static ssize_t
sock_fd_read(int sock, void *buf, ssize_t bufsize, int *fd)
{
    ssize_t size;

    if(fd)
    {
        struct msghdr msg;
        struct iovec iov;
        union {
            struct cmsghdr cmsghdr;
            char control[CMSG_SPACE(sizeof(int))];
        } cmsgu;
        struct cmsghdr* cmsg;

        iov.iov_base = buf;
        iov.iov_len = bufsize;

        msg.msg_name = NULL;
        msg.msg_namelen = 0;
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_control = cmsgu.control;
        msg.msg_controllen = sizeof(cmsgu.control);
        size = recvmsg(sock, &msg, 0);
        if(size < 0)
        {
            perror("[worker] recvmsg()");
            _exit(EXIT_FAILURE);
        }
        cmsg = CMSG_FIRSTHDR(&msg);
        if(cmsg && cmsg->cmsg_len == CMSG_LEN(sizeof(int)))
        {
            if(cmsg->cmsg_level != SOL_SOCKET)
            {
                fprintf(stderr, "[worker] read: invalid cmsg_level %d\n",
                        cmsg->cmsg_level);
                _exit(EXIT_FAILURE);
            }
            if(cmsg->cmsg_type != SCM_RIGHTS)
            {
                fprintf(stderr, "[worker] read: invalid cmsg_type %d\n",
                        cmsg->cmsg_type);
                _exit(EXIT_FAILURE);
            }

            *fd = *((int *) CMSG_DATA(cmsg));
            printf("[worker] read: received fd %d\n", *fd);
        }
        else
        {
            *fd = -1;
        }
    }
    else
    {
        size = read(sock, buf, bufsize);
        if(size < 0)
        {
            perror("read()");
            _exit(EXIT_FAILURE);
        }
    }
    return size;
}

static int
get_vacant_worker_id()
{
    static wid_t wid = 0;
    return wid++ % WORKERS_CNT;
}

static void
worker_routine(int sock)
{
    int i;
    int bytes;
    int fdmax = sock;
    int status;
    char buf[1024];
    fd_set cache;
    fd_set work;

    FD_ZERO(&cache);
    FD_SET(sock, &cache);

    while(1)
    {
        work = cache;
        status = select(fdmax + 1, &work, NULL, NULL, NULL);
        if(status > 0)
        {
            for(i = 3; i <= fdmax; ++i)
                if(FD_ISSET(i, &work))
                {
                    if(i != sock)
                    {
                        if(0 < (bytes = recv(i, (void*) buf, 1024, 0)))
                        {
                            shutdown(i, SHUT_RD);
                            make_response(i, buf);
                        }
                        else
                        {
                            if(bytes == 0)
                            {
                                printf("[worker] socket %d hung up\n", i);
                            }
                            else if(EINTR != errno)
                            {
                                perror("[worker] recv");
                            }
                        }
                        shutdown(i, SHUT_WR);
                        close(i);
                        FD_CLR(i, &cache);
                    }
                    else
                    {
                        int fd;
                        char c;
                        ssize_t s = sock_fd_read(sock, (void*) &c, 1, &fd);
                        if(s <= 0)
                        {
                            if(EINTR != errno)
                            {
                                fprintf(stderr, "[worker] fd passing failed\n");
                            }
                        }
                        else
                        {
                            FD_SET(fd, &cache);
                            fdmax = (fdmax > fd) ? fdmax : fd;
                        }
                    }
                }
        }
        else if(-1 == status)
        {
            if(EINTR != errno)
            {
                perror("[worker] select");
                break;
            }
        }

        if(0 != g_doShutdown)
        {
            printf("[worker] Shutdown requested\n");
            break;
        }
    }
}

static void
sig_shutdown(int nsig)
{
    g_doShutdown = nsig;
}

static void
setup_worker_ipc()
{
    sigset_t mask;
    sigfillset(&mask);
    sigdelset(&mask, SIGFPE);
    sigdelset(&mask, SIGILL);
    sigdelset(&mask, SIGSEGV);
    sigdelset(&mask, SIGCONT);
    sigdelset(&mask, SIGINT);
    sigdelset(&mask, SIGTERM);
    sigprocmask(SIG_SETMASK, &mask, NULL);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sig_shutdown;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

static int
new_worker(int wid)
{
    int sfd[2];
    int status;
    pid_t pid;

    if(-1 == (status = socketpair(AF_UNIX, SOCK_STREAM, 0, sfd)))
    {
        perror("[worker] socketpair()");
        return EXIT_FAILURE;
    }

    pid = fork();
    switch(pid)
    {
        case 0:
            close(sfd[0]);
            setup_worker_ipc();
            worker_routine(sfd[1]);
            _exit(EXIT_SUCCESS);
        case -1:
            perror("[worker] fork failed while creating a new worker");
            return EXIT_FAILURE;
        default:
            close(sfd[1]);
            g_workers[wid].w_id = wid;
            g_workers[wid].w_sfd = sfd[0];
            g_workers[wid].w_pid = pid;
    }
    return wid;
}

int
respawn_worker()
{
    int i;
    int rv;

    g_doRespawn = 0;

    for(i = 0; i < WORKERS_CNT; ++i)
    {
        if(0 == (rv = waitpid(g_workers[i].w_pid, NULL, WNOHANG)))
        {
            continue;
        }
        else if(-1 != rv)
        {
            new_worker(i);
        }
    }
    return EXIT_SUCCESS;
}

void
reap_workers()
{
    int i;
    for(i = 0; i < WORKERS_CNT; ++i)
    {
        kill(g_workers[i].w_pid, SIGTERM);
    }
    for(i = 0; i < WORKERS_CNT; ++i)
    {
        waitpid(g_workers[i].w_pid, NULL, 0);
    }
}

static void
sig_worker_died(int nsig)
{
    g_doRespawn = nsig;
}

int
is_respawn_needed()
{
    return g_doRespawn;
}

static void
sig_worker_term(int nsig)
{
    g_doReaping = nsig;
}

int
is_reaping_needed()
{
    return g_doReaping;
}

static void
setup_master_ipc()
{
    struct sigaction sa_reap, sa_term;
    memset(&sa_reap, 0, sizeof(sa_reap));
    sa_reap.sa_handler = sig_worker_died;
    sigaction(SIGCHLD, &sa_reap, NULL);

    memset(&sa_term, 0, sizeof(sa_term));
    sa_term.sa_handler = sig_worker_term;
    sigaction(SIGTERM, &sa_term, NULL);
    sigaction(SIGINT, &sa_term, NULL);
}

void
init_workers()
{
    int i;
    for(i = 0; i < WORKERS_CNT; ++i)
    {
        new_worker(i);
    }

    setup_master_ipc();
}

int
worker_fd_pass(int fd)
{
    ssize_t size;
    size = sock_fd_write(g_workers[get_vacant_worker_id()].w_sfd, "1", 1, fd);
    return size;
}
