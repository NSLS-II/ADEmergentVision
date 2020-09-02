# ADEmergentVision Releases

This driver is in development and currently only has beta releases.

<!--RELEASE START-->

### R0-3

This release adds higher bit depth support as well as
improved performance

* Key Features Implemented
    * High bit depth (10/12 bit) supported
    * Improve performances - honor driver framerate setting
    * Improved edge case error handling

### R0-2

This release adds support for the driver on windows

* Key Features Implemented
    * Windows support
    * Improved driver handling of invalid camera parameter settings
* Known Issues
    * Logging inconsistencies
    * Performance downgrade over vendor software
    * Limited to 8 bit

### R0-1

Initial beta release of ADEmergentVision

* Key Features Implemented:
    * Image acquisition
    * Exposure time control (in ms)
    * Example IOC (used with HR-5000S camera)
    * Vendor support sdk installation instructions
    * OPI screen
* Known Issues
    * Not all camera parameters are editable
    * Camera sometimes needs to be unplugged and replugged on IOC server reboot (Seems to be a firmware issue)
    * Changing image gives errors.
* Future plans
    * Camera parameters will be better implemented
    * Improve readability by removing redundant code
