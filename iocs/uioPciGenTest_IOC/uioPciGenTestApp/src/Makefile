TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================
USR_INCLUDES += $(CEXP_INCLUDES)

#=============================
# Build the IOC application

PROD_IOC = uioPciGenTest
# uioPciGenTest.dbd will be created and installed
DBD += uioPciGenTest.dbd

# uioPciGenTest.dbd will be made up from these files:
uioPciGenTest_DBD += base.dbd
uioPciGenTest_DBD += epicspci.dbd
uioPciGenTest_DBD += devBusMapped.dbd
uioPciGenTest_DBD += drvUioPciGen.dbd
uioPciGenTest_DBD += $(CEXP_DBDS)

# uioPciGenTest_registerRecordDeviceDriver.cpp derives from uioPciGenTest.dbd
uioPciGenTest_SRCS += uioPciGenTest_registerRecordDeviceDriver.cpp

# Build the main IOC entry point on workstation OSs.
uioPciGenTest_SRCS_DEFAULT += uioPciGenTestMain.cpp
uioPciGenTest_SRCS_RTEMS   += -nil-
uioPciGenTest_SRCS_vxWorks += -nil-

# Add support from base/src/vxWorks if needed
#uioPciGenTest_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

# Finally link to the EPICS Base libraries
uioPciGenTest_LIBS += $(EPICS_BASE_IOC_LIBS)
uioPciGenTest_LIBS += drvUioPciGen epicspci devBusMapped
uioPciGenTest_LIBS += $(CEXP_LIBS)
uioPciGenTest_SYS_LIBS += $(CEXP_SYS_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

