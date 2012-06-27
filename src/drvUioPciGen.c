#include <stdlib.h>
#include <string.h>

#include <devLibPCI.h>
#include <devBusMapped.h>

#include <epicsTypes.h>
#include <errlog.h>
#include <epicsStdio.h>
#include <epicsExport.h>
#include <epicsMMIO.h>

#include <iocsh.h>

#define epicsExportSharedSymbols
#include <drvUioPciGen.h>

#define DRVNAM "drvUioPciGen"

typedef struct SearchArg_ {
	int         instance;
	int         count;
	epicsUInt32 bar_bitmask;
	const char  *name_prefix;	
	char        *name_buf;
	int         name_buf_len;
} SearchArg;

typedef struct DrvUioPciGenPvt_ {
	const epicsPCIDevice *dev;
	int                   bar;
} DrvUioPciGenPvt;

static int
searchFn(void *ptr, const epicsPCIDevice *dev)
{
SearchArg       *arg=ptr;
volatile void   *ba;
int              bar;
DevBusMappedDev  bmd;
epicsUInt32      bar_bitmask = arg->bar_bitmask;
DrvUioPciGenPvt *pvt = 0;

	if ( 0 < arg->instance && arg->count != arg->instance ) {
		arg->count++;
		return 0;
	}

	for ( bar=0; bar_bitmask; bar_bitmask>>=1, bar++ ) {
		if ( ! ( 1 & bar_bitmask ) )
			continue;

		if ( devPCIToLocalAddr( dev, bar, &ba, 0 ) ) {
			errlogPrintf(DRVNAM": devPCIToLocalAddr failed for instance #%i, BAR #%i\n", arg->count, bar);
			/* skip this BAR, try next */
			continue;
		}

		/* went well so far; build a name and register with devBusMapped */
		if( epicsSnprintf( arg->name_buf, arg->name_buf_len, "%s%u.%u", arg->name_prefix, arg->count, bar ) 
			>= arg->name_buf_len ) {
			errlogPrintf(DRVNAM": name too long (instance #%i, BAR #%i)\n", arg->count, bar);
			return -1;
		}

		if ( ! (pvt = malloc(sizeof(*pvt))) ) {
			errlogPrintf(DRVNAM": no memory for pvt struct\n");
			return -1;
		}

		if ( ! ( bmd = devBusMappedRegister( arg->name_buf, ba ) ) ) {
			errlogPrintf(DRVNAM": unable to register with devBusMapped (instance #%i, BAR #%i)\n", arg->count, bar);
			free( pvt );
			continue;
		}

		pvt->dev   = dev;
		pvt->bar   = bar;

		bmd->udata = (void*)pvt;
	}

	arg->count++;

	return 0 <= arg->instance ? 1 : 0;
}

volatile void *
devBusMappedExtFindBA(const char *nm)
{
DevBusMappedDev dev;
	if ( ! ( dev = devBusMappedFind( nm ) ) )
		return 0;
	return dev->baseAddr;
}

DrvUioPciGenPvt *
drvUioPciGenFindPvt(const char *nm)
{
DevBusMappedDev dev;
	if ( ! ( dev = devBusMappedFind( nm ) ) )
		return 0;
	return (DrvUioPciGenPvt*)dev->udata;
}

epicsShareFunc
const epicsPCIDevice *
drvUioPciGenFindDev(const char *nm)
{
DrvUioPciGenPvt *pvt;

	if ( ! ( pvt = drvUioPciGenFindPvt( nm ) ) ) {
		return 0;
	}

	return pvt->dev;
}


epicsShareFunc
int
drvUioPciGenRW(const char *nm, epicsUInt32 *val_p, unsigned off, unsigned width)
{
DevBusMappedDev  dev;
DrvUioPciGenPvt *pvt;
epicsUInt32     barl;
int             wr   = width < 0;
int             rval = 0;

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

	switch ( width ) {
		case 1:
			if ( wr ) {
				iowrite8( dev->baseAddr, (epicsUInt8)*val_p );
			} else {
				*val_p = ioread8( dev->baseAddr );
			}
		break;

		case 2:
			if ( wr ) {
				le_iowrite16( dev->baseAddr, (epicsUInt16)*val_p );
			} else {
				*val_p = le_ioread16( dev->baseAddr );
			}
		break;

		case 4:
			if ( wr ) {
				le_iowrite32( dev->baseAddr, *val_p );
			} else {
				*val_p = le_ioread32( dev->baseAddr );
			}
		break;
			
		default:
			errlogPrintf("Error: Unsupported width: %u (not 1,2,4)\n", width);
			rval = -1;
		break;
	}

	return rval;
}

epicsShareFunc
int drvUioPciGenRegisterDevice(
		epicsUInt32 vendor_id,
		epicsUInt32 device_id,
		epicsUInt32 subvendor_id,
		epicsUInt32 subdevice_id,
		int         instance,  /* zero-based */
		epicsUInt32 bar_bitmask,
		const char  *name_prefix
	)
{
int        rval;
SearchArg  sarg;

	if ( ! name_prefix )
		return -1;

	if ( vendor_id > 0xffff )
		vendor_id = DEVPCI_ANY_VENDOR;
	if ( device_id > 0xffff )
		device_id = DEVPCI_ANY_DEVICE;
	if ( subvendor_id > 0xffff )
		subvendor_id = DEVPCI_ANY_SUBVENDOR;
	if ( subdevice_id > 0xffff )
		subdevice_id = DEVPCI_ANY_SUBDEVICE;

	sarg.instance     = instance;
	sarg.count        = 0;
	sarg.bar_bitmask  = bar_bitmask;
	sarg.name_prefix  = name_prefix;
	sarg.name_buf_len = strlen( name_prefix ) + 10 ;

	if ( ! (sarg.name_buf = malloc( sarg.name_buf_len )) ) {
		errlogPrintf(DRVNAM": no memory for string buffer\n");
		return -1;
	}

	/* Need to initialize the_ids in it's own block - otherwise we can't use the macro */
	{
	epicsPCIID the_ids[] = {
		                     DEVPCI_SUBDEVICE_SUBVENDOR(
		                     device_id,
		                     vendor_id,
		                     subdevice_id,
		                     subvendor_id),
		                     DEVPCI_END
		                   };
	rval = devPCIFindCB( the_ids, searchFn, &sarg, 0 );
	}

	free( sarg.name_buf );

	/* If no error occurred then compute and return the number of devices found */
	if ( rval >= 0 ) {
		rval = ( instance >=0 && sarg.count > 0 ) ? 1 : sarg.count;
	}

	return rval;
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
		(int)        args[4].ival,
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
	args1, args1+1
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
		if ( 0 == drvUioPciGenRW( args[0].sval, &val, args[1].ival, 1) && wid >= 0 ) { \
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

epicsExportRegistrar(drvUioPciGenRegistrar);
