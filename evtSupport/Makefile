TOP=..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

# The files in this directory are installed first because they are
# used both by the NDPluginColorConvert and by the Prosilica driver
INC += include/*

# Note, the following files were manually copied from the Prosilica GigESDK/bin-pc directory
# to this directory after building


ifeq (linux-x86_64, $(findstring linux-x86_64, $(T_A)))
LIB_INSTALLS_Linux += 
endif

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
