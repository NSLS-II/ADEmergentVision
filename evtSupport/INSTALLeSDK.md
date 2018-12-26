# Installing the eSDK for use with Area Detector.

### Ubuntu 18.04

In order to install the eSDK for use with the area detector driver, download the tar folder
from the Emergent Vision Technologies website. 

Place the tar in a location such as ~/Documents/EVTINSTALL.

Untar the file with
```
tar -xvzf emergent_camera_2.19.01.20601.x86_64.tgz
```
This will create 3 files in the directory:
```
EVTINSTALL
|
|--emergent_camera_2.19.01.20601.x86_64.tgz
|
|--emergent_camera.deb
|
|--install_eSdk.sh
|
|--uninstall_eSdk.sh
```
Simply run the install_eSdk.sh script, and the eSDK will be installed on your computer.

In my case it was placed in the /opt/EVT directory, on my Ubuntu 18.04 machine.

You must now move the libs and includes required by the eSDK into the appropriate locations for the EPICS build path.

To do this, open the prep_eSDK script for your OS, and edit the PATH_ESDK macro to match the path to the eSDK
on your machine. In my case this was /opt/EVT/eSDK. Then simply run
```
prep_eSDK_Linux.sh
```
if you are on linux. A script for windows machines will be added by R1-0.
 
Finally, you may have to edit the Makefile in evtSupport, if the versions of the libraries you built are different
than those used to write the driver.  
Simply edit:
``
INC += INCLUDE_FILENAME
LIB_INSTALLS_ARCH += LIBRARY_NAME
```
lines to contain the correct filenames that are provided with your eSDK download.

Once this is done, you should be ready to compile the ADEmergentVision driver.
