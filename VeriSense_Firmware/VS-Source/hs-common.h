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
 *  hs-common.h
 *
 *  Common data structures and definitions between Sensors and the Hub
 *
 *
 ******************************************************************************
 */

#ifndef __HS_COMMON_H
#define __HS_COMMON_H

#include <contiki.h>
#include <net/ip/uip.h>


// Definitions of Sensor Unique Identifier
typedef struct {uint8_t ID[6]; } SensorID_t;

// Definitions of all Sensor Kinds
enum SensorKind_t {
    skNone = 0,     // Undefined sensor type (ie, empty)
    skHub  = 1,     // The VeriRadio Board in a hub
    skHT   = 2,     // Temperature/Humidity Sensor on a VeriRadio Board
};

// Strings Naming all Sensor Kinds
#define SensorKindString_Hub    "Hub"
#define SensorKindString_HT     "HT"

// Message Type Definitions for all Hub<==>Sensor Messages
enum SensorMsg_t {
    mtFWA = 'A',    // Sensor Firmware Upload Acknowledge
    mtFWB = 'B',    // Sensor Firmware Upload Begin
    mtFWC = 'C',    // Sensor Firmware Upload Chunk
    mtSDT = 0x15,   // Sensor Data Transfer
    mtSDA = 0x01,   // Sensor Data Acknowledge
};

// Flag Definitions for Firmware Upload Begin Messages
struct FWBFlags_t {
    bool                LastPacket          : 1;
    unsigned int                            : 7;
} __attribute__ ((packed)) ;

// Firmware Upload Begin Message Definition (Hub=>Sensor)
struct FWB {
    enum SensorMsg_t    MsgType    : 8; // offset 0
    uint8_t             SeqNum;         // offset 1
    enum SensorKind_t   SensorKind : 8; // offset 2
    struct FWBFlags_t   Flags;          // offset 3
    uint32_t            ByteLength;     // offset 4
    uint64_t            Password;       // offset 8
    uint32_t            BootTime;       // offset 16
    uint32_t            CRC;            // offset 20
    uint16_t            NumChunks;      // offset 24
    uint16_t            MsgCksum;       // offset 26
} __attribute__ ((packed)) ;            // length 28 bytes

// Flag Definitions for Sensor=>Hub Acknowlegements
struct FWAFlags_t {
    bool                LastMsgValid        : 1;
    bool                OutOfSequence       : 1;
    bool                FailedCksumCheck    : 1;
    bool                FailedCRCCheck      : 1;
    bool                BoottimeTooEarly    : 1;
    bool                WrongSensorKind     : 1;
    bool                InvalidPassword     : 1;
    bool                InvalidImageLength  : 1;
} __attribute__ ((packed)) ;

// Firmware Message Acknowledgement Message (Sensor=>Hub)
struct FWA {
    enum SensorMsg_t    MsgType  : 8;   // offset 0
    uint8_t             SeqNum;         // offset 1
    struct FWAFlags_t   Flags;          // offset 2
    uint8_t             pad;            // offset 3
    uint16_t            ChunkNum;       // offset 4
    uint16_t            MsgCksum;       // offset 6
} __attribute__ ((packed)) ;            // length 8 bytes

// Firmware Upload Message Chunk Definition (Hub=>Sensor)
struct FWC {
    enum SensorMsg_t    MsgType : 8;    // offset 0
    uint8_t             SeqNum;         // offset 1
    uint16_t            ChunkNum;       // offset 2
    uint32_t            ByteOffset;     // offset 4
    uint16_t            MsgCksum;       // offset 8
    uint8_t             BytesThisChunk; // offset 10
    uint8_t             pad;            // offset 11
    uint8_t             Contents[244];  // offset 12
} __attribute__ ((packed)) ;            // length (upto) 256 bytes

//
#define UDP_CLIENT_PORT 8765    // These are not IANA assigned!!
#define UDP_SERVER_PORT 5678

//
struct client_t {
    enum SensorKind_t   kind;
    uip_ipaddr_t        ip;
    char                mac[16];
    int                 seqNum;
    clock_time_t        lastReportTime;
};

//
struct vsMsgData_t {
    char            mac[13];
    uint8_t         index;
    signed short    temp;
    uint8_t         rssi;
    uint8_t         humidity;
    uint8_t         battery;
    uint8_t         parent_mac[6];
};

struct _sensor { 
    signed short temp;
    uint8_t      humidity;
    uint8_t      battery;
};

#endif  /* __HS_COMMON_H */
