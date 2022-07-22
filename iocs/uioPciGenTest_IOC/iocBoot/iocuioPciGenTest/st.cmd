#!../../bin/rhel6-x86_64/uioPciGenTest

. envPaths

## Register all support components
dbLoadDatabase("../../dbd/uioPciGenTest.dbd",0,0)
uioPciGenTest_registerRecordDeviceDriver(pdbbase)
drvUioPciGenRegisterDevice(0x1022,0x2000,0x10000,0x10000,0,2,"amd")

iocInit()

## Start any sequence programs
#seq sncuioPciGenTest,"user=egumtow"
