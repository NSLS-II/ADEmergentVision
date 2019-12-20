# ADEmergentVision

Area Detector driver for Emergent Vision detectors.

This driver is currently in development

### Setup

In order to connect to Emergent Vision Detectors, a myricom network card is required, and must be activated. To do so, follow the steps below:

* Download the myricom sdk from the EVT website. For me it was placed in `/opt/EVT/myri_mva-1.2.7.x86_64`
* If necessary, rebuild the driver with `sbin/rebuild.sh` from the sdk location
* Start the driver with `sbin/myri_start_stop start`.

You will most likely also need to run the start command on IOC server reboot.
