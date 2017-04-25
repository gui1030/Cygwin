/*
 ******************************************************************************
 *
 *   This File Contains PROPRIETARY and CONFIDENTIAL TRADE SECRET information
 *   which is the exclusive property of VeriSolutions, LLC.  "Trade Secret"
 *   means information constituting a trade secret within the meaning of
 *   Section 10-1-761(4) of the Georgia Trade Secrets Act of 1990, including
 *   all amendments hereafter adopted.
 *
 *   (c) 2017 by VeriSolutions, LLC.                 www.VeriSolutions.co
 *
 ******************************************************************************
 *
 *      Firmware Upload From Host and To Sensor Routines
 *
 *
 *
 *
 ******************************************************************************
 */

#include <time.h>
#include "hs-common.h"
#include "util.h"

//------------------------------------------------------------------------------
//  Debug behavior functionality
//------------------------------------------------------------------------------
#undef PRINTF
#define PRINTF(...) printf(__VA_ARGS__)

//------------------------------------------------------------------------------
//  External Public function prototypes
//------------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//
//  Here defines the structure of a replacement firmware image kept
//  in External Flash
//
//
struct FWImageSegment {
    uint32_t            ByteLength;
    uint32_t            SegmentCRC;
    uint8_t             Contents[4];
};

struct FWImageHeader_t {
    enum SensorKind_t   SensorKind;
    time_t              Boottime;
    uint64_t            Password;
    uint32_t            ByteLength;
};

//------------------------------------------------------------------------------
//  Sensor passwords by Sensor Kind
//------------------------------------------------------------------------------
static const uint64_t   sensorPassword[3] = { // correspond to enum SensorKind_t
        0,                          // skNone
        0xED1B1EF00d275391,         // skHub
        0xFEEDAB1E00118765,         // skHT
};


//------------------------------------------------------------------------------
//  
//------------------------------------------------------------------------------
void HubToSensorFWUploadBegin (
        SensorID_t* sensor,
        struct FWB* toSensor) {



}

//------------------------------------------------------------------------------
//  
//------------------------------------------------------------------------------
void HubToSensorFWUploadProcessChunk (
        SensorID_t* sensor,
        struct FWC* toSensor) {
}

