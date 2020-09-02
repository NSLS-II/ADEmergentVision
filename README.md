# ADEmergentVision

Area Detector driver for Emergent Vision detectors.

This driver is currently in development

### Pre-Installation Setup

In order to connect to Emergent Vision Detectors, a myricom network card is required, and must be activated. To do so, follow the steps below:

First, download the Emergent Vision SDK. This should include eCapture, a vendor provided viewer, eSDK, the libraries used to connect
to EmergentVision devices by the areaDetector driver, and finally the myricom network driver. On linux, this driver was placed in `/opt/EVT/myri_mva-1.2.7.x86_64`.
On windows it will likely be placed under `C:/Program Files/EVT/myri_mva-1.2.7.x86_64` or something similar. Then, it is required to build the
driver:

```
cd /opt/EVT/myri_mva-1.2.7.x86_64/sbin
./rebuild.sh
```

This step is required for every time the linux kernel on the host server is updated. Next start the network card driver

```
cd /opt/EVT/myri_mva-1.2.7.x86_64/sbin
./myri_start_stop start
```

You will most likely also need to run the start command on IOC server reboot. You should now be able to see your EVT camera in the vendor provided
eCapture software, and assign it an I.P. address from here. This will be required for connecting to it via an IOC.

### Installation

To install the `ADEmergentVision` areaDetector driver, first make sure to have completed the setup described above. Next,
clone this repostiory to your EPICS installation's areaDetector folder. From here, enter the `evtSupport` directory, and
modify the appropriate `prep` script to point to your SDK installation location. Then run it:

```
./prep_eSDK_Linux.sh
```

Next, navigate back to the top level directory and build with `make`. On windows the resulting executable is statically built,
meaning that it doesn't require setting the library path. On Linux, the vendor provides only shared libraries, meaning they
will need to be installed to your system path to function.
