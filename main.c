#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "server.h"

int
main(int argc, char** argv)
{
    if(0 == init_server(argc, argv))
    {
        int listensocket = prepare_server();
        pid_t child_pid;
        if(0 == (child_pid = fork()))
        {
            if(-1 == chroot(CONFIG.document_root))
            {
                perror("chroot()");
                return EXIT_FAILURE;
            }
            run_server(listensocket);
        }
        else if(-1 != child_pid)
        {
            printf("WebServer has started with pid=%d\n", child_pid);
            fclose(stdin);
            fclose(stdout);
            fclose(stderr);
            close(listensocket);
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
