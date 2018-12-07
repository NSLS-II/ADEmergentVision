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
Simply run the install_eSdk.sh script, and the eSDK will be installed on your computer in the /opt/EVT directory. You must now move the libs and includes required by the eSDK into the appropriate locations for the EPICS build path.