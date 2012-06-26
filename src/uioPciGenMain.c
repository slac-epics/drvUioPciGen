#include <iocsh.h>
#include <epicsThread.h>
#include <epicsExit.h>
#ifdef HAVE_CEXP
#include <cexp.h>
#endif


int
main(int argc, const char *argv[])
{
#ifdef HAVE_CEXP
	cexp_main(argc,argv);
#else
    if(argc>=2) {    
        iocsh(argv[1]);
        epicsThreadSleep(.2);
    }
#endif
    iocsh(NULL);
    epicsExit(0);
    return(0);
}
