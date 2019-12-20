TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

PROD_NAME = evtApp
PROD_IOC += $(PROD_NAME)

# <name>.dbd will be created from <name>Include.dbd
DBD += $(PROD_NAME).dbd
$(PROD_NAME)_DBD += evtSupport.dbd

# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
$(PROD_NAME)_SRCS += $(PROD_NAME)_registerRecordDeviceDriver.cpp $(PROD_NAME)Main.cpp

# Add locally compiled object code
$(PROD_NAME)_LIBS += adEVT

$(PROD_NAME)_SYS_LIBS += 


PROD_SYS_LIBS_Linux += EmergentCameraC
PROD_SYS_LIBS_Linux += EmergentCamera
PROD_SYS_LIBS_Linux += EmergentGenICam
PROD_SYS_LIBS_Linux += EmergentGigEVision
#PROD_SYS_LIBS_Linux += vma

#LIB_LIBS_Linux += boost_system-mt
include $(ADCORE)/ADApp/commonDriverMakefile

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
