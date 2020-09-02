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


// Error message formatters
#define ERR(msg) asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s: %s\n", \
    driverName, functionName, msg)

#define ERR_ARGS(fmt,...) asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, \
    "%s::%s: " fmt "\n", driverName, functionName, __VA_ARGS__);

// Flow message formatters
#define LOG(msg) asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, "%s::%s: %s\n", \
    driverName, functionName, msg)

#define LOG_ARGS(fmt,...) asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, \
    "%s::%s: " fmt "\n", driverName, functionName, __VA_ARGS__);


// Area Detector include
#include "ADEmergentVision.h"


using namespace std;
using namespace Emergent;


const char* driverName = "ADEmergentVision";


// Maximum number of cameras that can be detected at one time
#define MAX_CAMERAS     10
// We want 1 frame at a time
#define NUM_FRAMES      1


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
 * @params[in]: all passed into constructor
 * @return:     status
 */
extern "C" int ADEmergentVisionConfig(const char* portName, const char* serialNumber, int maxBuffers, size_t maxMemory, int priority, int stackSize){
    new ADEmergentVision(portName, serialNumber, maxBuffers, maxMemory, priority, stackSize);
    return(asynSuccess);
}


/*
 * Callback function called when IOC is terminated.
 * Deletes created object
 *
 * @params[in]: pPvt -> pointer to the ADEmergentVision object created in ADEmergentVisionConfig
 * @return:     void
 */
void ADEmergentVision::exitCallback(void* pPvt){
    ADEmergentVision* pEVT = (ADEmergentVision*) pPvt;
    if (pEVT) delete pEVT;
}


/**
 * Function that writes to ADStatus PV
 *
 * @params[in]: status -> message to write to ADStatus PV
 */
void ADEmergentVision::updateStatus(const char* status) {
    if (strlen(status) >= 25) return;

    char statusMessage[25];
    epicsSnprintf(statusMessage, sizeof(statusMessage), "%s", status);
    setStringParam(ADStatusMessage, statusMessage);
    callParamCallbacks();
}


/**
 * Function that reports error encountered in vendor library from EVT
 * It also prints the error status enum name to the status PV
 * 
 * @params[in]: status          -> error code
 * @params[in]: functionName    -> function in which error occured
 * @return: void
 */
void ADEmergentVision::reportEVTError(EVT_ERROR status, const char* functionName){

    string statusStr = "";

    switch (status) {
        case EVT_SUCCESS:
            statusStr = "EVT_SUCCESS";
            break;
        case EVT_ENOENT:
            statusStr = "EVT_ENOENT";
            break;
        case EVT_ERROR_SRCH:
            statusStr = "EVT_ERROR_SRCH";
            break;
        case EVT_ERROR_INTR:
            statusStr = "EVT_ERROR_INTR";
            break;
        case EVT_ERROR_IO:
            statusStr = "EVT_ERROR_IO";
            break;
        case EVT_ERROR_ECHILD:
            statusStr = "EVT_ERROR_ECHILD";
            break;
        case EVT_ERROR_AGAIN:
            statusStr = "EVT_ERROR_AGAIN";
            break;
        case EVT_ERROR_NOMEM:
            statusStr = "EVT_ERROR_NOMEM";
            break;
        case EVT_ERROR_INVAL:
            statusStr = "EVT_ERROR_INVAL";
            break;
        case EVT_ERROR_NOBUFS:
            statusStr = "EVT_ERROR_NOBUFS";
            break;
        case EVT_ERROR_NOT_SUPPORTED:
            statusStr = "EVT_ERROR_NOT_SUPPORTED";
            break;
        case EVT_ERROR_DEVICE_CONNECTED_ALRD:
            statusStr = "EVT_ERROR_DEVICE_CONNECTED_ALRD";
            break;
        case EVT_ERROR_DEVICE_NOT_CONNECTED:
            statusStr = "EVT_ERROR_DEVICE_NOT_CONNECTED";
            break;
        case EVT_ERROR_DEVICE_LOST_CONNECTION:
            statusStr = "EVT_ERROR_DEVICE_LOST_CONNECTION";
            break;
        case EVT_ERROR_GENICAM_ERROR:
            statusStr = "EVT_ERROR_GENICAM_ERROR";
            break;
        case EVT_ERROR_GENICAM_NOT_MATCH:
            statusStr = "EVT_ERROR_GENICAM_NOT_MATCH";
            break;
        case EVT_ERROR_GENICAM_OUT_OF_RANGE:
            statusStr = "EVT_ERROR_GENICAM_OUT_OF_RANGE";
            break;
        case EVT_ERROR_SOCK:
            statusStr = "EVT_ERROR_SOCK";
            break;
        case EVT_ERROR_GVCP_ACK:
            statusStr = "EVT_ERROR_GVCP_ACK";
            break;
        case EVT_ERROR_GVSP_DATA_CORRUPT:
            statusStr = "EVT_ERROR_GVSP_DATA_CORRUPT";
            break;
        case EVT_ERROR_OS_OBTAIN_ADAPTER:
            statusStr = "EVT_ERROR_OS_OBTAIN_ADAPTER";
            break;
        case EVT_ERROR_SDK:
            statusStr = "EVT_ERROR_SDK";
            break;
        default:
            statusStr = "Unknown Error";
            break;
    }
    updateStatus(statusStr.c_str());
    ERR_ARGS("EVT Error: %s, Error Code: %d\n", statusStr.c_str(), status);
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
    printf("Serial: %s\n", this->pdeviceInfo->serialNumber);
    //printf("Manufacturer Specific Information: %s\n", this->pdeviceInfo->manufacturerSpecifiedInfo);
    printf("--------------------------------------\n");
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
 * @return:     status          -> success if connected, error if not connected
 */
asynStatus ADEmergentVision::connectToDeviceEVT(){
    const char* functionName = "connectToDeviceEVT";
    unsigned int count, numCameras;
    numCameras = 10;
    struct GigEVisionDeviceInfo deviceList[10];
    this->evt_status = EVT_ListDevices(deviceList, &numCameras, &count);
    if(this->evt_status != EVT_SUCCESS){
        reportEVTError(this->evt_status, functionName);
        return asynError;
    }
    else if(count == 0){
        ERR("No Cameras detected on the network\n");
        return asynError;
    }
    else{
        unsigned int i;
        for(i = 0; i< count; i++){
            if(strcmp(deviceList[i].serialNumber, this->serialNumber) == 0){
                pdeviceInfo = (struct GigEVisionDeviceInfo*) malloc(sizeof(struct GigEVisionDeviceInfo));
                *pdeviceInfo = deviceList[i];
                break;
            }
        }
        if(pdeviceInfo == NULL){
            ERR("Could not find camera with specified serial number\n");
            return asynError;
        }
        else{
            this->evt_status = EVT_CameraOpen(this->pcamera, this->pdeviceInfo);
            printConnectedDeviceInfo();
            if(this->evt_status != EVT_SUCCESS){
                reportEVTError(this->evt_status, functionName);
                return asynError;
            }
            unsigned int height_max, width_max;
            //Get resolution.
            EVT_CameraGetUInt32ParamMax(&camera, "Height", &height_max);
            EVT_CameraGetUInt32ParamMax(&camera, "Width" , &width_max);
            printf("Max Resolution: %d by %d\n", width_max, height_max);

            // Update maximum possible sensor size
            setIntegerParam(ADMaxSizeX, width_max);
            setIntegerParam(ADMaxSizeY, height_max);

            // Default target resolution to be the maximum
            setIntegerParam(ADSizeX, width_max);
            setIntegerParam(ADSizeY, height_max);

            this->connected = 1;
            collectCameraInformation();
            return asynSuccess;
        }
    }
}


/**
 * Override of base ADDriver connect function.
 * Simply calls the connectToDeviceEVT function and returns the result.
 * 
 * @return:     status          -> success if connected, error if not connected
 */
asynStatus ADEmergentVision::connect(asynUser* pasynUser) {
    return connectToDeviceEVT();
}


/**
 * Function that disconnects from any connected EVT device
 * First checks if is connected, then if it is, it frees the memory
 * for the info and the camera
 * 
 * @return: status  -> success if freed, error if never connected
 */
asynStatus ADEmergentVision::disconnectFromDeviceEVT(){
    this->connected = 0;
    const char* functionName = "disconnectFromDeviceEVT";
    int acquiring;
    getIntegerParam(ADAcquire, &acquiring);
    if(acquiring) acquireStop();
    if(this->pdeviceInfo == NULL || this->pcamera == NULL){
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s Never connected to device\n", driverName, functionName);
        return asynError;
    }
    else{
        free(this->pdeviceInfo);
        asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, "%s::%s Closing camera connection\n", driverName, functionName);

        this->evt_status = EVT_CameraClose(this->pcamera);
        if(this->evt_status != EVT_SUCCESS){
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s ERROR - Could not close camera correctly\n", driverName, functionName);
            reportEVTError(this->evt_status, functionName);
            return asynError;
        }
        printf("Disconnected from device.\n");
        this->connected = 0;
        return asynSuccess;
    }
}


/**
 * Override of base ADDriver disconnect function.
 * Simply calls the disconnectFromDeviceEVT function and returns the result.
 * 
 * @return:     status          -> success if disconnected, error if not disconnected
 */
asynStatus ADEmergentVision::disconnect(asynUser* pasynUser){
    return disconnectFromDeviceEVT();
}


/**
 * Function that updates PV values with camera information
 * 
 * @return: status
 */
asynStatus ADEmergentVision::collectCameraInformation(){
    const char* functionName = "collectCameraInformation";
    if (connected == 0) return asynError;
    LOG("Collecting camera information");
    setStringParam(ADManufacturer, this->pdeviceInfo->manufacturerName);
    setStringParam(ADSerialNumber, this->pdeviceInfo->serialNumber);
    setStringParam(ADFirmwareVersion,this->pdeviceInfo->deviceVersion);
    setStringParam(ADModel, this->pdeviceInfo->modelName);
    EVT_CameraGetEnumParamRange(this->pcamera, "PixelFormat", this->supportedModes, SUPPORTED_MODE_BUFFER_SIZE, &(this->supportedModeSizeReturn));
    printf("Supported formats: %s\n", this->supportedModes);
    return asynSuccess;
}


// -----------------------------------------------------------------------
// ADEmergentVision Acquisition Functions
// -----------------------------------------------------------------------


/**
 * Function that will set all camera parameters from their PV Values
 * 
 * TODO
 * 
 * return: void
 */
asynStatus ADEmergentVision::setCameraValues(){
    if (connected == 0) return asynError;
    EVT_CameraSetEnumParam(this->pcamera,   "AcquisitionMode",        "Continuous");
    EVT_CameraSetUInt32Param(this->pcamera, "AcquisitionFrameCount",  1);
    EVT_CameraSetEnumParam(this->pcamera,   "TriggerSelector",        "AcquisitionStart");
    EVT_CameraSetEnumParam(this->pcamera,   "TriggerMode",            "Off");
    EVT_CameraSetEnumParam(this->pcamera,   "TriggerSource",          "Software");
    EVT_CameraSetEnumParam(this->pcamera,   "BufferMode",             "Off");
    EVT_CameraSetUInt32Param(this->pcamera, "BufferNum",              0);
    return asynSuccess;
}

/**
 * Function that initializes the image acquisition thread for the EVT camera
 * calls pthread_create to create the thread and sets the threadActive flag to true 
 *
 * return: status
 */
asynStatus ADEmergentVision::startImageAcquisitionThread(){
    const char* functionName = "startImageAcquisitionThread";
    asynStatus status;
    if(this->imageCollectionThreadActive == 0){
        this->imageCollectionThreadActive = 1;
        thread imageThread(evtCallbackWrapper, this);
        imageThread.detach();
        printf("Image acquistion thread started.\n");
        status = asynSuccess;
    }
    else{
        ERR("Acquisition thread already active\n");
        status = asynError;
    }
    return status;
}


/**
 * Function that stops the image acquisition thread
 * Sets flag for active to false.
 * 
 */
asynStatus ADEmergentVision::stopImageAcquisitionThread(){
    const char* functionName = "stopImageAcquisitionThread";
    asynStatus status;
    if(this->imageCollectionThreadActive == 0){
        ERR("Image thread not active\n");
        status = asynError;
    }
    else{
        this->imageCollectionThreadActive = 0;
        printf("Stopping image acquisition thread.\n");
    }
    return status;
}


string ADEmergentVision::getSupportedFormatStr(PIXEL_FORMAT evtPixelFormat){
    const char* functionName = "getSupportedFormatStr";
    string supportedFormatStr;
    switch(evtPixelFormat){
        case GVSP_PIX_MONO8:
            supportedFormatStr = "Mono8";
            break;
        case GVSP_PIX_MONO10:
            supportedFormatStr = "Mono10";
            break;
        case GVSP_PIX_MONO12:
            supportedFormatStr = "Mono12";
            break;
        case GVSP_PIX_MONO10_PACKED:
            supportedFormatStr = "Mono10Packed";
            break;
        case GVSP_PIX_MONO12_PACKED:
            supportedFormatStr = "Mono12Packed";
            break;
        case GVSP_PIX_RGB8:
            supportedFormatStr = "RGB8Packed";
            break;
        case GVSP_PIX_RGB10:
            supportedFormatStr = "RGB10Packed";
            break;
        case GVSP_PIX_RGB12:
            supportedFormatStr = "RGB12Packed";
            break;
        case GVSP_PIX_BAYRG8:
            supportedFormatStr = "BayerRG8";
            break;
        case GVSP_PIX_BAYRG10:
            supportedFormatStr = "BayerRG10";
            break;
        case GVSP_PIX_BAYRG12:
            supportedFormatStr = "BayerRG12";
            break;
        case GVSP_PIX_BAYBG10_PACKED:
            supportedFormatStr = "BayerRG10Packed";
            break;
        case GVSP_PIX_BAYRG12_PACKED:
            supportedFormatStr = "BayerRG12Packed";
            break;
        default:
            supportedFormatStr = "";
            break;
    }
    return supportedFormatStr;
}


bool ADEmergentVision::isFrameFormatValid(const char* formatStr){
    const char* functionName = "isFrameFormatValid";
    char* nextToken;
    bool valid = false;
    char temp[SUPPORTED_MODE_BUFFER_SIZE];
    memcpy(temp, this->supportedModes, SUPPORTED_MODE_BUFFER_SIZE);
    char* enumMember = strtok_s(temp, ",", &next_token);
    while(enumMember != NULL){
        if(strcmp(formatStr, enumMember) == 0) valid = true;
        enumMember = strtok_s(NULL, ",", &next_token);
    }
    return valid;
}


/**
 * Function responsible for starting camera image acqusition. First, check if there is a
 * camera connected. Then, set camera values by reading from PVs. Then, we execute the 
 * Acquire Start command. if this command was successful, image acquisition started.
 * 
 * @return: status  -> error if no device, camera values not set, or execute command fails. Otherwise, success
 */
asynStatus ADEmergentVision::acquireStart(){
    setIntegerParam(ADEVT_Framerate, 30);
    const char* functionName = "acquireStart";
    if (connected == 0) return asynError;
    asynStatus status;
    if(this->pcamera == NULL){
        ERR("Error: No camera connected");
        status = asynError;
    }
    else{
        unsigned int evtPixelFormat;
        getFrameFormatEVT(&evtPixelFormat);

        string pixelMode = getSupportedFormatStr((PIXEL_FORMAT) evtPixelFormat);
        printf("Starting acquisition with pixel mode %s\n", pixelMode.c_str());
        if(!isFrameFormatValid(pixelMode.c_str())) status = asynError;
        else status = setCameraValues();
        if(status != asynSuccess){
            ERR_ARGS("Invalid camera settings! Supported formats: %s", this->supportedModes);
        }
        else{
            this->evt_status = EVT_CameraOpenStream(pcamera);
            startImageAcquisitionThread();
            if(this->evt_status != EVT_SUCCESS){
                reportEVTError(this->evt_status, functionName);
                setIntegerParam(ADAcquire, 0);
                setIntegerParam(ADStatus, ADStatusIdle);
                callParamCallbacks();
                status = asynError;
            }
            else{
                this->evt_status = EVT_CameraExecuteCommand(this->pcamera, "AcquisitionStart");
                if(this->evt_status != EVT_SUCCESS){
                    stopImageAcquisitionThread();
                    ERR("Failed to start acquistion.");
                    status = asynError;
                }
                else{
                    setIntegerParam(ADStatus, ADStatusAcquire);
                    //asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s Image acquistion start\n", driverName, functionName);
                    callParamCallbacks();
                }
            }

        }
    }
    return status;
}


/**
 * Function responsible for stopping camera image acquisition. First check if the camera is connected.
 * If it is, execute the 'AcquireStop' command. Then set the appropriate PV values, and callParamCallbacks
 * 
 * @return: status  -> error if no camera or command fails to execute, success otherwise
 */ 
asynStatus ADEmergentVision::acquireStop(){
    const char* functionName = "acquireStop";
    if (connected == 0) return asynError;
    asynStatus status;
    if(this->pcamera == NULL){
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s Error: No camera connected\n", driverName, functionName);
        status = asynError;
    }
    else{
        stopImageAcquisitionThread();
        // Make sure camera acquisition is completed before we close the stream.
        while(this->imageThreadOpen == 1)
            epicsThreadSleep(0.1);
        this->evt_status = EVT_CameraExecuteCommand(&camera, "AcquisitionStop");
        if(this->evt_status != EVT_SUCCESS){
            reportEVTError(this->evt_status, functionName);
            status = asynError;
        }
        else{
            this->evt_status = EVT_CameraCloseStream(this->pcamera);
            if(this->evt_status != EVT_SUCCESS) reportEVTError(this->evt_status, functionName);
        }
    }
    setIntegerParam(ADStatus, ADStatusIdle);
    setIntegerParam(ADAcquire, 0);
    callParamCallbacks();
    return status;
}


/**
 * Function that takes selected NDDataType and NDColorMode, and converts into an EVT pixel type
 * This is then used by the camera when starting image acquisiton
 * 
 * @params[out]: evtPixelType   -> Pixel type enum value to be used given the current configuration.
 * @return: status              -> error if combination of dtype and color mode invalid
 */
asynStatus ADEmergentVision::getFrameFormatEVT(unsigned int* evtPixelType){
    const char* functionName = "getFrameFormatEVT";
    asynStatus status = asynSuccess;
    int pixelFormat;
    int colorMode;
    getIntegerParam(ADEVT_PixelFormat, &pixelFormat);
    getIntegerParam(NDColorMode, &colorMode);

    switch((NDColorMode_t) colorMode){
        case NDColorModeMono:
            switch(pixelFormat){
                case 0:
                    *evtPixelType = GVSP_PIX_MONO8;
                    break;
                case 1:
                    *evtPixelType = GVSP_PIX_MONO10;
                    break;
                case 2:
                    *evtPixelType = GVSP_PIX_MONO12;
                    break;
                case 3:
                    *evtPixelType = GVSP_PIX_MONO10_PACKED;
                    break;
                case 4:
                    *evtPixelType = GVSP_PIX_MONO12_PACKED;
                    break;
                default:
                    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s Unsupported data type for this color mode\n", driverName, functionName);
                    return asynError;
            }
            break;
        case NDColorModeRGB1:
            switch(pixelFormat){
                case 0:
                    *evtPixelType = GVSP_PIX_RGB8;
                    break;
                case 1:
                    *evtPixelType = GVSP_PIX_RGB10;
                    break;
                case 2:
                    *evtPixelType = GVSP_PIX_RGB12;
                    break;
                default:
                    ERR("Unsupported data type for this color mode\n");
                    return asynError;
            }
            break;
        case NDColorModeBayer:
            switch(pixelFormat){
                case 0:
                    *evtPixelType = GVSP_PIX_BAYRG8;
                    break;
                case 1:
                    *evtPixelType = GVSP_PIX_BAYRG10;
                    break;
                case 2:
                    *evtPixelType = GVSP_PIX_BAYRG12;
                    break;
                case 3:
                    *evtPixelType = GVSP_PIX_BAYRG10_PACKED;
                    break;
                case 4:
                    *evtPixelType = GVSP_PIX_BAYRG12_PACKED;
                    break;
                default:
                    ERR("Unsupported data type for this color mode\n");
                    return asynError;
            }
            break;
        default:
            ERR("Not supported color format\n");
            status = asynError;
            break;
    }
    return status;
}



/**
 * Function that gets frame format information from a captured frame from the camera
 * 
 * TODO: This function needs to be reworked
 * 
 * @params[in]:     frame       -> pointer to currently acquired frame
 * @params[out]:    dataType    -> pointer to output data type
 * @params[out]:    colorMode   -> pointer to output color mode
 * @return:         status       
 */
asynStatus ADEmergentVision::getFrameFormatND(CEmergentFrame* frame, NDDataType_t* dataType, NDColorMode_t* colorMode){
    const char* functionName = "getFrameFormatND";
    asynStatus status = asynSuccess;
    unsigned int evtDepth = frame->pixel_type;
    switch(evtDepth){
        case GVSP_PIX_MONO8:
            *dataType = NDUInt8;
            *colorMode = NDColorModeMono;
            break;
        case GVSP_PIX_RGB8:
            *dataType = NDUInt8;
            *colorMode = NDColorModeRGB1;
            break;
        case GVSP_PIX_MONO10:
        case GVSP_PIX_MONO12:
        case GVSP_PIX_MONO10_PACKED:
        case GVSP_PIX_MONO12_PACKED:
            *dataType = NDUInt16;
            *colorMode = NDColorModeMono;
            break;
        case GVSP_PIX_RGB10:
        case GVSP_PIX_RGB12:
            *dataType = NDUInt16;
            *colorMode = NDColorModeRGB1;
            break;
        case GVSP_PIX_BAYRG8:
            *dataType = NDUInt8;
            *colorMode = NDColorModeBayer;
            break;
        case GVSP_PIX_BAYRG10:
        case GVSP_PIX_BAYRG10_PACKED:
        case GVSP_PIX_BAYRG12:
        case GVSP_PIX_BAYRG12_PACKED:
            *dataType = NDUInt16;
            *colorMode = NDColorModeBayer;
            break;
        default:
            //not a supported depth
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s Unsupported Frame format\n", driverName, functionName);
            *dataType = NDUInt8;
            break;
    }
    //currently only mono images supported
    *colorMode = NDColorModeMono;
    return status;
}

unsigned int ADEmergentVision::getConvertBitDepth(PIXEL_FORMAT evtPixelFormat) {
    unsigned int convert = EVT_CONVERT_NONE;
    int dataType;

    getIntegerParam(NDDataType, &dataType);

    if (evtPixelFormat == GVSP_PIX_MONO8 || evtPixelFormat == GVSP_PIX_RGB8 || evtPixelFormat == GVSP_PIX_BAYRG8) {
        if ((NDDataType_t)dataType == NDUInt16 || (NDDataType_t)dataType == NDInt16)
            convert = EVT_CONVERT_16BIT;
    }
    else {
        if ((NDDataType_t)dataType == NDUInt8 || (NDDataType_t)dataType == NDInt8)
            convert = EVT_CONVERT_8BIT;
        else convert = EVT_CONVERT_16BIT;
    }

    return convert;
}


/**
 * Function that allocates space for a new NDArray and copies the data from the captured EVT frame
 * 
 * NDArray dimensions depend on the color mode and data type. Run getFrameFormatND to get these.
 * Next, we allocate space for the NDArray. Then, we copy the image data from the Emergent Frame
 * to the NDArray. Then we set the attributes of the new NDArray to the appropriate dtype and color mode.
 * 
 * @params[in]:     frame   -> frame recieved from Emergent Vision Camera
 * @params[out]:    pArray  -> NDArray output that is pushed out to ArrayData PV
 * @return:         status  -> success if copied, error if alloc/copy failed
 */
asynStatus ADEmergentVision::evtFrame2NDArray(CEmergentFrame* evtFrame, CEmergentFrame* evtConvertFrame, NDArray** pArray){
    const char* functionName = "evtFrame2NDArray";
    asynStatus status = asynSuccess;
    
    size_t dims[3];
    int ndims;
    int dataType;
    int colorMode;
    int xsize;
    int ysize;
    NDArrayInfo arrayInfo;
    //status = getFrameFormatND(frame, &dataType, &colorMode);
    getIntegerParam(NDDataType, &dataType);
    getIntegerParam(NDColorMode, &colorMode);

    unsigned int convert = getConvertBitDepth(evtFrame->pixel_type);

    if(status == asynError){
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s Error computing dType and color mode\n", driverName, functionName);
        return asynError;
    }
    else{
        xsize = evtFrame->size_x;
        ysize = evtFrame->size_y;
        if(colorMode == NDColorModeMono) ndims = 2;
        else ndims = 3;

        if(ndims == 2){
            dims[0] = xsize;
            dims[1] = ysize;
        }
        else{
            dims[0] = 3;
            dims[1] = xsize;
            dims[2] = ysize;
        }

        this->pArrays[0] = pNDArrayPool->alloc(ndims, dims, (NDDataType_t) dataType, 0, NULL);
        if(this->pArrays[0]!=NULL) (*pArray) = this->pArrays[0];
        else{
            this->pArrays[0]->release();
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s Unable to allocate array\n", driverName, functionName);
            return asynError;
        }
        CEmergentFrame* targetFrame = evtFrame;
        if (convert != EVT_CONVERT_NONE || (evtFrame->pixel_type == GVSP_PIX_MONO10_PACKED || evtFrame->pixel_type == GVSP_PIX_MONO12_PACKED)) {
            EVT_FrameConvert(evtFrame, evtConvertFrame, convert, EVT_COLOR_CONVERT_NONE);
            targetFrame = evtConvertFrame;
        }
        EVT_FrameSave(evtFrame, "/home/jwlodek/test.tif", EVT_FILETYPE_TIF, EVT_ALIGN_LEFT);


        (*pArray)->getInfo(&arrayInfo);
        size_t total_size = arrayInfo.totalBytes;
        memcpy((unsigned char*)(*pArray)->pData, targetFrame->imagePtr, total_size);
        (*pArray)->pAttributeList->add("ColorMode", "Color Mode", NDAttrInt32, &colorMode);
        //int arrayCounter;
        //getIntegerParam(NDArrayCounter, &arrayCounter);
        //arrayCounter++;
        //setIntegerParam(NDArrayCounter, arrayCounter);
        ////refresh PVs
        //callParamCallbacks();
        getAttributes((*pArray)->pAttributeList);
        return asynSuccess;
    }
}

void* ADEmergentVision::evtCallbackWrapper(void* pPtr){
    ADEmergentVision* pEVT = (ADEmergentVision*) pPtr;
    pEVT->evtCallback();
    return NULL;
}


/**
 * Function that constantly loops and on each loop, it collects a frame and converts it to an NDArray and
 * pushes it to the ArrayData PV. It is called from a pthread.
 * 
 * @return: void*
 */
void ADEmergentVision::evtCallback(){
    const char* functionName = "evtCallback";
    int imageMode;
    CEmergentFrame evtFrame;
    CEmergentFrame evtFrameConvert;
    asynStatus status;

    int numFramesCollected = 1;
    int uniqueIDCounter;
    int imageCounter;
    int xsize, ysize;
    unsigned int evtPixelType;
    this->imageThreadOpen = 1;
    getIntegerParam(ADImageMode, &imageMode);


    while(this->imageCollectionThreadActive == 1){
        NDArray* pArray;
        NDArrayInfo arrayInfo;
        // One frame for current image, one for conversion if necessary

        getIntegerParam(ADNumImagesCounter, &uniqueIDCounter);
        getIntegerParam(ADSizeX, &xsize);
        getIntegerParam(ADSizeY, &ysize);
        
        asynStatus current = getFrameFormatEVT(&evtPixelType);

        if(current == asynError){
            ERR("Error finding evt frame format");
        }
        else{
            EVT_ERROR alloc = EVT_SUCCESS;
            EVT_ERROR err = EVT_SUCCESS;
            //if(err != EVT_SUCCESS) reportEVTError(err, "EVT_CameraExecuteCommand - AcqusitionStart");

            evtFrame.size_x = xsize;
            evtFrame.size_y = ysize;
            evtFrame.pixel_type = (PIXEL_FORMAT) evtPixelType;

            evtFrameConvert.size_x = xsize;
            evtFrameConvert.size_y = ysize;
            evtFrameConvert.pixel_type = (PIXEL_FORMAT) evtPixelType;
            evtFrameConvert.convertColor = EVT_COLOR_CONVERT_NONE;
            evtFrameConvert.convertBitDepth = getConvertBitDepth((PIXEL_FORMAT) evtPixelType);

            //printf("allocating frame buffer command\n");
            alloc = EVT_AllocateFrameBuffer(this->pcamera, &evtFrame, EVT_FRAME_BUFFER_ZERO_COPY);
            if (alloc == EVT_SUCCESS) alloc = EVT_AllocateFrameBuffer(this->pcamera, &evtFrameConvert, EVT_FRAME_BUFFER_DEFAULT);
            
            if(alloc != EVT_SUCCESS) reportEVTError(alloc, "EVT_AllocateFrameBuffer");
            else {

                LOG("Queue camera frame");
                if (alloc == EVT_SUCCESS) err = EVT_CameraQueueFrame(this->pcamera, &evtFrame);
                if (err != EVT_SUCCESS) reportEVTError(err, "EVT_CameraQueueFrame");

                //asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, "%s::%s Triggering w/ software\n", driverName, functionName);
                //if (err == EVT_SUCCESS) err = EVT_CameraExecuteCommand(this->pcamera, "TriggerSoftware");
                //if (err != EVT_SUCCESS) reportEVTError(err, "EVT_CameraExecuteCommand - TriggerSoftware");

                LOG("Grabbing frame");
                if (err == EVT_SUCCESS) err = EVT_CameraGetFrame(this->pcamera, &evtFrame, EVT_INFINITE);
                if (err != EVT_SUCCESS) reportEVTError(err, "EVT_CameraGetFrame");


                // Only process the frame if we successfully finished all of the above commands.
                if (err == EVT_SUCCESS) {
                    //printf("Converting to NDArray\n");
                    status = evtFrame2NDArray(&evtFrame, &evtFrameConvert, &pArray);
                    if (status == asynSuccess) {
                        //printf("Converted to NDArray\n");
                        pArray->uniqueId = uniqueIDCounter;
                        updateTimeStamp(&pArray->epicsTS);
                        doCallbacksGenericPointer(pArray, NDArrayData, 0);
                        pArray->getInfo(&arrayInfo);
                        size_t total_size = arrayInfo.totalBytes;
                        setIntegerParam(NDArraySize, (int)total_size);
                        setIntegerParam(NDArraySizeX, arrayInfo.xSize);
                        setIntegerParam(NDArraySizeY, arrayInfo.ySize);

                        pArray->release();
                    }

                    getIntegerParam(NDArrayCounter, &imageCounter);
                    imageCounter++;
                    setIntegerParam(NDArrayCounter, imageCounter);
                    callParamCallbacks();

                    if (status == asynError) {
                        this->imageThreadOpen = 0;
                        ERR("Error converting to NDArray");
                        acquireStop();
                        break;
                    }
                    if (imageMode == ADImageSingle) {
                        this->imageThreadOpen = 0;
                        acquireStop();
                    }
                    else if (imageMode == ADImageMultiple) {
                        int numImages;
                        getIntegerParam(ADNumImages, &numImages);

                        if (numFramesCollected == numImages) {
                            this->imageThreadOpen = 0;
                            acquireStop();
                        }
                    }
                }
                else{
                    reportEVTError(err, functionName);
                }
                //If frame buffer allocation was successful, we need to deallocate the frame buffer no matter what.
                //printf("RELEASING FRAME BUFFER\n");
                err = EVT_ReleaseFrameBuffer(this->pcamera, &evtFrame);
                if (err != EVT_SUCCESS) reportEVTError(err, "EVT_ReleaseFrameBuffer");
                err = EVT_ReleaseFrameBuffer(this->pcamera, &evtFrameConvert);
                if (err != EVT_SUCCESS) reportEVTError(err, "EVT_ReleaseFrameBuffer");
            }
        }
        //printf("done with first loop iteration\n");
        numFramesCollected++;
    }
    this->imageThreadOpen = 0;
}


// -----------------------------------------------------------------------
// ADEmergentVision Camera Functions (Exposure, Format, Gain etc.)
// -----------------------------------------------------------------------


bool ADEmergentVision::isEVTInt32ParamValid(unsigned int newVal, const char* param){

    const char* functionName = "validateEVTInt32Param";
    if(this->connected == 0) return false;
    bool valid = true;
    unsigned int max, min, inc;
    EVT_CameraGetUInt32ParamMax(this->pcamera, param, &max);
    EVT_CameraGetUInt32ParamMin(this->pcamera, param, &min);
    EVT_CameraGetUInt32ParamInc(this->pcamera, param, &inc);
    if(newVal < min || newVal > max){
        ERR_ARGS("Parameter %s must be between %d and %d!", param, min, max);
        valid = false;
    }
    return valid;
}

asynStatus ADEmergentVision::getEVTInt32Param(unsigned int* retVal, const char* param){
    const char* functionName = "getEVTInt32Param";
    if(this->connected == 0) return asynError;
    asynStatus status = asynSuccess;
    this->evt_status = EVT_CameraGetUInt32Param(this->pcamera, param, retVal);
    if(evt_status != EVT_SUCCESS){
        status = asynError;
        reportEVTError(evt_status, param);
    }
    return status;
}

asynStatus ADEmergentVision::setEVTInt32Param(unsigned int newVal, const char* param){
    const char* functionName = "setEVTInt32Param";
    if(this->connected == 0) return asynError;
    if(!isEVTInt32ParamValid(newVal, param)) return asynError;
    
    asynStatus status = asynSuccess;
    this->evt_status = EVT_CameraSetUInt32Param(this->pcamera, param, newVal);
    if(evt_status != EVT_SUCCESS){
        status = asynError;
        printf("Failed to set %s to %d!\n", param, newVal);
        reportEVTError(evt_status, functionName);
    }
    else{
        printf("Set %s to %d\n", param, newVal);
    }
    return status;
}


asynStatus ADEmergentVision::getEVTBoolParam(bool* retVal, const char* param){
    const char* functionName = "getEVTBoolParam";
    if(this->connected == 0) return asynError;
    asynStatus status = asynSuccess;
    this->evt_status = EVT_CameraGetBoolParam(this->pcamera, param, retVal);
    if(evt_status != EVT_SUCCESS){
        status = asynError;
        reportEVTError(evt_status, functionName);
    }
    return status;
}

asynStatus ADEmergentVision::setEVTBoolParam(bool newVal, const char* param){
    const char* functionName = "setEVTBoolParam";
    if(this->connected == 0) return asynError;
    asynStatus status = asynSuccess;
    this->evt_status = EVT_CameraSetBoolParam(this->pcamera, param, newVal);
    if(evt_status != EVT_SUCCESS){
        status = asynError;
        reportEVTError(evt_status, functionName);
    }
    else if(newVal) printf("Enabled %s\n", param);
    else printf("Disabled %s\n", param);
    return status;
}


// -----------------------------------------------------------------------
// ADEmergentVision ADDriver Overrides (WriteInt32/WriteFloat64/report)
// -----------------------------------------------------------------------


/**
 * Function overwriting ADDriver base function.
 * Takes in a function (PV) changes, and a value it is changing to, and processes the input
 *
 * @params[in]: pasynUser       -> asyn client who requests a write
 * @params[in]: value           -> int32 value to write
 * @return:     asynStatus      -> success if write was successful, else failure
 */
asynStatus ADEmergentVision::writeInt32(asynUser* pasynUser, epicsInt32 value){
    int function = pasynUser->reason;
    int acquiring;
    asynStatus status = asynSuccess;
    const char* functionName = "writeInt32";
    getIntegerParam(ADAcquire, &acquiring);

    status = setIntegerParam(function, value);
    if(status != asynSuccess){
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s Error writing to PV\n", driverName, functionName);
        return status;
    }
    else{
        if(function == ADAcquire){
            if(value && !acquiring){
                status = acquireStart();
            }
            else if (!value && acquiring){
                status = acquireStop();
            }
        }
        else if(function == ADImageMode){
            if(acquiring) acquireStop();
            if(value == ADImageSingle) setIntegerParam(ADNumImages, 1);
            else if(value == ADImageMultiple){
                setIntegerParam(ADNumImages, 100);
            }
        }
        else if(function == ADEVT_PixelFormat || function == NDColorMode){
            unsigned int evtPixelFormat;
            status = getFrameFormatEVT(&evtPixelFormat);
            if(status == asynError){
                ERR("Invalid pixel format selected!");
            }
            else{
                string pixelFormatStr = getSupportedFormatStr((PIXEL_FORMAT) evtPixelFormat);
                EVT_ERROR err = EVT_CameraSetEnumParam(this->pcamera, "PixelFormat", pixelFormatStr.c_str());
                if(err != EVT_SUCCESS){
                    reportEVTError(err, functionName);
                    status = asynError;
                }
                else printf("Set camera pixel format parameter: %s\n", pixelFormatStr.c_str());
            }
        }
        else if(function == ADEVT_Framerate) status = setEVTInt32Param((unsigned int) value, "FrameRate");
        else if(function == ADEVT_OffsetX) status = setEVTInt32Param((unsigned int) value, "OffsetX");
        else if(function == ADEVT_OffsetY) status = setEVTInt32Param((unsigned int) value, "OffsetY");
        //else if(function == ADEVT_BufferNum) status = setEVTInt32Param((unsigned int) value, "BufferNum");
        else if(function == ADEVT_LUTEnable) status = setEVTBoolParam(value > 0, "LUTEnable");
        else if(function == ADEVT_AutoGain) status = setEVTBoolParam(value > 0, "AutoGain");
        else if(function == ADEVT_BufferMode){
            EVT_ERROR err;
            if(value > 0) err = EVT_CameraSetEnumParam(this->pcamera, "BufferMode", "On");
            else err = EVT_CameraSetEnumParam(this->pcamera, "BufferMode", "Off");
            if(err != EVT_SUCCESS){
                status = asynError;
                reportEVTError(err, functionName);
            }
        }
        else if(function == ADSizeX) status = setEVTInt32Param((unsigned int) value, "Width");
        else if(function == ADSizeY) status = setEVTInt32Param((unsigned int) value, "Height");
        else if(function < ADEVT_FIRST_PARAM){
            status = ADDriver::writeInt32(pasynUser, value);
        }
    }
    callParamCallbacks();
    if(status == asynError){
        ERR_ARGS("ERROR status=%d, function=%d, value=%d\n", status, function, value);
    }
    else{
        LOG_ARGS("function=%d value=%d\n", function, value);
    }
    return status;
}


/**
 * Function overwriting ADDriver base function.
 * Takes in a function (PV) changes, and a value it is changing to, and processes the input
 * This is the same functionality as writeInt32, but for processing doubles.
 *
 * @params[in]: pasynUser       -> asyn client who requests a write
 * @params[in]: value           -> float64 value to write
 * @return:     asynStatus      -> success if write was successful, else failure
 */
asynStatus ADEmergentVision::writeFloat64(asynUser* pasynUser, epicsFloat64 value){
    int function = pasynUser->reason;
    int acquiring;
    asynStatus status = asynSuccess;
    const char* functionName = "writeFloat64";
    getIntegerParam(ADAcquire, &acquiring);

    status = setDoubleParam(function, value);
    if(function == ADAcquireTime){
        // exposure time is an integer in microseconds. Input will be in milliseconds.
        unsigned int exposureTime = (unsigned int) (value * 1000);
        printf("Trying to set exposure to %d ms...\n", exposureTime);
        status = setEVTInt32Param(exposureTime, "Exposure");
    }
    else if(function == ADGain) {
        unsigned int gain = (unsigned int) (value * 1000);
        status = setEVTInt32Param(gain, "Gain");
    }
    else if(function < ADEVT_FIRST_PARAM){
        status = ADDriver::writeFloat64(pasynUser, value);
    }
    callParamCallbacks();
    if(status == asynError){
        ERR_ARGS("ERROR status=%d, function=%d, value=%lf\n", status, function, value);
    }
    else LOG_ARGS("function=%d value=%lf\n", function, value);
    return status;
}


/**
 * Function used for reporting ADEmergentVision device and library information to a external
 * log file. The function first prints all GigEVision specific information to the file,
 * then continues on to the base ADDriver 'report' function
 * 
 * @params[in]: fp      -> pointer to log file
 * @params[in]: details -> number of details to write to the file
 * @return:     void
 */
void ADEmergentVision::report(FILE* fp, int details){
    const char* functionName = "report";
    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s reporting to external log file\n", driverName, functionName);
    fprintf(fp, "--------------------------------------\n");
    fprintf(fp, "Connected to EVT device\n");
    fprintf(fp, "--------------------------------------\n");
    fprintf(fp, "Specification: %d.%d\n", this->pdeviceInfo->specVersionMajor, this->pdeviceInfo->specVersionMinor);
    fprintf(fp, "Device mode: %d, Device Version: %s\n", this->pdeviceInfo->deviceMode, this->pdeviceInfo->deviceVersion);
    fprintf(fp, "ManufacturerName: %s, Model name %s\n", this->pdeviceInfo->manufacturerName, this->pdeviceInfo->modelName);
    fprintf(fp, "IP: %s, Mask %s\n",this->pdeviceInfo->currentIp, this->pdeviceInfo->currentSubnetMask);
    fprintf(fp, "MAC address: %s\n", this->pdeviceInfo->macAddress);
    fprintf(fp, "Serial: %s, User Name: %s\n", this->pdeviceInfo->serialNumber, this->pdeviceInfo->userDefinedName);
    fprintf(fp, "Manufacturer Specific Information: %s\n", this->pdeviceInfo->manufacturerSpecifiedInfo);

    ADDriver::report(fp, details);
}


// -----------------------------------------------------------------------
// ADEmergentVision Constructor/Destructor
// -----------------------------------------------------------------------


/*
 * Constructor for ADEmergentVision driver. Most params are passed to the parent ADDriver constructor.
 * Connects to the camera, then gets device information, and is ready to aquire images.
 *
 * @params[in]: portName        -> port for NDArray recieved from camera
 * @params[in]: serialNumber    -> serial number of device to connect to
 * @params[in]: maxBuffers      -> max buffer size for NDArrays
 * @params[in]: maxMemory       -> maximum memory allocated for driver
 * @params[in]: priority        -> what thread priority this driver will execute with
 * @params[in]: stackSize       -> size of the driver on the stack
 */
ADEmergentVision::ADEmergentVision(const char* portName, const char* serialNumber, int maxBuffers, size_t maxMemory, int priority, int stackSize)
    : ADDriver(portName, 1, (int)NUM_EVT_PARAMS, maxBuffers, maxMemory, asynEnumMask, asynEnumMask, ASYN_CANBLOCK, 1, priority, stackSize){

    asynStatus status;

    const char* functionName = "ADEmergentVision";
    char evtVersionString[25];
    epicsSnprintf(evtVersionString, sizeof(evtVersionString), "%s", EVT_SDKVersion());
    setStringParam(ADSDKVersion, evtVersionString);

    char versionString[25];
    epicsSnprintf(versionString, sizeof(versionString), "%d.%d.%d", ADEMERGENTVISION_VERSION, ADEMERGENTVISION_REVISION, ADEMERGENTVISION_MODIFICATION);
    setStringParam(NDDriverVersion, versionString);

    if(strlen(serialNumber) == 0){
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s Error: invalid serial number passed\n", driverName, functionName);
        status = asynError;
    }
    else{
        this->serialNumber = serialNumber;
        status = connectToDeviceEVT();
    }

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

    if(status == asynError)
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s Failed to connect to device\n", driverName, functionName);

    epicsAtExit(exitCallback, (void*) this);
}


/* ADEmergentVision Destructor */
ADEmergentVision::~ADEmergentVision(){
    const char* functionName = "~ADEmergentVision";
    printf("Uninitializing Emergent Vision Detector API.\n");
    this->lock();
    disconnectFromDeviceEVT();
    this->unlock();
    printf("%s::%s ADEmergentVision Driver Exiting...\n", driverName, functionName);
}


// -----------------------------------------------------------------------
// ADEmergentVision IOC Shell Registration Functions
// -----------------------------------------------------------------------


/* EVTConfig -> These are the args passed to the constructor in the epics config function */
static const iocshArg EVTConfigArg0 = { "Port name",        iocshArgString };
static const iocshArg EVTConfigArg1 = { "Serial number",    iocshArgString };
static const iocshArg EVTConfigArg2 = { "maxBuffers",       iocshArgInt };
static const iocshArg EVTConfigArg3 = { "maxMemory",        iocshArgInt };
static const iocshArg EVTConfigArg4 = { "priority",         iocshArgInt };
static const iocshArg EVTConfigArg5 = { "stackSize",        iocshArgInt };


/* Array of config args */
static const iocshArg * const EVTConfigArgs[] =
        { &EVTConfigArg0, &EVTConfigArg1, &EVTConfigArg2,
        &EVTConfigArg3, &EVTConfigArg4, &EVTConfigArg5 };


/* what function to call at config */
static void configEVTCallFunc(const iocshArgBuf *args) {
    ADEmergentVisionConfig(args[0].sval, args[1].sval, args[2].ival, args[3].ival,
            args[4].ival, args[5].ival);
}


/* information about the configuration function */
static const iocshFuncDef configEVT = { "ADEmergentVisionConfig", 5, EVTConfigArgs };


/* IOC register function */
static void EVTRegister(void) {
    iocshRegister(&configEVT, configEVTCallFunc);
}


/* external function for IOC register */
extern "C" {
    epicsExportRegistrar(EVTRegister);
}
