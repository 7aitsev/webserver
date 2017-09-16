#include "config/config.h"
#include "server/server.h"

#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
}

static int
setupsig()
{
    int rv = 0;
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
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
        perror("[manager] sem_open");
        return -1;
    }
}

static void
setupipc()
{
    if(0 != setupsig())
    {
        fprintf(stderr, "[manager] Could not init signals\n");
        exit(EXIT_FAILURE);
    }
    if(0 != setupsem())
    {
        fprintf(stderr, "[manager] Could not init a semaphore\n");
        exit(EXIT_FAILURE);
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
/*
static void
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
}
*/
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
            perror("[manager] sig_block");
        *set = mask;
    }
    else
    {
        if(-1 == sigprocmask(SIG_SETMASK, &oldmask, NULL))
            perror("[manager] sig_unblock");
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
                    fprintf(stderr, 
                            "[manager] sem_getvalue returned %d\n", rv);
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
                break;
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

    daemonize();

    blockhandledsignals();
    setupipc();

    while(1)
    {
        if(1 == g_isRunning)
        {
            if(1 == g_doReconfiguration)
            {
                if(0 != recfgmngr())
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
            sem_close(g_sem);
            break;
        }
    }

    return 0;
}
