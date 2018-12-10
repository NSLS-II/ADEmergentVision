TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

ifeq (windows-x64, $(findstring windows-x64, $(T_A)))
USR_CPPFLAGS += -D__X64
endif

USR_CPPFLAGS += -std=c++11

#USR_INCLUDES += -I$(EMERGENT_HOME)/eSDK/include/

LIBRARY_IOC = adEVT
LIB_SRCS += ADEmergentVision.cpp
LIB_LIBS += EmergentCamera
LIB_LIBS += EmergentGenICam
LIB_LIBS += EmergentGigEVision

#SYS_PROD_LIBS += boost_system

DBD += evtSupport.dbd

include $(ADCORE)/ADApp/commonLibraryMakefile

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
