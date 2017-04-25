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
 *      TEMPERATURE & HUMIDITY SENSOR FIRMWARE -
 *
 *
 *
 *
 ******************************************************************************
 */

#include "contiki.h"
#include "lib/random.h"
#include "sys/clock.h"
#include "net/ip/uip.h"
#include "net/ipv6/uip-ds6.h"
#include "net/ip/uip-udp-packet.h"
#include "sys/ctimer.h"
#include "lib/memb.h" 
#include "hdc-1000-sensor.h"
//RSSI
#include "net/ipv6/sicslowpan.h"
#include "uip-icmp6.h"
 //SensorTag
#include "ti-lib.h"
#include "batmon-sensor.h"
#include "board-peripherals.h"
//MAC Address
#include "ieee-addr.h"
#include "rpl.h"
#include "net/rpl/rpl.h"
#include "net/rpl/rpl-private.h"
#include <time.h>
#include "util.h"
// Common Hub-Sensor Messages
#include "hs-common.h"
#include "sensor-fw-download.h"

#define DEBUG DEBUG_NONE
#include "net/ip/uip-debug.h"

#undef PRINTF
#define PRINTF(...) printf(__VA_ARGS__)

//Prototype
static void             sendVSpacket();
static void             init_sensor_readings();

static struct uip_udp_conn *client_conn;
static uip_ipaddr_t     server_ipaddr;

static uint8_t          sensor_data[14];
static struct _sensor   sensor_reads;
static uint8_t          sensorMACAddress[8];

static struct ctimer    timerWaitForAck;        //Timeout for retransmission
static struct ctimer    timerInitSensorReads;   //Timer to get temp/humidity/battery
static struct etimer    timerRetrySensorRead;   //for getting new values f/hum sensor

static int              send_attempts = 0;   //Used for resends directly after noack
static int              read_attempts = 0;
static int              prevRSSIValue = 0;

//------------------------------------------------------------------------------
//  Externally visable local objects
//------------------------------------------------------------------------------
uint8_t         packetSeqNum = 0;

/*---------------------------------------------------------------------------*/
PROCESS(udp_client_process, "UDP client process");
AUTOSTART_PROCESSES(&udp_client_process);
/*---------------------------------------------------------------------------*/

//******************************************************************************

static void get_battery_reading() {

    short battery = batmon_sensor.value(BATMON_SENSOR_TYPE_VOLT);
    int   realBattery = (battery * 125) >> 5;
    sensor_reads.battery = (uint8_t)(realBattery/100);
}

//******************************************************************************
/*
 * Sensor deactivates itself when read is finished
 */
static int get_hdc_reading() {

    int         value = 0;
    const int   maxReadAttempts = 2;

    value = hdc_1000_sensor.value(HDC_1000_SENSOR_TYPE_TEMP);
    if (value != CC26XX_SENSOR_READING_ERROR && value > -4000) {
        sensor_reads.temp = (signed short)value;
    }
    else if (read_attempts < maxReadAttempts) {
  	    sensor_reads.temp = 12100;
  	    read_attempts++;
  	    return 0;
    }

    value = hdc_1000_sensor.value(HDC_1000_SENSOR_TYPE_HUMIDITY);
    if (value != CC26XX_SENSOR_READING_ERROR) {
        sensor_reads.humidity = ((uint8_t)((value+50)/100));
    } 
    read_attempts = 0;
    return 1;
}

//******************************************************************************
/*
 *Turn Sensors on
 */
static void init_sensors(void) {

#if BOARD_SENSORTAG	
    SENSORS_ACTIVATE(reed_relay_sensor);
#endif
    SENSORS_ACTIVATE(batmon_sensor);
}

//******************************************************************************
/**
 * Increment sequence number and enable humidity sensor
 */
static void init_sensor_readings() {

    ctimer_set(&timerInitSensorReads, SENSOR_COMM_WITH_HUB_INTERVAL,
            init_sensor_readings, NULL);
    memset(sensor_data, 0, sizeof(sensor_data));
    packetSeqNum = (packetSeqNum + 1) % 8;      // was modulus 255
    SENSORS_ACTIVATE(hdc_1000_sensor);
}

//******************************************************************************

static void tcpip_handler(void) {

    if (uip_newdata()) {
        uint8_t* data = uip_appdata;
        if ((enum SensorMsg_t)data[0] == mtSDA && data[1]==packetSeqNum) {
            PRINTF("Ack from Hub (Seqnum=%d)\n", data[1]);
            ctimer_stop(&timerWaitForAck);
            send_attempts = 0;
            prevRSSIValue = sicslowpan_get_last_rssi();
        }
        else if ((enum SensorMsg_t)data[0] == mtSDA && data[1]!=packetSeqNum) 
            PRINTF("==> Out of Sequence Ack from Server (Seqnum=%d)\n",
                    data[1]);
        else if ((enum SensorMsg_t)data[0] == mtFWB) {
            PRINTF("FW Download Begin Message Received\n");
            FWDownloadFromHubBegin((struct FWB*)data, skHT);
        }
        else if ((enum SensorMsg_t)data[0] == mtFWC) {
            FWDownloadFromHubProcessChunk((struct FWC*)data);
        }
        else 
            PRINTF("==> Unknown Packet Type from Server (0x%2.2X:0x%2.2X)\n",
                    data[0], data[1]);
    }
}

//******************************************************************************
/**
  * Resend payload until Max Attempts or until we get an ACK
  */
static void retryVSPacket() {
    PRINTF("==> No ACK within Timeout.  ");
    if (send_attempts < SENSOR_VS_MAX_RETRANSMITS) {
        send_attempts++;
        PRINTF("Retrying...(%d)\n", send_attempts);
 	    ctimer_set(&timerWaitForAck, SENSOR_VS_MAX_WAITING_FOR_ACK,
                retryVSPacket, NULL);
    }
    else {
        PRINTF("Retries Exhausted\n\n");
        send_attempts = 0;
    }
}

//******************************************************************************

static void sendVSpacket() { 

    //Get Parent MAC if non-null parent
    uint8_t         parentMAC[6];
    if(uip_ds6_defrt_choose() != NULL)
        memcpy(parentMAC, uip_ds6_defrt_choose()->u8+10, sizeof(parentMAC));

    // Fill send structure
    sensor_data[0] = (uint8_t)mtSDT;
    sensor_data[1] = packetSeqNum;
    sensor_data[2] = (uint8_t)((sensor_reads.temp >> 8) & 0xFF);
    sensor_data[3] = (uint8_t)(sensor_reads.temp & 0xFF);
    sensor_data[4] = (uint8_t)sensor_reads.humidity;
    sensor_data[5] = (uint8_t)sensor_reads.battery;
    sensor_data[6] = (uint8_t)(abs(prevRSSIValue));
    for(int i = 0; i<6; i++)
        sensor_data[i+8] = parentMAC[i];

    // Output debug data
    PRINTF("VS Pkt to Hub: Seqnum=%d Temp=%d Hum=%d Batt=%d RSSI=%d PMAC=",
            packetSeqNum, sensor_reads.temp, sensor_reads.humidity,
            sensor_reads.battery, sensor_data[6]);
    uint8_t *oM = sensor_data+8;
    PRINTF("%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X\n", 
            oM[0], oM[1], oM[2], oM[3], oM[4], oM[5]);

    // Send out packet
    uip_udp_packet_sendto(client_conn, sensor_data, sizeof(sensor_data),
             &server_ipaddr, UIP_HTONS(UDP_SERVER_PORT));
    ctimer_set(&timerWaitForAck, SENSOR_VS_MAX_WAITING_FOR_ACK,
            retryVSPacket, NULL);
}

//******************************************************************************

static void set_global_address(void) {

    uip_ipaddr_t ipaddr;

    uip_ip6addr(&ipaddr, UIP_DS6_DEFAULT_PREFIX, 0, 0, 0, 0, 0, 0, 0);
    uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
    uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);

#if 0
    /* Mode 1 - 64 bits inline */
    uip_ip6addr(&server_ipaddr, UIP_DS6_DEFAULT_PREFIX,
            0, 0, 0, 0, 0, 0, 1);
#elif 1
    /* Mode 2 - 16 bits inline */
    uip_ip6addr(&server_ipaddr, UIP_DS6_DEFAULT_PREFIX,
            0, 0, 0, 0, 0x00ff, 0xfe00, 1);
#else
    /* Mode 3 - derived from server link-local (MAC) address */
    uip_ip6addr(&server_ipaddr, UIP_DS6_DEFAULT_PREFIX,
            0, 0, 0, 0x0250, 0xc2ff, 0xfea8, 0xcd1a);
#endif
}

//******************************************************************************

static void print_local_addresses(void) {	

    uint8_t state;

    PRINTF("Client IPv6 addresses:\n");
    for (int i = 0; i < UIP_DS6_ADDR_NB; i++) {
        state = uip_ds6_if.addr_list[i].state;
        if (uip_ds6_if.addr_list[i].isused &&
                (state == ADDR_TENTATIVE || state == ADDR_PREFERRED)) {
            uint8_t *u = uip_ds6_if.addr_list[i].ipaddr.u8;
            PRINTF("   [");
            for (int c=0; c<16; c+=2)
                PRINTF("%.2X%.2X", u[c], u[c+1]);
            PRINTF("]\n");

            /* hack to make address "final" */
            if (state == ADDR_TENTATIVE)
                uip_ds6_if.addr_list[i].state = ADDR_PREFERRED;
        }
    }
}

//******************************************************************************

static void perform_self_test() {

    PRINTF("Humidity Self Test: ");
    if (self_test_humidity()) PRINTF("Works\n");
    else                      PRINTF("Fails\n");

    PRINTF("Ext Flash Self Test: ");
    if (ext_flash_test())     PRINTF("Works\n");
    else                      PRINTF("Fails\n");
}

//******************************************************************************

caddr_t _sbrk (int incr) {
    static long             block[256];
    static unsigned char    *heap = NULL;
    unsigned char           *prevHeap;

    if (heap == NULL) 
        heap = (unsigned char *)&block;
    prevHeap = heap;
    heap += incr;
    return ((heap-((unsigned char*)&block)) <= sizeof(block)) ?
        (caddr_t)prevHeap : NULL;
}

//******************************************************************************

PROCESS_THREAD(udp_client_process, ev, data) {

    PROCESS_BEGIN();
    PROCESS_PAUSE();    // why is this here?
  
    PRINTF("\n\n** Welcome to VeriSolutions Sensor %s **\n",
            SensorKindString_HT);
    // Comm with Server on channel Range 11-26 f/ IEEE 802.15.4
    PRINTF("Operating on RF Channel %d\n", RF_CORE_CONF_CHANNEL);
    PRINTF("Maximum Clients Preset is %d\n", MAX_VS_CLIENTS);
    PRINTF("Security Level is %d\n", NONCORESEC_CONF_SEC_LVL);
    PRINTF("Initial Sensor Data Transmission in %d.%.2d minutes\n",
            SENSOR_INITIAL_COMM_WITH_HUB/(CLOCK_SECOND*60),
            (((SENSOR_INITIAL_COMM_WITH_HUB/CLOCK_SECOND)*100)/60)%100);
    PRINTF("Sensor Data Transmission Rate is every %d.%.2d minutes\n",
            SENSOR_COMM_WITH_HUB_INTERVAL/(CLOCK_SECOND*60),
            (((SENSOR_COMM_WITH_HUB_INTERVAL/CLOCK_SECOND)*100)/60)%100);
    PRINTF("Max Sensor Data Retransmits set to %d\n",
            SENSOR_VS_MAX_RETRANSMITS);

    uint8_t *m = sensorMACAddress;
    ieee_addr_cpy_to(sensorMACAddress, sizeof(sensorMACAddress));
    PRINTF("Sensor MAC Address=%.2X%.2X%.2X%.2X%.2X%.2X%.2X%.2X\n",
                m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7]);

    set_global_address();
    print_local_addresses();

    /* new connection with remote host */
    client_conn = udp_new(NULL, UIP_HTONS(UDP_SERVER_PORT), NULL); 
    if (client_conn == NULL) {
        PRINTF("===>No UDP connection available\n");
        PRINTF("===>Exiting\n");
        PROCESS_EXIT();
    }
    udp_bind(client_conn, UIP_HTONS(UDP_CLIENT_PORT)); 
    perform_self_test();
    PRINTF("Created Connection with Server: ");

#if BOARD_SENSORTAG	
    SENSORS_DEACTIVATE(mpu_9250_sensor);
#endif

    uint16_t *u = client_conn->ripaddr.u16;
    PRINTF("[%.4X:%.4X:%.4X:%.4X:%.4X:%.4X:%.4X:%.4X]\n",
            u[0], u[1], u[2], u[3], u[4], u[5], u[6], u[7]);

    PRINTF("\nLocal/Remote Port %u/%u\n",
	UIP_HTONS(client_conn->lport), UIP_HTONS(client_conn->rport));
    init_sensors();
    ctimer_set(&timerInitSensorReads, SENSOR_INITIAL_COMM_WITH_HUB,
            init_sensor_readings, NULL);

    while (true) {
        PROCESS_YIELD();    // wait for an event to happen

        PRINTF("%s  Type=", now_string());

        if(ev == tcpip_event) {
            PRINTF("TCP/IP: ");
            tcpip_handler();
        }
        else if (ev == sensors_event && data == &hdc_1000_sensor) {
            PRINTF("SENSOR EVENT:  ");
            int good = get_hdc_reading();
            if (good) {
      	        get_battery_reading();
      	        sendVSpacket();  // send VeriSense packet to Hub
            }
            else {
      	        PRINTF("Bad Sensor Read\n");
      	        etimer_set(&timerRetrySensorRead,
                        SENSOR_RETRY_SENSOR_READ_INTERVAL);
            }
        }
        else if (etimer_expired(&timerRetrySensorRead)) {
            PRINTF("TIMER EXPIRATION\n");
    	    SENSORS_ACTIVATE(hdc_1000_sensor);
        }
        else
            PRINTF("Unknown (ev=%X)\n", ev);
    }

    PROCESS_END();
}

/*---------------------------------------------------------------------------*/
