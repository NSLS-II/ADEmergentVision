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
extern "C" int ADEmergentVisionConfig(const char* portName, const char* serialNumber, int maxBuffers, size_t maxMemory, int priority, int stackSize){
    new ADEmergentVision(portName, serialNumber, maxBuffers, maxMemory, priority, stackSize);
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


/**
 * Simple function that prints all gigeVision information about a connected camera
 * 
 * @return: void
 */
void ADEmergentVision::printConnectedDeviceInfo(){
    printf("--------------------------------------\n");
    printf("Connected to EVT device\n");
    printf("--------------------------------------\n");
    printf("Specification: %d.%d\n", this->pdeviceInfo->specVersionMajor, this->pdeviceInfo->specVersionMinor);
    printf("Device mode: %d, Device Version: %s\n", this->pdeviceInfo->deviceMode, this->pdeviceInfo->deviceVersion);
    printf("ManufacturerName: %s, Model name %s\n", this->pdeviceInfo->manufacturerName, this->pdeviceInfo->modelName);
    printf("IP: %s, Mask %s\n",this->pdeviceInfo->currentIp, this->pdeviceInfo->currentSubnetMask);
    printf("MAC address: %s\n", this->pdeviceInfo->macAddress);
    printf("Serial: %s, User Name: %s\n", this->pdeviceInfo->serialNumber, this->pdeviceInfo->userDefinedName);
    printf("Manufacturer Specific Information: %s\n", this->pdeviceInfo->manufacturerSpecifiedInfo);
}


// -----------------------------------------------------------------------
// ADEmergentVision Connect/Disconnect Functions
// -----------------------------------------------------------------------


/**
 * Function that is used to initialize and connect to the EVT camera device.
 * First, it computes a list of all gige vision devices connected to the network.
 * It searches through these devices and searches for the one with the serial number 
 * passed to the function. Then, the camera open function is called to initalize the
 * camera object itself.
 * 
 * @params: serialNumber    -> serial number of camera to connect to. Passed through IOC shell
 * @return: status          -> success if connected, error if not connected
 */
asynStatus ADEmergentVision::connectToDeviceEVT(const char* serialNumber){
    const char* functionName = "connectToDeviceEVT()";
    unsigned int count, numCameras;
    numCameras = MAX_CAMERAS;
    struct GigEVisionDeviceInfo deviceList[numCameras];
    this->evt_status = EVT_ListDevices(deviceList, &numCameras, &count);
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
            return asynError;
        }
        else{
            printConnectedDeviceInfo();
            this->evt_status = EVT_CameraOpen(this->pcamera, this->pdeviceInfo);
            if(this->evt_status != EVT_SUCCESS){
                reportEVTError(this->evt_status, functionName);
                return asynError;
            }
            return asynSuccess;
        }
    }
}


/**
 * Function that disconnects from any connected EVT device
 * First checks if is connected, then if it is, it frees the memory
 * for the info and the camera
 * 
 * @return: status  -> success if freed, error if never connected
 */
asynStatus ADEmergentVision::disconnectFromDeviceEVT(){
    const char* functionName = "disconnectFromDeviceEVT";
    if(this->pdeviceInfo == NULL || this->pcamera == NULL){
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s Never connected to device\n", driverName, functionName);
        return asynError;
    }
    else{
        free(this->pdeviceInfo);
        EVT_CameraClose(pcamera);
        return asynSuccess;
    }
}


/**
 * Function that updates PV values with camera information
 * 
 * @return: status
 */
asynStatus ADEmergentVision::collectCameraInformation(){
    const char* functionName = "collectCameraInformation";
    asynStatus status = asynSuccess;
    setStringParam(ADManufacturer, this->pdeviceInfo->manufacturerName);
    setStringParam(ADSerialNumber, this->pdeviceInfo->serialNumber);
    setStringParam(ADFirmwareVersion,this->pdeviceInfo->deviceVersion);
    setStringParam(ADModel, this->pdeviceInfo->modelName);
    return status;
}


// -----------------------------------------------------------------------
// ADEmergentVision Acquisition Functions
// -----------------------------------------------------------------------


/**
 * Function that gets frame format information from a captured frame from the camera
 * 
 * TODO: This function needs to be reworked
 * 
 * @params: frame       -> pointer to currently acquired frame
 * @params: dataType    -> pointer to output data type
 * @params: colorMode   -> pointer to output color mode
 * @return: status    
 */
asynStatus ADEmergentVision::getFrameFormatND(CEmergentFrame* frame, NDDataType_t* dataType, NDColorMode_t* colorMode){
    const char* functionName = "getFrameFormatND";
    asynStatus status = asynSuccess;
    unsigned int evtDepth = frame->GetPixBitDepth();
    switch(evtDepth){
        case PIX_BIT_DEPTH_8:
            *dataType = NDUInt8;
            break;
        case PIX_BIT_DEPTH_16:
            *dataType = NDUInt16;
            break;
        case PIX_BIT_DEPTH_MASK:
        case PIX_BIT_DEPTH_SHIFT:
        default:
            //not a supported depth
            *dataType = NDUInt8;
            break;
    }
    //currently only mono images supported
    *colorMode = NDColorModeMono;
    return status;
}


/**
 * Function that allocates space for a new NDArray and copies the data from the captured EVT frame
 * 
 * NDArray dimensions depend on the color mode and data type.
 * 
 * 
 */
asynStatus ADEmergentVision::evtFrame2NDArray(CEmergentFrame* frame, NDArray* pArray){
    const char* functionName = "evtFrame2NDArray";
    asynStatus status = asynSuccess;
    int ndims;
    NDDataType_t dataType;
    NDColorMode_t colorMode;
    NDArrayInfo arrayInfo;
    int xsize;
    int ysize;
    status = getFrameFormatND(frame, &dataType, &colorMode);
    if(status = asynError){
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s Error computing dType and color mode\n", driverName, functionName);
        return asynError;
    }
    else{
        xsize = frame->size_x;
        ysize = frame->size_y;
        if(colorMode == NDColorModeMono) ndims = 2;
        else ndims = 3;

        size_t dims[ndims];
        if(ndims == 2){
            dims[0] = xsize;
            dims[1] = ysize;
        }
        else{
            dims[0] = 3;
            dims[1] = xsize;
            dims[2] = ysize;
        }

        this->pArrays[0] = pNDArrayPool->alloc(ndims, dims, dataType, 0, NULL);
        if(this->pArrays[0]!=NULL) pArray = this->pArrays[0];
        else{
            this->pArrays[0]->release();
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s Unable to allocate array\n", driverName, functionName);
	        return asynError;
        }    
        pArray->getInfo(&arrayInfo);
        size_t total_size = arrayInfo.totalBytes;
        memcpy((unsigned char*)pArray->pData, frame->imagePtr, total_size);
        pArray->pAttributeList->add("ColorMode", "Color Mode", NDAttrInt32, &colorMode);
        getAttributes(pArray->pAttributeList);
        return asynSuccess;
    }
}



// -----------------------------------------------------------------------
// ADEmergentVision ADDriver Overrides (WriteInt32/WriteFloat64/report)
// -----------------------------------------------------------------------

// -----------------------------------------------------------------------
// ADEmergentVision Constructor/Destructor
// -----------------------------------------------------------------------


ADEmergentVision::ADEmergentVision(const char* portName, const char* serialNumber, int maxBuffers, size_t maxMemory, int priority, int stackSize)
    : ADDriver(portName, 1, (int)NUM_EVT_PARAMS, maxBuffers, maxMemory, asynEnumMask, asynEnumMask, ASYN_CANBLOCK, 1, priority, stackSize){
    
    asynStatus status;

    const char* functionName = "ADEmergentVision";
    char evtVersionString[25];
    epicsSnprintf(evtVersionString, sizeof(evtVersionString), "%s", EVT_SDKVersion);
    setStringParam(ADSDKVersion, evtVersionString);

    char versionString[25];
    epicsSnprintf(versionString, sizeof(versionString), "%d.%d.%d", ADEMERGENTVISION_VERSION, ADEMERGENTVISION_REVISION, ADEMERGENTVISION_MODIFICATION);
    setStringParam(NDDriverVersion, versionString);

    if(strlen(serialNumber) == 0){
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s Error: invalid serial number passed\n", driverName, functionName);
        status = asynError;
    }
    else{
        status = connectToDeviceEVT(serialNumber);
    }
    if(status == asynSuccess) collectCameraInformation();
    
    // Create PV Parameters
    createParam(ADEVT_PixelFormatString,        asynParamInt32,     &ADEVT_PixelFormat);
    createParam(ADEVT_FramerateString,          asynParamInt32,     &ADEVT_Framerate);
    createParam(ADEVT_OffsetXString,            asynParamInt32,     &ADEVT_OffsetX);
    createParam(ADEVT_OffsetYString,            asynParamInt32,     &ADEVT_OffsetY);
    createParam(ADEVT_BufferModeString,         asynParamInt32,     &ADEVT_BufferMode);
    createParam(ADEVT_BufferNumString,          asynParamInt32,     &ADEVT_BufferNum);
    createParam(ADEVT_PacketSizeString,         asynParamInt32,     &ADEVT_PacketSize);
    createParam(ADEVT_LUTEnableString,          asynParamInt32,     &ADEVT_LUTEnable);
    createParam(ADEVT_AutoGainString,           asynParamInt32,     &ADEVT_AutoGain);

    epicsAtExit(exitCallbackC, this);
}


/* ADEmergentVision Destructor */
ADEmergentVision::~ADEmergentVision(){
    const char* functionName = "~ADEmergentVision";
    disconnectFromDeviceEVT();
    asynPrint(this->pasynUserSelf, ASYN_TRACEIO_DRIVER, "%s::%s ADEmergentVision driver exiting\n", driverName, functionName);
    disconnect(this->pasynUserSelf);
}

// -----------------------------------------------------------------------
// ADEmergentVision IOC Shell Registration Functions
// -----------------------------------------------------------------------



