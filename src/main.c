#include <manager/manager.h>

int
main(int argc, char** argv)
{
    (void) daemonize();
    return manage(argc, argv);
}
