/**
 * Main source file for the ADEmergentVision EPICS driver 
 * 
 * This file contains functions for connecting and disconnectiong from the camera,
 * for starting and stopping image acquisition, and for controlling all camera functions through
 * EPICS.
 * 
 * Author: Jakub Wlodek
 * Created On: December-7-2018
 * 
 * Copyright (c) : 2018 Brookhaven National Laboratory
 * 
 */


// Standard includes
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


// EPICS includes
#include <epicsTime.h>
#include <epicsThread.h>
#include <epicsExit.h>
#include <epicsString.h>
#include <epicsStdio.h>
#include <iocsh.h>
#include <epicsExport.h>


// Area Detector include
#include "ADEmergentVision.h"


using namespace std;
using namespace Emergent;


const char* driverName = "ADEmergentVision";


// Maximum number of cameras that can be detected at one time
#define MAX_CAMERAS     10


// Constants
static const double ONE_BILLION = 1.E9;


// -----------------------------------------------------------------------
// ADEmergentVision Utility Functions (Reporting/Logging/ExternalC)
// -----------------------------------------------------------------------


/*
 * External configuration function for ADEmergentVision.
 * Envokes the constructor to create a new ADEmergentVision object
 * This is the function that initializes the driver, and is called in the IOC startup script
 *
 * @params: all passed into constructor
 * @return: status
 */
extern "C" int ADEmergentVisionConfig(const char* portName, int maxBuffers, size_t maxMemory, int priority, int stackSize){
    new ADEmergentVision(portName, maxBuffers, maxMemory, priority, stackSize);
    return(asynSuccess);
}


/*
 * Callback function called when IOC is terminated.
 * Deletes created object
 *
 * @params: pPvt -> pointer to the ADEmergentVision object created in ADUVCConfig
 */
static void exitCallbackC(void* pPvt){
    ADEmergentVision* pEVT = (ADEmergentVision*) pPvt;
    delete(pEVT);
}


/**
 * Function that reports error encountered in vendor library from EVT
 * 
 * @params: status          -> error code
 * @params: functionName    -> function in which error occured
 * @return: void
 */
void ADEmergentVision::reportEVTError(EVT_ERROR status, const char* functionName){
    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s EVT Error: %d\n", driverName, 
        functionName, status);
}


// -----------------------------------------------------------------------
// ADEmergentVision Connect/Disconnect Functions
// -----------------------------------------------------------------------


asynStatus ADEmergentVision::connectToDeviceEVT(const char* serialNumber){
    const char* functionName = "connectToDeviceEVT()";
    unsigned int count, numCameras;
    struct GigEVisionDeviceInfo deviceList[numCameras];
    numCameras = MAX_CAMERAS;
    this->evt_status = EVT_ListDevices(pdeviceInfo, &numCameras, &count);
    if(this->evt_status != EVT_SUCCESS){
        reportEVTError(this->evt_status, functionName);
        return asynError;
    }
    else if(count == 0){
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s No Cameras detected on the network\n", driverName, functionName);
        return asynError;
    }
    else{
        int i;
        for(i = 0; i< count; i++){
            if(strcmp(deviceList[i].serialNumber, serialNumber) == 0){
                pdeviceInfo = (struct GigEVisionDeviceInfo*) malloc(sizeof(struct GigEVisionDeviceInfo));
                *pdeviceInfo = deviceList[i];
                break;
            }
        }
        if(pdeviceInfo == NULL){
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s Could not find camera with specified serial number\n", driverName, functionName);
        }
    }
}

asynStatus ADEmergentVision::disconnectFromDeviceEVT(){
    const char* functionName = "disconnectFromDeviceEVT";
    if(this->pdeviceInfo == NULL || this->pdevice == NULL){
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s Never connected to device\n", driverName, functionName);
        return asynError;
    }
    else{
        free(this->pdeviceInfo);
        EVT_CameraClose(pdevice);
    }
}

// -----------------------------------------------------------------------
// ADEmergentVision Acquisition Functions
// -----------------------------------------------------------------------



// -----------------------------------------------------------------------
// ADEmergentVision ADDriver Overrides (WriteInt32/WriteFloat64/report)
// -----------------------------------------------------------------------

// -----------------------------------------------------------------------
// ADEmergentVision Constructor/Destructor
// -----------------------------------------------------------------------



ADEmergentVision::~ADEmergentVision(){
    const char* functionName = "~ADEmergentVision";
    disconnectFromDeviceEVT();
    asynPrint(this->pasynUserSelf, ASYN_TRACEIO_DRIVER, "%s::%s ADEmergentVision driver exiting\n", driverName, functionName);
    disconnect(this->pasynUserSelf);
}

// -----------------------------------------------------------------------
// ADEmergentVision IOC Shell Registration Functions
// -----------------------------------------------------------------------



