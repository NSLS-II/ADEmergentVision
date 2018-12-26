#
# Script that moves eSDK libs and includes to the appropriate locations for compiiling ADEmergent Vision
#
# This version of the script is in bash for use on linux
#
# Author: Jakub Wlodek
# Created: Dec 26, 2018
#
# Copyright (c): Brookhaven National Laboratory 2018

# set this macro to the path in which your eSDK is installed. 
# on my linux machine it was installed to /opt/EVT/eSDK
PATH_EVT="/opt/EVT/eSDK"

mkdir include
mkdir os
cd os
mkdir linux-x86_64
cd ..
cp $PATH_EVT/include/* include/.
cp $PATH_EVT/lib/* os/linux-x86_64/.