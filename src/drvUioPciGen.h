#ifndef DRV_UIO_PCI_GEN_H
#define DRV_UIO_PCI_GEN_H
/* $Id: drvUioPciGen.h,v 1.3 2012/06/27 22:12:59 strauman Exp $ */

/* EPICS driver to layer between (patched) UIO pci generic driver and devBusMapped */

#ifdef __cplusplus
extern "C" {
#endif

#include <epicsTypes.h>
#include <shareLib.h>
#include <devLibPCI.h>

#define DRVUIOPCIGEN_ANY_ID ((epicsInt32)-1)
#define DRVUIOPCIGEN_PCISIG(b,d,f) ( (1<<31) | ((b)<<8) | (((d)&0x1f)<<3) | ((f)&7) )

/* Register a given PCI device/instance with devBusMapped.
 * 
 * This function must be called *before* drivers are initialized
 * (iocInit).
 *
 * Each BAR with 'its' bit ( 1<<bar_no ) set in 'bar_bitmask'
 * is registered with devBusMapped under the name 
 *
 *   <name_prefix><instance>.<bar_no>.
 *
 * E.g., if bar_bitmask == 5, instance == 2 and name_prefix == "mydev"
 * then this routine registers
 * 
 *   mydev2.0
 *   mydev2.2
 * 
 * with devBusMapped.
 *
 * RETURNS: Number of devices found or a negative number if an error 
 *          occurred.
 *
 * NOTE:    This routine is not thread-safe! Normal use from an initialization script...
 */
epicsShareFunc
int drvUioPciGenRegisterDevice(
		epicsUInt32 vendor_id,
		epicsUInt32 device_id,
		epicsUInt32 subvendor_id,
		epicsUInt32 subdevice_id,
		epicsInt32  filter,  /* zero-based */
		epicsUInt32 bar_bitmask,
		const char  *name_prefix
	);

/*
 * Look a name up in the devBusMapped registry and return
 * the corresponding epicsPCIDevice or NULL if none is found.
 */
epicsShareFunc
const epicsPCIDevice *
drvUioPciGenFindDev(const char *name);

/* Write to or read from register offset 'off' of device 'nm'
 *
 * 'width' may be -4, -2, -1, 1, 2 or 4; the sign indicates
 * direction of the data transfer (<0 -> write to device).
 *
 * RETURNS: 0 on success nonzero on error.
 *
 * NOTE: Access/offset must be aligned to 'width'.
 */
epicsShareFunc
int
drvUioPciGenRW(const char *nm, epicsUInt32 *val_p, unsigned off, int width);

#ifdef __cplusplus
}
#endif

#endif
