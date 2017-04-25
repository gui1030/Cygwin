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
 *      Firmware Upload to Sensor From Hub Routines
 *
 *
 *
 *
 ******************************************************************************
 */

#include <stdio.h>
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

//------------------------------------------------------------------------------
//  External Public data objects
//------------------------------------------------------------------------------
extern uint8_t  packetSeqNum;

//------------------------------------------------------------------------------
//  Private function prototypes
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//  Private global data
//------------------------------------------------------------------------------
static uint32_t providedCRC;
static uint32_t imageByteLength;
static time_t   timeToBoot;
static time_t   lastDownloadChunkReceived = 0;
static bool     downloadInProgress = false;

//------------------------------------------------------------------------------
//  
//------------------------------------------------------------------------------
void FWDownloadFromHubBegin (
        struct FWB*         fromHub,
        enum SensorKind_t   sensorKind) {

    struct FWAFlags_t   returnedFlags = { false };
    const uint64_t      expectedPassword = 0xfeedab1e00118765;
    const int           maxImageBytes = 80*1024;
    const int           maxImageChunks = maxImageBytes/128;

    // Validate integrity of message from hub
    if (fromHub->MsgCksum !=
            cksum16(fromHub, sizeof(fromHub)-sizeof(fromHub->MsgCksum))) {
        returnedFlags.FailedCksumCheck = true;
    } 
    // Validate that we're the right sensor kind
    else if (fromHub->SensorKind != sensorKind) {
        returnedFlags.WrongSensorKind = true;
    }
    // Validate that password given by hub is correct
    else if (fromHub->Password != expectedPassword) {
        returnedFlags.InvalidPassword = true;
    }
    // Validate Image Length and Number of Chunks
    else if ((fromHub->ByteLength > maxImageBytes) ||
            (fromHub->NumChunks > maxImageChunks)) {
        returnedFlags.InvalidImageLength = true;
    }
    // Validate time is not too late
    else if (fromHub->BootTime <= time(NULL)) {
        returnedFlags.BoottimeTooEarly = true;
    }
    // Otherwise we assume we're good to proceed
    else {
        // Keep provided values until upload is complete
        downloadInProgress        = true;
        lastDownloadChunkReceived = time(NULL);
        providedCRC               = fromHub->CRC;
        imageByteLength           = fromHub->ByteLength;
        timeToBoot                = fromHub->BootTime;
    }

    PRINTF("%s: Flags returned to Hub are ", now_string());
    PRINTF("%d", returnedFlags.InvalidImageLength ? 1 : 0);
    PRINTF("%d", returnedFlags.InvalidPassword    ? 1 : 0);
    PRINTF("%d", returnedFlags.WrongSensorKind    ? 1 : 0);
    PRINTF("%d", returnedFlags.BoottimeTooEarly   ? 1 : 0);
    PRINTF("%d", returnedFlags.FailedCRCCheck     ? 1 : 0);
    PRINTF("%d", returnedFlags.FailedCksumCheck   ? 1 : 0);
    PRINTF("%d", returnedFlags.OutOfSequence      ? 1 : 0);
    PRINTF("%d", returnedFlags.LastMsgValid       ? 1 : 0);
    PRINTF("\n");
}

//------------------------------------------------------------------------------
//  
//------------------------------------------------------------------------------
void FWDownloadFromHubProcessChunk (
        struct FWC* fromHub) {
}

//------------------------------------------------------------------------------
//  
//------------------------------------------------------------------------------
bool FWDownloadFromHubInProgress () {
    return downloadInProgress;
}


//------------------------------------------------------------------------------
//  
//------------------------------------------------------------------------------
//
//      ext_flash_init();
//      ext_flash_open();
//      ext_flash_erase(0, 80*1024);
//      ext_flash_write(p, chunkSize, buf);
//      ext_flash_read(where, sizeof(buf), buf);
//      ext_flash_close();


//------------------------------------------------------------------------------
