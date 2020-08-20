TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

ifeq (windows-x64, $(findstring windows-x64, $(T_A)))
USR_CPPFLAGS += -D__X64
endif

ifeq (linux-x86_64, $(findstring linux-x86_64, $(T_A)))
USR_CPPFLAGS += -std=c++11
endif

#USR_INCLUDES += -I$(EMERGENT_HOME)/eSDK/include/

LIBRARY_IOC_WIN32 += emergent
LIBRARY_IOC_Linux += emergent

LIB_SRCS += ADEmergentVision.cpp

#LIB_LIBS += EmergentCameraC
LIB_LIBS += EmergentCamera
LIB_LIBS += EmergentGenICam
LIB_LIBS += EmergentGigEVision

#LIB_LIBS += vma

#SYS_PROD_LIBS += boost_system

#PROD_LIBS += EmergentCameraC
#PROD_LIBS += EmergentCamera
#PROD_LIBS += EmergentGenICam
#PROD_LIBS += EmergentGigEVision
#PROD_LIBS += vma


DBD += evtSupport.dbd

include $(ADCORE)/ADApp/commonLibraryMakefile

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
