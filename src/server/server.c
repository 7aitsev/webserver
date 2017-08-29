#include "config/config.h"
#include "server/handler.h"

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
reload_config(int nsig)
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
        perror("getpwnam()");
        exit(EXIT_FAILURE);
    }
    if(NULL == (g = getgrnam(g_conf.group)))
    {
        perror("getgrnam()");
        exit(EXIT_FAILURE);
    }

    return (struct perms) {p->pw_uid, g->gr_gid};
}

static void
drop_privileges(uid_t uid, gid_t gid)
{
    if(-1 == setgid(gid))
    {
        perror("[server] setgid()");
        exit(EXIT_FAILURE);
    }
    if(-1 == setuid(uid))
    {
        perror("[server] setuid()");
        exit(EXIT_FAILURE);
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
    sigprocmask(SIG_SETMASK, &mask, NULL);
}

static void
setup_ipc()
{
    struct sigaction sa, sadfl;
    
    setsigmask();

    sa.sa_handler = reload_config;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGHUP, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    sadfl.sa_handler = SIG_DFL;
    sigaction(SIGCHLD, &sadfl, NULL);

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
    struct addrinfo hints;
    struct addrinfo* servinfo;
    struct addrinfo* p;
    int sfd;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_protocol = IPPROTO_TCP;

    if(0 != (status = getaddrinfo(NULL, "http", &hints, &servinfo)))
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        exit(EXIT_FAILURE);
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
            exit(EXIT_FAILURE);
        }

        if(0 == bind(sfd, p->ai_addr, p->ai_addrlen))
        {
            break;
        }

        close(sfd);
    }

    if(p == NULL)
    {
        fprintf(stderr, "Could not bind\n");
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(servinfo);

    if(-1 == listen(sfd, BACKLOG_SIZE))
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    return sfd;
}

static int
run_server(int sfd)
{
    int i;
    int status;
    int fdmax = sfd;
    fd_set cache;
    fd_set work;
    struct sockaddr_storage client;
    socklen_t addr_size = sizeof(client);
    int cs;
    int bytes;
    char buf[1024];

    FD_ZERO(&cache);
    FD_SET(sfd, &cache);
 
    while(1)
    {
        work = cache;
        status = select(fdmax + 1, &work, NULL, NULL, NULL);
        if(status > 0)
        {
            for(i = 3; i <= fdmax; ++i)
            {
                if(FD_ISSET(i, &work))
                {
                    if(i != sfd)
                    {
                        if(0 < (bytes = recv(i, (void*) buf, 1024, 0)))
                        {
                            make_response(i, buf);
                            close(i);
                            FD_CLR(i, &cache);
                        }
                        else
                        {
                            if(bytes == 0)
                            {
                                printf("socket %d hung up\n", i);
                            }
                            else
                            {
                                perror("recv()");
                            }
                            close(i);
                            FD_CLR(i, &cache);
                        }
                    }
                    else
                    {
                        addr_size = sizeof(client);
                        if(-1 != (cs = accept(sfd, (struct sockaddr*) &client,
                                        &addr_size)))
                        {
                            FD_SET(cs, &cache);
                            fdmax = (cs > fdmax) ? cs : fdmax;
                            printf("new client: %d\n", cs);
                        }
                        else
                        {
                            perror("accept()");
                        }
                    }
                }
            }
        }
        else if(-1 == status)
        {
            if(errno == EINTR && 0 != g_nsig)
            {
                if(SEM_FAILED != g_sem && SIGHUP == g_nsig)
                {
                    printf("[server] Reload requested\n");
                    sem_post(g_sem);
                }
                else
                {
                    printf("[server] Normal termination\n");
                }
                shutdown(sfd, SHUT_RDWR);
                FD_CLR(sfd, &cache);
                break;
            }
            else
            {
                perror("select()");
                break;
            }
        }
    }
    
    printf("[server] Finished\n");
    return 0;
}

int
runservinproc()
{   
    pid_t servpid = fork();
    if(0 == servpid)
    {
        int rv;
        struct perms p = get_perms();
        int listensocket = prepare_server();
        (void) setup_ipc();
        if(-1 == chroot(g_conf.document_root))
        {
            perror("[server] chroot()");
            exit(EXIT_FAILURE);
        }
        (void) drop_privileges(p.p_uid, p.p_gid);

        rv = run_server(listensocket);
        exit(rv);
    }
    return servpid;
}

/*int
main(int argc, char** argv)
{
    int rv;
    if(0 == (rv = cfgserv(argc, argv, &g_sconf)))
    {
        int sfd = prepare_server();
        run_server(sfd);
    }
    return rv;
}*/
