#include <grp.h>
#include <pwd.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

#include "config.h"
#include "server.h"

uid_t
get_uid_by_name(const char* uname)
{
    struct passwd* p;
    if(NULL != (p = getpwnam(uname)))
    {
        return p->pw_uid;
    }
    else
    {
        perror("getpwnam()");
        exit(EXIT_FAILURE);
    }
}

gid_t
get_gid_by_name(const char* gname)
{
    struct group* g;
    if(NULL != (g = getgrnam(gname)))
    {
        return g->gr_gid;
    }
    else
    {
        perror("getgrnam()");
        exit(EXIT_FAILURE);
    }
}

void
drop_privileges(uid_t uid, gid_t gid)
{
    if(-1 == setgid(gid))
    {
        perror("setgid()");
        exit(EXIT_FAILURE);
    }
    if(-1 == setuid(uid))
    {
        perror("setuid()");
        exit(EXIT_FAILURE);
    }
}

int
main(int argc, char** argv)
{
    if(0 == init_server(argc, argv))
    {
        pid_t child_pid;
        if(0 == (child_pid = fork()))
        {
            uid_t uid = get_uid_by_name(CONFIG.user);
            gid_t gid = get_gid_by_name(CONFIG.group);

            if(-1 == setsid())
            {
                perror("setsid()");
                return EXIT_FAILURE;
            }

            int listensocket = prepare_server();

            if(-1 == chroot(CONFIG.document_root))
            {
                perror("chroot()");
                return EXIT_FAILURE;
            }

            drop_privileges(uid, gid);

            run_server(listensocket);
        }
        else if(-1 != child_pid)
        {
            printf("WebServer has started with pid=%d\n", child_pid);
            fclose(stdin);
            fclose(stdout);
            fclose(stderr);
        }
        else
        {
            perror("fork()");
            return EXIT_FAILURE;
        }
    }
    else
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
