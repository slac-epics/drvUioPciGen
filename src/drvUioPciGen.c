#include <stdlib.h>
#include <string.h>

#include <devLibPCI.h>
#include <devBusMapped.h>

#include <epicsTypes.h>
#include <errlog.h>
#include <epicsStdio.h>
#include <epicsExport.h>
#include <epicsMMIO.h>
#include <drvSup.h>

#include <iocsh.h>

#define epicsExportSharedSymbols
#include <drvUioPciGen.h>

#define DRVNAM "drvUioPciGen"

typedef struct DrvUioPciGenPvtRec_ {
	const epicsPCIDevice    *dev;
	int                     bar;
	DevBusMappedDev         bmd;
	struct DrvUioPciGenPvtRec_ *next;
} DrvUioPciGenPvtRec, *DrvUioPciGenPvt;

typedef struct DrvUioPciGenDevRec_ {
	const epicsPCIDevice *dev;
} DrvUioPciGenDevRec, *DrvUioPciGenDev;

typedef struct AddFnArgRec_ {
	DrvUioPciGenDev p;
	int             n;
} AddFnArgRec, *AddFnArg;

static DrvUioPciGenDev theDevs;
static int             nDevs   = -1;
static DrvUioPciGenPvt regDevs = 0;

static long init(void);

/* Count number of PCI devices */
static int
countFn(void *ptr, const epicsPCIDevice *dev)
{
	(*(int*)ptr)++;
	return 0;
}

/* Add to array */
static int
addFn(void *ptr, const epicsPCIDevice *dev)
{
AddFnArg a = ptr;

	/* number of devices should not have changed
	 * but we make no assumptions about devlib2 
	 * internals...
	 */
	if ( a->n-- <= 0 )
		return -1;
	a->p[a->n].dev = dev;
	return 0;
}

static int d2sig(const epicsPCIDevice *dev)
{
	return (dev->bus << 8) | ((dev->device & 0x1f) << 3) | (dev->function & 7);
}

static int
cmpFn(const void *a, const void *b)
{
const DrvUioPciGenDevRec * devA = a;
const DrvUioPciGenDevRec * devB = b;
int          sigA, sigB;
	sigA = d2sig(devA->dev);
	sigB = d2sig(devB->dev);
	return sigA - sigB;
}

static long
init(void)
{
epicsPCIID any_id[] = {
		DEVPCI_DEVICE_VENDOR(
				DEVPCI_ANY_DEVICE,
				DEVPCI_ANY_VENDOR),
		DEVPCI_END
	};
int          st;
AddFnArgRec  adda;

	if ( nDevs >= 0 )
		return 0;

	nDevs = 0;

	if ( (st = devPCIFindCB( any_id, countFn, &nDevs, 0 )) ) {
		nDevs = 0;
		errlogPrintf("drvUioPciGen: unable to count number of PCI devices (%i)\n", st);
		return -1;
	}
	if ( ! (theDevs = malloc( sizeof(*theDevs) * nDevs )) ) {
		nDevs = 0;
		errlogPrintf("drvUioPciGen: no memory for PCI device list\n");
		return -1;
	}
	adda.p = theDevs;
	adda.n = nDevs;
	if ( (st = devPCIFindCB( any_id, addFn, &adda, 0 )) ) {
		nDevs = 0;
		free( theDevs );
		theDevs = 0;
		errlogPrintf("drvUioPciGen: unable to count number of PCI devices (%i)\n", st);
		return -1;
	}	

	/* devlib2 lists devices in an arbitrary order (as listed in /proc/bus/pci/devices [linux]) */
	/* Let's sort */
	qsort( theDevs, nDevs, sizeof(theDevs[0]), cmpFn );
	
	return 0;
}

static long
report(int level)
{
DrvUioPciGenPvt p;
int             i;
epicsUInt32     barLen;

	errlogPrintf("drvUioPciGen: Generic driver for devBusMapped devSup on PCI MMIO devices\n");
	if ( level > 0 ) {
		errlogPrintf("Registered Devices and BARs:\n");
		errlogPrintf("Name:                         PCI:         BAR #  Mapped @   Len\n");
		for ( p=regDevs; p; p=p->next ) {
			if ( devPCIBarLen( p->dev, p->bar, &barLen ) )
				barLen=0xdeadbeef;
			errlogPrintf("%-30s0000:%02x:%02x.%1d     %1d  %p 0x%08x\n",
				p->bmd->name,
				p->dev->bus, p->dev->device, p->dev->function,
				p->bar,
				p->bmd->baseAddr,
				(unsigned)barLen);
		}
		if ( level > 1 ) {
			errlogPrintf("Devices known to devlib2:\n");
			for ( i=0; i<nDevs; i++ ) {
				errlogPrintf("0000:%02x:%02x:%1x 0x%04x 0x%04x 0x%04x 0x%04x\n",
					theDevs[i].dev->bus, theDevs[i].dev->device, theDevs[i].dev->function,
					theDevs[i].dev->id.vendor,
					theDevs[i].dev->id.device,
					theDevs[i].dev->id.sub_vendor,
					theDevs[i].dev->id.sub_device);
			}
		}
	}

	return 0;
}

volatile void *
devBusMappedExtFindBA(const char *nm)
{
DevBusMappedDev dev;
	if ( ! ( dev = devBusMappedFind( nm ) ) )
		return 0;
	return dev->baseAddr;
}

DrvUioPciGenPvt
drvUioPciGenFindPvt(const char *nm)
{
DevBusMappedDev dev;
	if ( ! ( dev = devBusMappedFind( nm ) ) )
		return 0;
	return (DrvUioPciGenPvt)dev->udata;
}

epicsShareFunc
const epicsPCIDevice *
drvUioPciGenFindDev(const char *nm)
{
DrvUioPciGenPvt pvt;

	if ( ! ( pvt = drvUioPciGenFindPvt( nm ) ) ) {
		return 0;
	}

	return pvt->dev;
}


epicsShareFunc
int
drvUioPciGenRW(const char *nm, epicsUInt32 *val_p, unsigned off, int width)
{
DevBusMappedDev dev;
DrvUioPciGenPvt pvt;
epicsUInt32     barl;
int             wr   = width < 0;
int             rval = 0;
volatile void   *addr;

	width = abs(width);

	if ( ! (dev = devBusMappedFind( nm )) || ! (pvt = dev->udata) ) {
		errlogPrintf("%s: No such device found\n", nm);
		return -1;
	}
	if ( devPCIBarLen( pvt->dev, pvt->bar, &barl ) ) {
		errlogPrintf("%s: Unable to determine region size (BAR %u)\n", nm, pvt->bar);
		return -1;
	}
	if ( off + width > barl ) {
		errlogPrintf("Offset out of range (Region %u size: %u)!\n", pvt->bar, barl);
		return -1;;
	}

	if ( 0 == width )
		width = 4; /* default */

	if ( (width-1) & off ) {
		errlogPrintf("Error: Misaligned offset!\n");
	}

	addr = dev->baseAddr + off;

	switch ( width ) {
		case 1:
			if ( wr ) {
				iowrite8( addr, (epicsUInt8)*val_p );
			} else {
				*val_p = ioread8( addr );
			}
		break;

		case 2:
			if ( wr ) {
				le_iowrite16( addr, (epicsUInt16)*val_p );
			} else {
				*val_p = le_ioread16( addr );
			}
		break;

		case 4:
			if ( wr ) {
				le_iowrite32( addr, *val_p );
			} else {
				*val_p = le_ioread32( addr );
			}
		break;
			
		default:
			errlogPrintf("Error: Unsupported width: %u (not 1,2,4)\n", width);
			rval = -1;
		break;
	}

	return rval;
}

static int
matches(epicsUInt32 v, epicsUInt32 d, epicsUInt32 sv, epicsUInt32 sd, DrvUioPciGenDev p)
{
printf("ma %x %x %x %x\n",   v, d, sv, sd);
printf("mb %x %x %x %x\n\n", p->dev->id.vendor, p->dev->id.device, p->dev->id.sub_vendor, p->dev->id.sub_device);
	return    (DEVPCI_ANY_DEVICE == d || d == p->dev->id.device)
	       && (DEVPCI_ANY_VENDOR == v || v == p->dev->id.vendor)
	       && (DEVPCI_ANY_SUBDEVICE == sd || sd == p->dev->id.sub_device)
	       && (DEVPCI_ANY_SUBVENDOR == sv || sv == p->dev->id.sub_vendor);
}

static DrvUioPciGenPvt *
posBefore( DrvUioPciGenPvt *pos_p, DrvUioPciGenDev p, unsigned bar)
{
DrvUioPciGenPvt pos;
int             a,b;

	b = (d2sig(p->dev)<<4) | (bar & 0xf);
	while ( (pos = *pos_p) ) {
		a = (d2sig(pos->dev)<<4) | (pos->bar & 0xf);
		if ( a >= b ) {
			return a == b ? 0 : pos_p;
		}
		pos_p = &pos->next;
	}
	return pos_p;
}

static int
registerDev(DrvUioPciGenDev p, epicsUInt32 bar_bitmask, const char *name_prefix, unsigned inst)
{
int              name_buf_len = strlen(name_prefix) + 10;
char            *name_buf;
volatile void   *ba;
int              bar;
DevBusMappedDev  bmd;
DrvUioPciGenPvt  pvt = 0;
int              rval = -1;
DrvUioPciGenPvt *pos_p;
unsigned         opt = 0;

	if ( ! (name_buf = malloc( name_buf_len )) ) {
		errlogPrintf(DRVNAM": no memory for string buffer\n");
		return -1;
	}

	pos_p = &regDevs;

#ifdef DEVLIB_MAP_UIOCOMPACT
	opt |= DEVLIB_MAP_UIOCOMPACT;
#endif

	for ( bar=0; bar_bitmask; bar_bitmask>>=1, bar++ ) {
		if ( ! ( 1 & bar_bitmask ) )
			continue;

		if ( devPCIToLocalAddr( p->dev, bar, &ba, opt ) ) {
			errlogPrintf(DRVNAM": devPCIToLocalAddr failed for %02x:%02x:%1x, BAR #%i\n", p->dev->bus, p->dev->device, p->dev->function, bar);
			/* skip this BAR, try next */
			continue;
		}

		if ( ! (pos_p = posBefore( pos_p, p, bar)) ) {
			errlogPrintf(DRVNAM": devPCIToLocalAddr failed for %02x:%02x:%1x, BAR #%i - already present!\n", p->dev->bus, p->dev->device, p->dev->function, bar);
			goto bail;
		}


		/* went well so far; build a name and register with devBusMapped */
		if( epicsSnprintf( name_buf, name_buf_len, "%s%u.%u", name_prefix, inst, bar ) 
			>= name_buf_len ) {
			errlogPrintf(DRVNAM": name too long (%02x:%02x:%1x, BAR #%i)\n", p->dev->bus, p->dev->device, p->dev->function, bar);
			goto bail;
		}

		if ( ! (pvt = malloc(sizeof(*pvt))) ) {
			errlogPrintf(DRVNAM": no memory for pvt struct\n");
			goto bail;
		}

		if ( ! ( bmd = devBusMappedRegister( name_buf, ba ) ) ) {
			errlogPrintf(DRVNAM": unable to register with devBusMapped (instance #%i, BAR #%i)\n", inst, bar);
			free( pvt );
			goto bail;
		}

		pvt->dev   = p->dev;
		pvt->bar   = bar;
		pvt->bmd   = bmd;
		pvt->next  = *pos_p;
		*pos_p     = pvt;

		bmd->udata = (void*)pvt;

		pos_p      = &pvt->next;
	}

	rval = 0;

bail:
	free( name_buf );
	return rval;
}


/* This routine is not thread-safe! Normal use from an initialization script... */
epicsShareFunc
int drvUioPciGenRegisterDevice(
		epicsUInt32 vendor_id,
		epicsUInt32 device_id,
		epicsUInt32 subvendor_id,
		epicsUInt32 subdevice_id,
		epicsInt32  filter,  /* zero-based */
		epicsUInt32 bar_bitmask,
		const char  *name_prefix
	)
{
int        rval = -1;
unsigned   b,d,f;
int        i,found;

	if ( nDevs < 0 )
		init();

	if ( ! name_prefix ) {
		return -1;
	}

	/* if they don't give bus/dev/fun then vendor/device or subvendor/subdevice must
	 * be given
	 */
	if (     filter >= -1
		&& ! (   (   vendor_id<=0xffff &&    device_id<=0xffff)
		      || (subvendor_id<=0xffff && subdevice_id<=0xffff) ) ) {
		return -1;
	}

	if ( vendor_id > 0xffff )
		vendor_id = DEVPCI_ANY_VENDOR;
	if ( device_id > 0xffff )
		device_id = DEVPCI_ANY_DEVICE;
	if ( subvendor_id > 0xffff )
		subvendor_id = DEVPCI_ANY_SUBVENDOR;
	if ( subdevice_id > 0xffff )
		subdevice_id = DEVPCI_ANY_SUBDEVICE;

	if ( filter < -1 ) {
		/* filter is actually a PCI signature */
		b = (filter >> 8) & 0xff;
		d = (filter >> 3) & 0x1f;
		f = (filter >> 0) & 0x07;
		for ( i = 0; i < nDevs; i++ ) {
			 if (    theDevs[i].dev->bus      == b
			      && theDevs[i].dev->device   == d
			      && theDevs[i].dev->function == f ) {
				   vendor_id = theDevs[i].dev->id.vendor;
				subvendor_id = theDevs[i].dev->id.sub_vendor;
				   device_id = theDevs[i].dev->id.device;
				subdevice_id = theDevs[i].dev->id.sub_device;
				goto found_it;
			}
		}
		/* No such device found */
		return -1;

	} else {
		b = d = f = 256;
	}

found_it:

	for ( i = 0, found = 0; i < nDevs; i++ ) {
		if (    matches(vendor_id, device_id, subvendor_id, subdevice_id, &theDevs[i]) ) {
			if ( f > 7 ? (filter < 0 || found == filter)
                       : (    theDevs[i].dev->bus      == b
			               && theDevs[i].dev->device   == d
						   && theDevs[i].dev->function == f ) ) {
				rval = registerDev( &theDevs[i], bar_bitmask, name_prefix, found );
				if ( rval || -1 != filter )
					break;
			}
			found++;
		}
	}

	return rval ? rval : found;
}


static const struct iocshArg args0[] = {
	{
	"PCI vendor id (>0xffff for 'any')",
	iocshArgInt
	},
	{
	"PCI device id (>0xffff for 'any')",
	iocshArgInt
	},
	{
	"PCI subvendor id (>0xffff for 'any')",
	iocshArgInt
	},
	{
	"PCI subdevice id (>0xffff for 'any')",
	iocshArgInt
	},
	{
	"Instance (0-based); use -1 for 'all'",
	iocshArgInt
	},
	{
	"BARs - bitmask of BARs to be mapped",
	iocshArgInt
	},
	{
	"name prefix",
	iocshArgString
	},
};

static const struct iocshArg *bloat0[] = {
	args0, args0+1, args0+2, args0+3, args0+4, args0+5, args0+6
};

static const struct iocshFuncDef fn0Desc = {
	"drvUioPciGenRegisterDevice",
	sizeof(bloat0)/sizeof(bloat0[0]),
	bloat0
};

static void
fn0Wrap(const iocshArgBuf *args)
{
	drvUioPciGenRegisterDevice(
		(epicsUInt32)args[0].ival,
		(epicsUInt32)args[1].ival,
		(epicsUInt32)args[2].ival,
		(epicsUInt32)args[3].ival,
		(epicsInt32) args[4].ival,
		(epicsUInt32)args[5].ival,
		             args[6].sval);
}

static const struct iocshArg args1[] = {
	{
	"uioPciGen device name",
	iocshArgString
	},
	{
	"offset",
	iocshArgInt
	},
	{
	"val",
	iocshArgInt
	},
};

static const struct iocshArg *bloat1[] = {
	args1, args1+1, args1+2
};

static const struct iocshFuncDef fn1Desc[] = {
	{ "drvUioPciGenRd8",  sizeof(bloat1)/sizeof(bloat1[0]) - 1, bloat1 },
	{ "drvUioPciGenRd16", sizeof(bloat1)/sizeof(bloat1[0]) - 1, bloat1 },
	{ "drvUioPciGenRd32", sizeof(bloat1)/sizeof(bloat1[0]) - 1, bloat1 },
	{ "drvUioPciGenWr8",  sizeof(bloat1)/sizeof(bloat1[0]) - 0, bloat1 },
	{ "drvUioPciGenWr16", sizeof(bloat1)/sizeof(bloat1[0]) - 0, bloat1 },
	{ "drvUioPciGenWr32", sizeof(bloat1)/sizeof(bloat1[0]) - 0, bloat1 },
};

#define FN1WRAP(idx,wid) \
	static void fn1Wrap##idx(const iocshArgBuf *args) \
	{ \
	epicsUInt32 val;	\
		if ( wid < 0 )  \
			val = args[2].ival; \
		if ( 0 == drvUioPciGenRW( args[0].sval, &val, args[1].ival, wid) && wid >= 0 ) { \
			errlogPrintf("0x%08x (%u)\n", val, val); \
		} \
	}

FN1WRAP(0, 1)
FN1WRAP(1, 2)
FN1WRAP(2, 4)
FN1WRAP(3,-1)
FN1WRAP(4,-2)
FN1WRAP(5,-4)


static void
drvUioPciGenRegistrar(void)
{
	iocshRegister( &fn0Desc, fn0Wrap );
	iocshRegister( &fn1Desc[0], fn1Wrap0 );
	iocshRegister( &fn1Desc[1], fn1Wrap1 );
	iocshRegister( &fn1Desc[2], fn1Wrap2 );
	iocshRegister( &fn1Desc[3], fn1Wrap3 );
	iocshRegister( &fn1Desc[4], fn1Wrap4 );
	iocshRegister( &fn1Desc[5], fn1Wrap5 );
}

static drvet drvUioPciGen = {
	number: 2,
	report: report,
	init:   init
};

epicsExportRegistrar(drvUioPciGenRegistrar);
epicsExportAddress(drvet, drvUioPciGen);
