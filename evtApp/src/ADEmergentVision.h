/**
 * Header file for the ADEmergent Vision EPICS driver
 * 
 * This file contains the definitions of PV params and the declaration of the ADEmergentVision class and function
 * 
 * 
 * Author: Jakub Wlodek
 * Created On: December-7-2018
 * 
 * Copyright (c) : 2018 Brookhaven National Laboratory
 * 
 */

// header guard
#ifndef ADEMERGENTVISION_H
#define ADEMERGENTVISION_H

// version numbers
#define ADEMERGENTVISION_VERSION        0
#define ADEMERGENTVISION_REVISION       0
#define ADEMERGENTVISION_MODIFICATION   3





// PV Definitions

#define ADEVT_PixelFormatString             "EVT_PIXEL_FORMAT"          //asynParamInt32
#define ADEVT_FramerateString               "EVT_FRAMERATE"             //asynParamInt32
#define ADEVT_OffsetXString                 "EVT_OFFX"                  //asynParamInt32
#define ADEVT_OffsetYString                 "EVT_OFFY"                  //asynParamInt32
#define ADEVT_BufferModeString              "EVT_BUFF_MODE"             //asynParamInt32
#define ADEVT_BufferNumString               "EVT_BUFF_NUM"              //asynParamInt32
#define ADEVT_PacketSizeString              "EVT_PACKET"                //asynParamInt32
#define ADEVT_LUTEnableString               "EVT_LUT"                   //asynParamInt32
#define ADEVT_AutoGainString                "EVT_AUTOGAIN"              //asynParamInt32
#define ADEVT_GpiStartModeString            "EVT_GPI_START_MODE"        //asynParamInt32
#define ADEVT_TriggerDelayString            "EVT_TRIGGER_DELAY"         //asynParamInt32
#define ADEVT_TgHighTimeString              "EVT_TG_HIGH_TIME"          //asynParamInt32
#define ADEVT_TgFrameTimeString             "EVT_TG_FRAME_TIME"         //asynParamInt32
#define ADEVT_GpiEndEventString             "EVT_GPI_END_EVENT"         //asynParamInt32
#define ADEVT_GpiEndModeString              "EVT_GPI_END_MODE"          //asynParamInt32
#define ADEVT_GpiStartEventString           "EVT_GPI_START_EVENT"       //asynParamInt32

#define SUPPORTED_MODE_BUFFER_SIZE 1000


// includes
#include <EmergentCameraAPIs.h>
#include <EvtParamAttribute.h>
#include <gigevisiondeviceinfo.h>
#include <emergentcameradef.h>
#include <thread>
#include "ADDriver.h"

using namespace std;
using namespace Emergent;




class ADEmergentVision : ADDriver {

    public:

        // constructor
        ADEmergentVision(const char* portName, const char* serialNumber, int maxBuffers, size_t maxMemory, int priority, int stackSize);

        // ADDriver overrides
        virtual asynStatus writeInt32(asynUser* pasynUser, epicsInt32 value);
        virtual asynStatus writeFloat64(asynUser* pasynUser, epicsFloat64 value);
        virtual asynStatus connect(asynUser* pasynUser);
        virtual asynStatus disconnect(asynUser* pasynUser);

        // destructor
        ~ADEmergentVision();

    protected:

        // PV indexes
        int ADEVT_PixelFormat;
        #define ADEVT_FIRST_PARAM   ADEVT_PixelFormat
        int ADEVT_Framerate;
        int ADEVT_OffsetX;
        int ADEVT_OffsetY;
        int ADEVT_BufferMode;
        int ADEVT_BufferNum;
        int ADEVT_PacketSize;
        int ADEVT_LUTEnable;
        int ADEVT_AutoGain;
        int ADEVT_GpiStartMode;
        int ADEVT_GpiStartEvent;
        int ADEVT_GpiEndMode;
        int ADEVT_GpiEndEvent;
        int ADEVT_TgFrameTime;
        int ADEVT_TgHighTime;
        int ADEVT_TriggerDelay;
        #define ADEVT_LAST_PARAM ADEVT_TriggerDelay

    private:

    // ----------------------------
    // EVT variables
    // ----------------------------

    EVT_ERROR evt_status;
    CEmergentCamera camera;
    CEmergentCamera* pcamera = &camera;
    struct GigEVisionDeviceInfo* pdeviceInfo;

    int withShutter = 0;

    // Image thread 
    int imageCollectionThreadActive = 0;
    int imageThreadOpen = 0;


    const char* serialNumber;
    int connected = 0;

    //EVT Camera supported modes
    unsigned long supportedModeSizeReturn = 0;
    char supportedModes[SUPPORTED_MODE_BUFFER_SIZE];

    // ----------------------------
    // EVT Functions for logging/reporting
    // ----------------------------

    asynStatus getDeviceInformation();
    void updateStatus(const char* status);
    static void exitCallback(void* pEVT);
    void report(FILE* fp, int details);
    void reportEVTError(EVT_ERROR status, const char* functionName);
    void printConnectedDeviceInfo();

    // ---------------------------
    // EVT Functions for connecting to camera
    // ---------------------------

    asynStatus connectToDeviceEVT();
    asynStatus disconnectFromDeviceEVT();
    asynStatus collectCameraInformation();

    // -----------------------------
    // EVT Camera Functions
    // -----------------------------

    bool isEVTInt32ParamValid(unsigned int newVal, const char* param);
    asynStatus getEVTInt32Param(unsigned int* retVal, const char* param);
    asynStatus setEVTInt32Param(unsigned int newVal, const char* param);

    asynStatus getEVTBoolParam(bool* retVal, const char* param);
    asynStatus setEVTBoolParam(bool newVal, const char* param);


    // -----------------------------
    // EVT Image acquisition functions
    // -----------------------------

    asynStatus setDefaultCameraValues();
    string getSupportedFormatStr(PIXEL_FORMAT evtPixelFormat);
    bool isFrameFormatValid(const char* formatStr);
    asynStatus getFrameFormatEVT(unsigned int* evtPixelType);
    asynStatus getConvertFormatEVT(unsigned int* evtPixelType, NDDataType_t dataType, NDColorMode_t colorMode);
    asynStatus getFrameFormatND(CEmergentFrame* frame, NDDataType_t* dataType, NDColorMode_t* colorMode);
    asynStatus evtFrame2NDArray(CEmergentFrame* frame, CEmergentFrame* convertFrame, NDArray** pArray);
    unsigned int getConvertBitDepth(PIXEL_FORMAT evtPixelFormat);
    
    void evtCallback();
    static void* evtCallbackWrapper(void* pPtr);


    asynStatus acquireStart();
    asynStatus acquireStop();

    asynStatus startImageAcquisitionThread();
    asynStatus stopImageAcquisitionThread();
    

};

#define NUM_EVT_PARAMS ((int) (&ADEVT_LAST_PARAM - &ADEVT_FIRST_PARAM+1))


#endif