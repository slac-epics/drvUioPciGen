#include <stdlib.h>
#include <string.h>

#include <devLibPCI.h>
#include <devBusMapped.h>

#include <epicsTypes.h>
#include <errlog.h>
#include <epicsStdio.h>
#include <epicsExport.h>

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

static int
searchFn(void *ptr, const epicsPCIDevice *dev)
{
SearchArg       *arg=ptr;
volatile void   *ba;
int              bar;
DevBusMappedDev  bmd;
epicsUInt32      bar_bitmask = arg->bar_bitmask;

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

		if ( ! ( bmd = devBusMappedRegister( arg->name_buf, ba ) ) ) {
			errlogPrintf(DRVNAM": unable to register with devBusMapped (instance #%i, BAR #%i)\n", arg->count, bar);
			continue;
		}

		bmd->udata = (void*)dev;
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

epicsShareFunc
epicsPCIDevice *
drvUioPciGenFindDev(const char *nm)
{
DevBusMappedDev dev;
	if ( ! ( dev = devBusMappedFind( nm ) ) )
		return 0;
	return dev->udata;
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

static const struct iocshArg args[] = {
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

static const struct iocshArg *bloat[] = {
	args, args+1, args+2, args+3, args+4, args+5, args+6
};

static const struct iocshFuncDef fn0Desc = {
	"drvUioPciGenRegisterDevice",
	sizeof(bloat)/sizeof(bloat[0]),
	bloat
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

static void
drvUioPciGenRegistrar(void)
{
	iocshRegister( &fn0Desc, fn0Wrap );
}

epicsExportRegistrar(drvUioPciGenRegistrar);
