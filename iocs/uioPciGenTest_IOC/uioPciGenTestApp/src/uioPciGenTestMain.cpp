#include <iocsh.h>
#include <epicsThread.h>
#include <epicsExit.h>
#ifdef HAVE_CEXP
#include <cexp.h>
#endif


int
main(int argc, char *argv[])
{
#ifdef HAVE_CEXP
	cexp_main(argc, argv);
#else
    if(argc>=2) {    
        iocsh(argv[1]);
        epicsThreadSleep(.2);
    }
    iocsh(NULL);
#endif
    epicsExit(0);
    return(0);
}
