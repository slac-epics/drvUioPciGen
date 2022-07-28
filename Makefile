TOP = .
include $(TOP)/configure/CONFIG

DIRS += configure
DIRS += src

ifeq ($(BUILD_IOCS), YES)
DIRS := $(DIRS) $(filter-out $(DIRS), $(wildcard iocs))
iocs_DEPEND_DIRS += $(wildcard *App)
endif

include $(TOP)/configure/RULES_TOP

ifeq ($(BUILD_IOCS), YES)
uninstall: uninstall_iocs
uninstall_iocs:
	$(MAKE) -C iocs uninstall
.PHONY: uninstall uninstall_iocs

realuninstall: realuninstall_iocs
realuninstall_iocs:
	$(MAKE) -C iocs realuninstall
.PHONY: realuninstall realuninstall_iocs
endif
