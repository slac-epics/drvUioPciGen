TOP=..

include $(TOP)/configure/CONFIG

#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#USR_CFLAGS += 

#=============================

# xxxRecord.h will be created from xxxRecord.dbd
#DBDINC += xxxRecord

#=============================

LIBRARY = drvUioPciGen
PROD_IOC= uioPciGenTest
INC     = drvUioPciGen.h
# <name>.dbd will be created from <name>Include.dbd
DBD		= drvUioPciGen.dbd
DBD    += uioPciGenTest.dbd

drvUioPciGen_SRCS += drvUioPciGen.c

uioPciGenTest_DBD  = base.dbd
uioPciGenTest_DBD += epicspci.dbd
uioPciGenTest_DBD += drvUioPciGen.dbd

uioPciGenTest_SRCS += uioPciGenTest_registerRecordDeviceDriver.cpp
uioPciGenTest_SRCS_DEFAULT += uioPciGenMain.c
uioPciGenTest_SRCS_RTEMS   += -nil-
uioPciGenTest_SRCS_vxWorks += -nil-

uioPciGenTest_LIBS  = $(EPICS_BASE_IOC_LIBS)
uioPciGenTest_LIBS += drvUioPciGen epicspci devBusMapped 
uioPciGenTest_LIBS += $(CEXP_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
