#include "config/config.h"
#include "server/server.h"

#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/** Config for the whole program */
extern struct conf g_conf;

static int g_isRunning;
static int g_doReconfiguration;

static volatile sig_atomic_t g_nsig;

/** Adress of the semaphore */
static sem_t* g_sem;

static void
sig_handler(int nsig)
{
    g_nsig = nsig;
    printf("[bad idea!] #sig = %d\n", g_nsig);
}

static int
setupsig()
{
    int rv = 0;
    struct sigaction sa;

    sa.sa_handler = sig_handler;
    sigemptyset(&sa.sa_mask);

    rv |= sigaction(SIGINT, &sa, NULL);
    rv |= sigaction(SIGTERM, &sa, NULL);
    rv |= sigaction(SIGCHLD, &sa, NULL);
    rv |= sigaction(SIGHUP, &sa, NULL);

    return rv;
}

static int
setupsem()
{
    if(SEM_FAILED != (g_sem = 
                sem_open("/webserver.sem", O_CREAT | O_RDWR, 0666, 0)))
    {
        return 0;
    }
    else
    {
        perror("sem_open");
        return -1;
    }
}

static void
setupipc()
{
    if(0 != setupsig())
    {
        fprintf(stderr, "[manager] Could not init signals\n");
        exit(-1);
    }
    if(0 != setupsem())
    {
        fprintf(stderr, "[manager] Could not init a semaphore\n");
        exit(-1);
    }
}

static void
blockhandledsignals()
{
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGHUP);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, NULL);
}

/*static void
printcurrsigmask()
{
    sigset_t mask;
    sigprocmask(0, NULL, &mask);
    
    unsigned long long set;
    unsigned char i = 1;
    for(; i <= 64; ++i) {
        int b = sigismember(&mask, i); // b = {0, 1}
        set = (set << 1) | b;
    }

    printf("%llx\n", set);
}*/

static void
blockforsuspend(int how, sigset_t* set)
{
    static char once = 0;
    static sigset_t mask;
    static sigset_t oldmask;

    if(0 == once)
    {
        sigfillset(&mask);
        sigdelset(&mask, SIGFPE);
        sigdelset(&mask, SIGILL);
        sigdelset(&mask, SIGSEGV);
        sigdelset(&mask, SIGHUP);
        sigdelset(&mask, SIGINT);
        sigdelset(&mask, SIGCHLD);
        sigdelset(&mask, SIGTERM);
        ++once;
    }

    if(SIG_BLOCK == how)
    {
        if(-1 == sigprocmask(SIG_SETMASK, &mask, &oldmask))
            perror("sig_block");
        *set = mask;
    }
    else
    {
        if(-1 == sigprocmask(SIG_SETMASK, &oldmask, NULL))
            perror("sig_unblock");
    }
}

static int
waitserver(pid_t servpid)
{
    int status;
    sigset_t mask;

    blockforsuspend(SIG_BLOCK, &mask);
    g_nsig = 0;
    while(0 == g_nsig)
    {
        sigsuspend(&mask);
        // all signals are blocked again at this point

        int rv;
        int val;
        switch(g_nsig)
        {
            case SIGCHLD:
                if(-1 != (rv = sem_getvalue(g_sem, &val)))
                {
                    fprintf(stderr, "[manager] val == %d\n", val);
                    if(0 != val)
                    {
                        g_isRunning = 1;
                        g_doReconfiguration = 1;
                        sem_wait(g_sem); // --*sem
                    }
                    else
                    {
                        g_isRunning = 0;
                    }
                }
                else
                {
                    fprintf(stderr, "[manager] sem_getvalue returned %d\n", rv);
                }
                break;
            case SIGHUP:
                g_isRunning = 1;
                g_doReconfiguration = 1;
                kill(servpid, SIGTERM);
                break;
            case SIGTERM:
            case SIGINT:
                g_isRunning = 0;
                kill(servpid, SIGTERM);
            default:
                continue;
        }
    }
    blockforsuspend(SIG_UNBLOCK, NULL);

    waitpid(servpid, &status, 0); 
    if(WIFEXITED(status))
    {
        return WEXITSTATUS(status);
    }
    return -1;
}

static void
create_webserver()
{
    pid_t servpid = runservinproc();

    if(-1 != servpid)
    {
        printf("[manager] %d %d\n", (int) getpid(), servpid);
        waitserver(servpid);
    }
    else
    {
        perror("[manager] fork failed");
    }
}

void
daemonize()
{
    int fdlog;
    pid_t pid = fork();
    if(-1 == pid)
    {
        perror("[manager] The first fork() failed");
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
        perror("[manager] The second fork() failed");
        exit(EXIT_FAILURE);
    }
    else if(0 != pid)
    {
        _exit(EXIT_SUCCESS);
    }

    if(-1 == chdir("/"))
    {
        perror("[manager] Chdir failed");
        exit(EXIT_FAILURE);
    }

    umask(0);

    close(STDIN_FILENO);
    if(-1 == open("/dev/null",O_RDONLY))
    {
        perror("[manager] Open /dev/null failed");
        exit(EXIT_FAILURE);
    }
    fdlog = open("/tmp/webserver.log",
            O_RDWR | O_CREAT | O_APPEND, 0644);
    if(fdlog == -1)
    {
        perror("[manager] Failed to open logfile");
    }
    dup2(fdlog, STDOUT_FILENO);
    dup2(fdlog, STDERR_FILENO);
    close(fdlog);
    setvbuf(stdout, NULL, _IONBF, 0);
}

int
manage(int argc, char** argv)
{
    g_nsig = 0;
    g_isRunning = 1;
    g_doReconfiguration = 0;

    if(0 != cfgmngr(argc, argv))
    {
        fprintf(stderr, "[manager] Initialization of the server failed\n");
        return -1;
    }

    blockhandledsignals();
    setupipc();

    while(1)
    {
        if(1 == g_isRunning)
        {
            if(1 == g_doReconfiguration)
            {
                if(-1 == recfgmngr())
                {
                    fprintf(stderr, "[manager] Reconfiguration failed\n");
                    return -1;
                }
                g_doReconfiguration = 0;
            }
            create_webserver(); 
        }
        else
        {
            fprintf(stderr, "[manager] isRunning == false\n");
            sem_unlink("/webserver.sem");
            break;
        }
    }

    return 0;
}
