#include "config/config.h"
#include "server/worker.h"

#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <netdb.h>
#include <semaphore.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h> // for memset
#include <sys/stat.h> 
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#define BACKLOG_SIZE 3

struct perms {
    uid_t p_uid;
    gid_t p_gid;
};

static sem_t* g_sem;

/** Config for the whole program */
extern struct conf g_conf;

static volatile sig_atomic_t g_nsig = 0;

static void
sig_reload_config(int nsig)
{
    g_nsig = nsig;
}

static struct perms
get_perms()
{
    struct passwd* p;
    struct group* g;

    if(NULL == (p = getpwnam(g_conf.user)))
    {
        perror("[server] getpwnam()");
        p->pw_uid = 0;
    }
    if(NULL == (g = getgrnam(g_conf.group)))
    {
        perror("[server] getgrnam()");
        g->gr_gid = 0;
    }

    return (struct perms) {p->pw_uid, g->gr_gid};
}

static void
drop_privileges(uid_t uid, gid_t gid)
{
    if(0 != uid && -1 == setgid(gid))
    {
        perror("[server] setgid()");
        _exit(EXIT_FAILURE);
    }
    if(0 != gid && -1 == setuid(uid))
    {
        perror("[server] setuid()");
        _exit(EXIT_FAILURE);
    }
}

static void
setsigmask()
{
    sigset_t mask;
    sigfillset(&mask);
    sigdelset(&mask, SIGFPE);
    sigdelset(&mask, SIGILL);
    sigdelset(&mask, SIGSEGV);
    sigdelset(&mask, SIGCONT);
    sigdelset(&mask, SIGHUP);
    sigdelset(&mask, SIGINT);
    sigdelset(&mask, SIGTERM);
    sigdelset(&mask, SIGCHLD);
    sigprocmask(SIG_SETMASK, &mask, NULL);
}

static void
setup_ipc()
{
    struct sigaction sa, sadfl;
    
    setsigmask();

    sa.sa_handler = sig_reload_config;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGHUP, &sa, NULL);

    sadfl.sa_handler = SIG_DFL;
    sigaction(SIGCHLD, &sadfl, NULL);
    sigaction(SIGINT, &sadfl, NULL);
    sigaction(SIGTERM, &sadfl, NULL);

    g_sem = sem_open("/webserver.sem", O_RDWR);
    if(SEM_FAILED == g_sem)
    {
        perror("[server] sem_open");
    }
}

static int
prepare_server()
{
    int status;
    char* port = g_conf.port;
    char* host = g_conf.host;
    struct addrinfo hints;
    struct addrinfo* servinfo;
    struct addrinfo* p;
    int sfd;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_protocol = IPPROTO_TCP;

    if(0 != (status = getaddrinfo(host, port, &hints, &servinfo)))
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        _exit(EXIT_FAILURE);
    }

    int yes = 1;
    for(p = servinfo; NULL != p; p = p->ai_next)
    {
        if(-1 == (sfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)))
        {
            continue;
        }

        if(-1 == setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes))) {
            perror("setsockopt");
            _exit(EXIT_FAILURE);
        }

        if(0 == bind(sfd, p->ai_addr, p->ai_addrlen))
        {
            break;
        }

        close(sfd);
    }

    if(p == NULL)
    {
        fprintf(stderr, "[server] Could not bind\n");
        _exit(EXIT_FAILURE);
    }

    freeaddrinfo(servinfo);

    if(-1 == listen(sfd, BACKLOG_SIZE))
    {
        perror("listen");
        _exit(EXIT_FAILURE);
    }

    return sfd;
}

static int
run_server(int master)
{
    int slave;
    struct sockaddr_storage client;
    socklen_t addr_size = sizeof(client);

    while(1)
    {
        slave = accept(master, (struct sockaddr*) &client, &addr_size);
        if(-1 != slave)
        {
            printf("[server] new client: %d\n", slave);
            worker_fd_pass(slave);
            close(slave);
        }
        else
        {
            if(EINTR != errno)
            {
                perror("[server] accept()");
                return EXIT_FAILURE;
            }
        }

        if(0 != g_nsig)
        {
            if(SEM_FAILED != g_sem)
            {
                printf("[server] Reload requested\n");
                sem_post(g_sem);
                break;
            }
        }
        else if(is_reaping_needed())
        {
            printf("[server] Termination requested\n");
            break;
        }
        else if(is_respawn_needed())
        {
            printf("[server] Respawn a worker\n");
            respawn_worker();
        }
    }

    shutdown(master, SHUT_RDWR);
    return EXIT_SUCCESS;
}

int
runservinproc()
{
    pid_t servpid = fork();
    if(0 == servpid)
    {
        int rv;
        (void) setup_ipc();
        struct perms p = get_perms();
        int listensocket = prepare_server();
        if(-1 == chroot(g_conf.document_root))
        {
            perror("[server] chroot()");
            _exit(EXIT_FAILURE);
        }
        (void) drop_privileges(p.p_uid, p.p_gid);

        init_workers();
        rv = run_server(listensocket);
        reap_workers();
        _exit(rv);
    }
    return servpid;
}
    
void
daemonize()
{
    int fdlog;
    pid_t pid = fork();
    if(-1 == pid)
    {
        perror("[server] The first fork() failed");
        exit(EXIT_FAILURE);
    }
    else if(0 != pid)
    {
        _exit(EXIT_SUCCESS);
    }

    setsid();

    pid = fork();
    if(-1 == pid)
    {
        perror("[server] The second fork() failed");
        exit(EXIT_FAILURE);
    }
    else if(0 != pid)
    {
        _exit(EXIT_SUCCESS);
    }

    if(-1 == chdir("/"))
    {
        perror("[server] Chdir failed");
        exit(EXIT_FAILURE);
    }

    umask(0);

    close(STDIN_FILENO);
    if(-1 == open("/dev/null", O_RDONLY))
    {
        perror("[server] Open /dev/null failed");
        exit(EXIT_FAILURE);
    }
    fdlog = open(g_conf.log_path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if(fdlog == -1)
    {
        fprintf(stderr, "[server] Failed to open logfile: %s\n\t",
                    g_conf.log_path);
        perror("");
    }
    dup2(fdlog, STDOUT_FILENO);
    dup2(fdlog, STDERR_FILENO);
    close(fdlog);
    setvbuf(stdout, NULL, _IONBF, 0);
}
