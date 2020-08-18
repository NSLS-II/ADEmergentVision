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
 * Function that reports error encountered in vendor library from EVT
 * 
 * @params[in]: status          -> error code
 * @params[in]: functionName    -> function in which error occured
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
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s No Cameras detected on the network\n", driverName, functionName);
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
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s Could not find camera with specified serial number\n", driverName, functionName);
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
            asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, "%s::%s Resolution: %d by %d\n", driverName, functionName, width_max, height_max);

            setIntegerParam(ADMaxSizeX, width_max);
            setIntegerParam(ADMaxSizeY, height_max);
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
    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s Collecting camera information\n", driverName, functionName);
    setStringParam(ADManufacturer, this->pdeviceInfo->manufacturerName);
    setStringParam(ADSerialNumber, this->pdeviceInfo->serialNumber);
    setStringParam(ADFirmwareVersion,this->pdeviceInfo->deviceVersion);
    setStringParam(ADModel, this->pdeviceInfo->modelName);
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
    EVT_CameraSetEnumParam(this->pcamera,   "AcquisitionMode",        "MultiFrame");
    EVT_CameraSetUInt32Param(this->pcamera, "AcquisitionFrameCount",  NUM_FRAMES);
    EVT_CameraSetEnumParam(this->pcamera,   "TriggerSelector",        "FrameStart");
    EVT_CameraSetEnumParam(this->pcamera,   "TriggerMode",            "On");
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
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s Image Thread Created\n", driverName, functionName);
        status = asynSuccess;
    }
    else{
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s Acquisition thread already active\n", driverName, functionName);
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
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s Image thread not active\n", driverName, functionName);
        status = asynError;
    }
    else{
        this->imageCollectionThreadActive = 0;
        asynPrint(this->pasynUserSelf, ASYN_TRACEIO_DRIVER, "%s::%s Stopping image acquisition thread\n", driverName, functionName);
    }
    return status;
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
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s Error: No camera connected\n", driverName, functionName);
        status = asynError;
    }
    else{
        status = setCameraValues();
        if(status != asynSuccess) asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s Error: setting camera values\n", driverName, functionName);
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
                setIntegerParam(ADStatus, ADStatusAcquire);
                asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s Image acquistion start\n", driverName, functionName);
                callParamCallbacks();
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
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s Stopping image acquisition thread\n", driverName, functionName);
        stopImageAcquisitionThread();
        // Make sure camera acquisition is completed before we close the stream.
        while(this->imageThreadOpen == 1)
            epicsThreadSleep(0.1);
        this->evt_status = EVT_CameraCloseStream(this->pcamera);
        if(this->evt_status != EVT_SUCCESS){
            reportEVTError(this->evt_status, functionName);
            status = asynError;
        }
        else status = asynSuccess;
    }
    setIntegerParam(ADStatus, ADStatusIdle);
    setIntegerParam(ADAcquire, 0);
    callParamCallbacks();
    asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, "%s::%s Stopped Image Acquisition\n", driverName, functionName);
    return status;
}


/**
 * Function that takes selected NDDataType and NDColorMode, and converts into an EVT pixel type
 * This is then used by the camera when starting image acquisiton
 * 
 * @params[out]: evtPixelType   -> pixel type of EVT image desired
 * @params[in]:  dataType       -> NDDataType selected in CSS
 * @params[in]:  colorMode      -> NDColorMode selected in CSS
 * @return: status              -> error if combination of dtype and color mode invalid
 */
asynStatus ADEmergentVision::getFrameFormatEVT(unsigned int* evtPixelType, NDDataType_t dataType, NDColorMode_t colorMode){
    const char* functionName = "getFrameFormatEVT";
    asynStatus status = asynSuccess;

    switch(colorMode){
        case NDColorModeMono:
            switch(dataType){
                case NDUInt8:
                    *evtPixelType = GVSP_PIX_MONO8;
                    break;
                case NDUInt16:
                    *evtPixelType = UNPACK_PIX_MONO16;
                    break;
                default:
                    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s Unsupported data type for this color mode\n", driverName, functionName);
                    return asynError;
            }
            break;
        case NDColorModeRGB1:
            switch(dataType){
                case NDUInt8:
                    *evtPixelType = GVSP_PIX_RGB8;
                    break;
                case NDUInt16:
                    *evtPixelType = UNPACK_PIX_RGB16;
                    break;
                default:
                    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s Unsupported data type for this color mode\n", driverName, functionName);
                    return asynError;
            }
            break;
        case NDColorModeBayer:
            switch(dataType){
                case NDUInt8:
                    *evtPixelType = CONVERT_PIX_BAYRG8;
                    break;
                case NDUInt16:
                    *evtPixelType = CONVERT_PIX_BAYRG16;
                    break;
                default:
                    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s Unsupported data type for this color mode\n", driverName, functionName);
                    return asynError;
            }
            break;
        default:
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s Error Not supported color format\n", driverName, functionName);
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
        case UNPACK_PIX_MONO16:
            *dataType = NDUInt16;
            *colorMode = NDColorModeMono;
            break;
        case UNPACK_PIX_RGB16:
            *dataType = NDUInt16;
            *colorMode = NDColorModeRGB1;
            break;
        case CONVERT_PIX_BAYRG8:
            *dataType = NDUInt8;
            *colorMode = NDColorModeBayer;
            break;
        case CONVERT_PIX_BAYRG16:
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
asynStatus ADEmergentVision::evtFrame2NDArray(CEmergentFrame* frame, NDArray** pArray){
    const char* functionName = "evtFrame2NDArray";
    asynStatus status = asynSuccess;
    size_t dims[3];
    int ndims;
    NDDataType_t dataType;
    NDColorMode_t colorMode;
    int xsize;
    int ysize;
    NDArrayInfo arrayInfo;
    status = getFrameFormatND(frame, &dataType, &colorMode);
    if(status == asynError){
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s Error computing dType and color mode\n", driverName, functionName);
        return asynError;
    }
    else{
        xsize = frame->size_x;
        ysize = frame->size_y;
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

        this->pArrays[0] = pNDArrayPool->alloc(ndims, dims, dataType, 0, NULL);
        if(this->pArrays[0]!=NULL) (*pArray) = this->pArrays[0];
        else{
            this->pArrays[0]->release();
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s Unable to allocate array\n", driverName, functionName);
            return asynError;
        }
        (*pArray)->getInfo(&arrayInfo);
        size_t total_size = arrayInfo.totalBytes;
        memcpy((unsigned char*)(*pArray)->pData, frame->imagePtr, total_size);
        (*pArray)->pAttributeList->add("ColorMode", "Color Mode", NDAttrInt32, &colorMode);
        int arrayCounter;
        getIntegerParam(NDArrayCounter, &arrayCounter);
        arrayCounter++;
        setIntegerParam(NDArrayCounter, arrayCounter);
        //refresh PVs
        callParamCallbacks();
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

    this->imageThreadOpen = 1;

    while(this->imageCollectionThreadActive == 1){
        NDArray* pArray;
        NDArrayInfo arrayInfo;
        CEmergentFrame frames[NUM_FRAMES];
        asynStatus status;
        int imageMode;
        int imageCounter;
        int xsize, ysize;
        unsigned int evtPixelType;
        int dataType;
        int colorMode;
        getIntegerParam(ADImageMode, &imageMode);
        getIntegerParam(ADNumImagesCounter, &imageCounter);
        getIntegerParam(ADSizeX, &xsize);
        getIntegerParam(ADSizeY, &ysize);
        getIntegerParam(NDDataType, &dataType);
        getIntegerParam(NDColorMode, &colorMode);
        asynStatus convert = getFrameFormatEVT(&evtPixelType, (NDDataType_t) dataType, (NDColorMode_t) colorMode);
        if(convert == asynError){
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s Error finding evt frame format\n", driverName, functionName);
        }
        else{
            asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, "%s::%s Arm detector\n", driverName, functionName);
            EVT_ERROR err = EVT_CameraExecuteCommand(this->pcamera, "AcquisitionStart");
            if(err != EVT_SUCCESS) reportEVTError(err, "EVT_CameraExecuteCommand");

            frames[0].size_x = xsize;
            frames[0].size_y = ysize;
            frames[0].pixel_type = (PIXEL_FORMAT) evtPixelType;

            asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, "%s::%s Arm detector\n", driverName, functionName);
            //printf("allocating frame buffer command\n");
            if(err == EVT_SUCCESS) err = EVT_AllocateFrameBuffer(this->pcamera, &frames[0], EVT_FRAME_BUFFER_ZERO_COPY);
            if(err != EVT_SUCCESS) reportEVTError(err, "EVT_AllocateFrameBuffer");
            
            asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, "%s::%s Queue camera frame\n", driverName, functionName);
            if(err == EVT_SUCCESS) err = EVT_CameraQueueFrame(this->pcamera, &frames[0]);
            if(err != EVT_SUCCESS) reportEVTError(err, "EVT_CameraQueueFrame");

            asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, "%s::%s Triggering w/ software\n", driverName, functionName);
            if(err == EVT_SUCCESS) err = EVT_CameraExecuteCommand(this->pcamera, "TriggerSoftware");
            if(err != EVT_SUCCESS) reportEVTError(err, "EVT_CameraExecuteCommand");

            asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, "%s::%s Grabbing frame\n", driverName, functionName);
            if(err == EVT_SUCCESS) err = EVT_CameraGetFrame(this->pcamera, &frames[0], EVT_INFINITE);
            if(err != EVT_SUCCESS) reportEVTError(err, "EVT_CameraGetFrame");

            asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, "%s::%s Stopping Acquisition\n", driverName, functionName);
            if(err == EVT_SUCCESS) err = EVT_CameraExecuteCommand(&camera, "AcquisitionStop");
            if(err != EVT_SUCCESS) reportEVTError(err, "EVT_CameraExecuteCommand");

            // Only process the frame if we successfully finished all of the above commands.
            if(err == EVT_SUCCESS){
                //printf("Converting to NDArray\n");
                status = evtFrame2NDArray(frames, &pArray);
                if(status == asynSuccess){
                    //printf("Converted to NDArray\n");
                    pArray->uniqueId = imageCounter;
                    updateTimeStamp(&pArray->epicsTS);
                    doCallbacksGenericPointer(pArray, NDArrayData, 0);
                    pArray->getInfo(&arrayInfo);
                    size_t total_size = arrayInfo.totalBytes;
                    setIntegerParam(NDArraySize, (int)total_size);
                    setIntegerParam(NDArraySizeX, arrayInfo.xSize);
                    setIntegerParam(NDArraySizeY, arrayInfo.ySize);

                    pArray->release();
                }

                //printf("RELEASING FRAME BUFFER\n");
                if(err == EVT_SUCCESS) err = EVT_ReleaseFrameBuffer(this->pcamera, &frames[0]);
                if(err != EVT_SUCCESS) reportEVTError(err, "EVT_ReleaseFrameBuffer");
                getIntegerParam(ADNumImagesCounter, &imageCounter);
                imageCounter++;
                setIntegerParam(ADNumImagesCounter, imageCounter);

                if(status == asynError){
                    this->imageThreadOpen = 0;
                    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s Error converting to NDArray\n", driverName, functionName);
                    acquireStop();
                    break;
                }
                if(imageMode == ADImageSingle){
                    this->imageThreadOpen = 0;
                    acquireStop();
                    asynPrint(this->pasynUserSelf, ASYN_TRACEIO_DRIVER, "%s::%s Done acquiring\n", driverName, functionName);
                }
                else if(imageMode == ADImageMultiple){
                    //printf("thinks it is multiple\n");
                    int numImages;
                    getIntegerParam(ADNumImages, &numImages);

                    if(imageCounter == numImages){
                        this->imageThreadOpen = 0;
                        acquireStop();
                        asynPrint(this->pasynUserSelf, ASYN_TRACEIO_DRIVER, "%s::%s Done\n", driverName, functionName);
                    }
                }
            }
        }
        //printf("done with first loop iteration\n");
    }
    this->imageThreadOpen = 0;
}


// -----------------------------------------------------------------------
// ADEmergentVision Camera Functions (Exposure, Format, Gain etc.)
// -----------------------------------------------------------------------


/* Getter and setter functions for setting the Framerate of the EVT Camera */


asynStatus ADEmergentVision::getEVTFramerate(unsigned int* framerate){
    const char* functionName = "getEVTFramerate";
    if(this->connected == 0) return asynError;
    asynStatus status = asynSuccess;
    this->evt_status = EVT_CameraGetUInt32Param(this->pcamera, "FrameRate", framerate);
    if(evt_status != EVT_SUCCESS){
        status = asynError;
        reportEVTError(evt_status, functionName);
    }
    return status;
}

asynStatus ADEmergentVision::setEVTFramerate(unsigned int framerate){
    const char* functionName = "setEVTFramerate";
    if(this->connected == 0) return asynError;
    asynStatus status = asynSuccess;
    this->evt_status = EVT_CameraSetUInt32Param(this->pcamera, "FrameRate", framerate);
    if(evt_status != EVT_SUCCESS){
        status = asynError;
        reportEVTError(evt_status, functionName);
    }
    else{
        printf("Set Framerate to %d/s\n", framerate);
    }
    return status;
}

/* Getter and setter functions for setting the Offset X of the EVT Camera */

asynStatus ADEmergentVision::getEVTOffsetX(unsigned int* offsetX){
    const char* functionName = "getEVTOffsetX";
    if(this->connected == 0) return asynError;
    asynStatus status = asynSuccess;
    this->evt_status = EVT_CameraGetUInt32Param(this->pcamera, "OffsetX", offsetX);
    if(evt_status != EVT_SUCCESS){
        status = asynError;
        reportEVTError(evt_status, functionName);
    }
    return status;
}

asynStatus ADEmergentVision::setEVTOffsetX(unsigned int offsetX){
    const char* functionName = "setEVTOffsetX";
    if(this->connected == 0) return asynError;
    asynStatus status = asynSuccess;
    this->evt_status = EVT_CameraSetUInt32Param(this->pcamera, "OffsetX", offsetX);
    if(evt_status != EVT_SUCCESS){
        status = asynError;
        reportEVTError(evt_status, functionName);
    }
    return status;
}

/* Getter and setter functions for setting the Offset Y of the EVT Camera */

asynStatus ADEmergentVision::getEVTOffsetY(unsigned int* offsetY){
    const char* functionName = "getEVTOffsetY";
    if(this->connected == 0) return asynError;
    asynStatus status = asynSuccess;
    this->evt_status = EVT_CameraGetUInt32Param(this->pcamera, "OffsetY", offsetY);
    if(evt_status != EVT_SUCCESS){
        status = asynError;
        reportEVTError(evt_status, functionName);
    }
    return status;
}

asynStatus ADEmergentVision::setEVTOffsetY(unsigned int offsetY){
    const char* functionName = "setEVTOffsetY";
    if(this->connected == 0) return asynError;
    asynStatus status = asynSuccess;
    this->evt_status = EVT_CameraSetUInt32Param(this->pcamera, "OffsetY", offsetY);
    if(evt_status != EVT_SUCCESS){
        status = asynError;
        reportEVTError(evt_status, functionName);
    }
    return status;
}

/* Getter and setter functions for setting the Packet size of the EVT Camera */

asynStatus ADEmergentVision::getEVTPacketSize(unsigned int* packetSize){
    const char* functionName = "getEVTPacketSize";
    if(this->connected == 0) return asynError;
    asynStatus status = asynSuccess;
    this->evt_status = EVT_CameraGetUInt32Param(this->pcamera, "GevSCPPacketSize", packetSize);
    if(evt_status != EVT_SUCCESS){
        status = asynError;
        reportEVTError(evt_status, functionName);
    }
    return status;
}

asynStatus ADEmergentVision::setEVTPacketSize(unsigned int packetSize){
    const char* functionName = "setEVTPacketSize";
    if(this->connected == 0) return asynError;
    asynStatus status = asynSuccess;
    this->evt_status = EVT_CameraSetUInt32Param(this->pcamera, "GevSCPPacketSize", packetSize);
    if(evt_status != EVT_SUCCESS){
        status = asynError;
        reportEVTError(evt_status, functionName);
    }
    return status;
}

/* Getter and setter functions for setting the Gain of the EVT Camera */

asynStatus ADEmergentVision::getEVTGain(unsigned int* gainValue){
    const char* functionName = "getEVTGain";
    if(this->connected == 0) return asynError;
    asynStatus status = asynSuccess;
    this->evt_status = EVT_CameraGetUInt32Param(this->pcamera, "Gain", gainValue);
    if(evt_status != EVT_SUCCESS){
        status = asynError;
        reportEVTError(evt_status, functionName);
    }
    return status;
}

asynStatus ADEmergentVision::setEVTGain(unsigned int gainValue){
    const char* functionName = "setEVTGain";
    if(this->connected == 0) return asynError;
    asynStatus status = asynSuccess;
    this->evt_status = EVT_CameraSetUInt32Param(this->pcamera, "Gain", gainValue);
    if(evt_status != EVT_SUCCESS){
        status = asynError;
        reportEVTError(evt_status, functionName);
    }
    return status;
}

/* Getter and setter functions for setting the Offset of the EVT Camera */

asynStatus ADEmergentVision::getEVTOffset(unsigned int* offset){
    const char* functionName = "getEVTOffset";
    if(this->connected == 0) return asynError;
    asynStatus status = asynSuccess;
    this->evt_status = EVT_CameraGetUInt32Param(this->pcamera, "Offset", offset);
    if(evt_status != EVT_SUCCESS){
        status = asynError;
        reportEVTError(evt_status, functionName);
    }
    return status;
}

asynStatus ADEmergentVision::setEVTOffset(unsigned int offset){
    const char* functionName = "setEVTOffset";
    if(this->connected == 0) return asynError;
    asynStatus status = asynSuccess;
    this->evt_status = EVT_CameraSetUInt32Param(this->pcamera, "Offset", offset);
    if(evt_status != EVT_SUCCESS){
        status = asynError;
        reportEVTError(evt_status, functionName);
    }
    return status;
}


/* Getter and setter functions for setting the Buffer size of the EVT Camera */

asynStatus ADEmergentVision::getEVTBufferNum(unsigned int* bufferNum){
    const char* functionName = "getEVTBufferNum";
    if(this->connected == 0) return asynError;
    asynStatus status = asynSuccess;
    this->evt_status = EVT_CameraGetUInt32Param(this->pcamera, "Offset", bufferNum);
    if(evt_status != EVT_SUCCESS){
        status = asynError;
        reportEVTError(evt_status, functionName);
    }
    return status;
}

asynStatus ADEmergentVision::setEVTBufferNum(unsigned int bufferNum){
    const char* functionName = "setEVTBufferNum";
    if(this->connected == 0) return asynError;
    asynStatus status = asynSuccess;
    this->evt_status = EVT_CameraSetUInt32Param(this->pcamera, "Offset", bufferNum);
    if(evt_status != EVT_SUCCESS){
        status = asynError;
        reportEVTError(evt_status, functionName);
    }
    return status;
}

/* Getter and setter functions for setting the LUT status of the EVT Camera */

asynStatus ADEmergentVision::getEVTLUTStatus(bool* lutValue){
    const char* functionName = "getEVTLUTStatus";
    if(this->connected == 0) return asynError;
    asynStatus status = asynSuccess;
    this->evt_status = EVT_CameraGetBoolParam(this->pcamera, "LUTEnable", lutValue);
    if(evt_status != EVT_SUCCESS){
        status = asynError;
        reportEVTError(evt_status, functionName);
    }
    return status;
}

asynStatus ADEmergentVision::setEVTLUTStatus(bool lutEnable){
    const char* functionName = "setEVTLUTStatus";
    if(this->connected == 0) return asynError;
    asynStatus status = asynSuccess;
    this->evt_status = EVT_CameraSetBoolParam(this->pcamera, "LUTEnable", lutEnable);
    if(evt_status != EVT_SUCCESS){
        status = asynError;
        reportEVTError(evt_status, functionName);
    }
    return status;
}

/* Getter and setter functions for setting the Autogain function of the EVT Camera */

asynStatus ADEmergentVision::getEVTAutoGain(bool* autoGainValue){
    const char* functionName = "getEVTAutoGain";
    if(this->connected == 0) return asynError;
    asynStatus status = asynSuccess;
    this->evt_status = EVT_CameraGetBoolParam(this->pcamera, "AutoGain", autoGainValue);
    if(evt_status != EVT_SUCCESS){
        status = asynError;
        reportEVTError(evt_status, functionName);
    }
    return status;
}

asynStatus ADEmergentVision::setEVTAutoGain(bool autoGainEnable){
    const char* functionName = "setEVTAutoGain";
    if(this->connected == 0) return asynError;
    asynStatus status = asynSuccess;
    this->evt_status = EVT_CameraSetBoolParam(this->pcamera, "AutoGain", autoGainEnable);
    if(evt_status != EVT_SUCCESS){
        status = asynError;
        reportEVTError(evt_status, functionName);
    }
    return status;
}


/* Getter and setter functions for setting the Buffer mode of the EVT Camera */

asynStatus ADEmergentVision::getEVTBufferMode(bool* bufferMode){
    const char* functionName = "getEVTBufferMode";
    if(this->connected == 0) return asynError;
    asynStatus status = asynSuccess;
    this->evt_status = EVT_CameraGetBoolParam(this->pcamera, "BufferMode", bufferMode);
    if(evt_status != EVT_SUCCESS){
        status = asynError;
        reportEVTError(evt_status, functionName);
    }
    return status;
}

asynStatus ADEmergentVision::setEVTBufferMode(bool bufferMode){
    const char* functionName = "setEVTBufferMode";
    if(this->connected == 0) return asynError;
    asynStatus status = asynSuccess;
    this->evt_status = EVT_CameraSetBoolParam(this->pcamera, "BufferMode", bufferMode);
    if(evt_status != EVT_SUCCESS){
        status = asynError;
        reportEVTError(evt_status, functionName);
    }
    return status;
}


/* Getter and setter functions for setting the Exposure of the EVT Camera */

asynStatus ADEmergentVision::getEVTExposureMax(unsigned int* maxExposure){
    const char* functionName = "getEVTExposureMax";
    if(this->connected == 0) return asynError;
    asynStatus status = asynSuccess;
    this->evt_status = EVT_CameraGetUInt32ParamMax(this->pcamera, "Exposure", maxExposure);
    if(evt_status != EVT_SUCCESS){
        status = asynError;
        reportEVTError(evt_status, functionName);
    }
    return status;
}

asynStatus ADEmergentVision::getEVTExposureMin(unsigned int* minExposure){
    const char* functionName = "getEVTExposureMin";
    if(this->connected == 0) return asynError;
    asynStatus status = asynSuccess;
    this->evt_status = EVT_CameraGetUInt32ParamMin(this->pcamera, "Exposure", minExposure);
    if(evt_status != EVT_SUCCESS){
        status = asynError;
        reportEVTError(evt_status, functionName);
    }
    return status;
}

asynStatus ADEmergentVision::getEVTExposureInc(unsigned int* incExposure){
    const char* functionName = "getEVTExposureInc";
    if(this->connected == 0) return asynError;
    asynStatus status = asynSuccess;
    this->evt_status = EVT_CameraGetUInt32ParamInc(this->pcamera, "Exposure", incExposure);
    if(evt_status != EVT_SUCCESS){
        status = asynError;
        reportEVTError(evt_status, functionName);
    }
    return status;
}


asynStatus ADEmergentVision::setEVTExposure(unsigned int exposure){
    const char* functionName = "setEVTExposure";
    if(this->connected == 0) return asynError;
    unsigned int maxExposure;
    unsigned int minExposure;
    asynStatus status;
    getEVTExposureMax(&maxExposure);
    getEVTExposureMin(&minExposure);
    if(exposure >= minExposure && exposure <= maxExposure){
        status = asynSuccess;
        this->evt_status = EVT_CameraSetUInt32Param(this->pcamera, "Exposure", exposure);
        if(evt_status != EVT_SUCCESS){
            status = asynError;
            reportEVTError(evt_status, functionName);
        }
        else{
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s Set exposure to %d microseconds\n", driverName, functionName, exposure);
        }
    }
    else {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s ERROR, Exposure time can be between %d and %d in microseconds.\n", driverName, functionName, minExposure, maxExposure);
        status = asynError;
    }
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
            else if(value == ADImageMultiple) setIntegerParam(ADNumImages, 100);
        }
        else if(function == ADEVT_Framerate) status = setEVTFramerate((unsigned int) value);
        else if(function == ADEVT_OffsetX) status = setEVTOffsetX((unsigned int) value);
        else if(function == ADEVT_OffsetY) status = setEVTOffsetY((unsigned int) value);
        else if(function == ADEVT_BufferNum) status = setEVTBufferNum((unsigned int) value);
        else if (function == ADEVT_LUTEnable) status = setEVTLUTStatus(value > 0);
        else if (function == ADEVT_AutoGain) status = setEVTAutoGain(value > 0);
        else if (function == ADEVT_BufferMode) status = setEVTBufferMode(value > 0);
        else if(function < ADEVT_FIRST_PARAM){
            status = ADDriver::writeInt32(pasynUser, value);
        }
    }
    callParamCallbacks();
    if(status == asynError) asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s ERROR status=%d, function=%d, value=%d\n", driverName, functionName, status, function, value);
    else asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s function=%d value=%d\n", driverName, functionName, function, value);
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
        status = setEVTExposure(exposureTime);
    }
    else if(function == ADGain) {
        unsigned int gain = (unsigned int) (value * 1000);
        status = setEVTGain(gain);
    }
    else if(function < ADEVT_FIRST_PARAM){
        status = ADDriver::writeFloat64(pasynUser, value);
    }
    callParamCallbacks();
    if(status == asynError) asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s ERROR status=%d, function=%d, value=%lf\n", driverName, functionName, status, function, value);
    else asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s function=%d value=%lf\n", driverName, functionName, function, value);
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
    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s reporting to external log file\n",driverName, functionName);
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
    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s ADEmergentVision Driver Exiting...\n", driverName, functionName);
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
        &EVTConfigArg3, &EVTConfigArg4, &EVTConfigArg5};


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
