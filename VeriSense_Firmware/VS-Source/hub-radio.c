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
 *      VERIRADIO HUB FIRMWARE -
 *
 *   Consists of two threads, udp_server_process and processCommWithElectron
 *
 *
 *
 ******************************************************************************
 */

#include "project-conf.h"
#include "contiki.h"
#include "contiki-net.h"
#include "net/netstack.h"
#include "net/ip/uip.h"
#include "net/ip/uip-debug.h"
#include "net/rpl/rpl.h"
#include "sys/process.h"
#include "dev/serial-line.h"
#include "dev/cc26xx-uart.h"
#include "ieee-addr.h"
#include "board-peripherals.h"
#include "dev/leds.h"
#include "ext-flash.h"
#include "hs-common.h"
#include "util.h"
#include <time.h>

#define DEBUG DEBUG_NONE

#undef PRINTF
#define PRINTF(...) printf(__VA_ARGS__)

//-----------------------------------------------------------------------------
//
//  These define various process events 
//
static process_event_t  eventSensorReading;
static process_event_t  eventSelfTest;


//-----------------------------------------------------------------------------
//
//  These are global data used by more than one function
//
static struct client_t      netClient[MAX_VS_CLIENTS];
static struct vsMsgData_t   vsMsgData[MAX_VS_CLIENTS];
static bool                 humSensorOkay, flashMemOkay;
static uint8_t              ourMACAddress[8];
static struct uip_udp_conn  *server_conn;
static struct ctimer        timerSendSensorPacket;

#define UIP_IP_BUF   ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])


//-----------------------------------------------------------------------------
//
//  These are prototypes for functions elaborated toward the bottom
//
static char                 *ipaddrToChar(const uint8_t*);


//-----------------------------------------------------------------------------
//
//  These define the two concurrent threads operating
//
PROCESS_NAME(udp_server_process);
PROCESS(udp_server_process, "UDP Server");
AUTOSTART_PROCESSES(&udp_server_process);

PROCESS_NAME(processCommWithElectron);
PROCESS(processCommWithElectron, "Electron Communication");


//*****************************************************************************

//-----------------------------------------------------------------------------
//
//  
//
static int getClientIndex (enum SensorKind_t kind, uip_ipaddr_t *ip, char *mac) {

    int thisClient;
    // Determine if we've seen this sensor before & add it to sensor list
    for (thisClient=0; thisClient<MAX_VS_CLIENTS; thisClient++) {
        // is this client slot empty?
        if (netClient[thisClient].kind == skNone) {
            // Add new client
            netClient[thisClient].kind = kind;
            strcpy(netClient[thisClient].mac, mac);
            uip_ipaddr_copy(&netClient[thisClient].ip, ip);
            PRINTF("===> Client (#%d) Added. MAC=%s\n", thisClient, mac);
            break;  // use this_client as our client number
        }
        // is this slot used by messages's client?
        else if (strcmp(mac, netClient[thisClient].mac) == 0)
            break;  //  use this_client as our client number
    }
    if (thisClient >= MAX_VS_CLIENTS) {    // no room for new client
        PRINTF("\n===> Only %d Clients Permitted. Sensor at MAC=%s ignored\n",
                MAX_VS_CLIENTS, mac);
        thisClient = -1;  // indicates no room for client
    }
    return thisClient;
}

//-----------------------------------------------------------------------------
//
//  Place incoming Sensor data packet in queue for transmission to Electron
//      Returns: Index into netClient list
//  
static int queueVSPacket(char *mac, uip_ipaddr_t *ipAddr,
        uint8_t *sensor_data) {

    int thisSensor = getClientIndex(skHT, ipAddr, mac);
    if (thisSensor >= 0) {
        clock_time_t        clock_time();
        struct vsMsgData_t  electronMsg;
        bool                skipSeq = false;
        int                 expectingSeq;

        clock_time_t prev_time = netClient[thisSensor].lastReportTime;
        // is this a duplicate message?
        if (sensor_data[1] == netClient[thisSensor].seqNum) {
            PRINTF("Duplicate Msg Ignored (seqNum=%d)\n", sensor_data[1]);
            return thisSensor;  // ignore if so
        }
        // New VeriSense Message
        if (((netClient[thisSensor].seqNum+1)%8) != sensor_data[1]) {
            skipSeq = true;     // if we somehow skipped a message
            expectingSeq = netClient[thisSensor].seqNum+1;
        }
        netClient[thisSensor].seqNum = sensor_data[1];
        netClient[thisSensor].lastReportTime = clock_time();

        strcpy(electronMsg.mac, mac);
        electronMsg.temp     = (sensor_data[2] << 8) | sensor_data[3];
        electronMsg.humidity = sensor_data[4];
        electronMsg.battery  = sensor_data[5];
        electronMsg.index    = thisSensor;
        electronMsg.rssi     = sensor_data[6];
        for (int k=0; k<6; k++) 
            electronMsg.parent_mac[k] = sensor_data[k+8];
 
        long deltaT = netClient[thisSensor].lastReportTime - prev_time;
        PRINTF("Seqnum=%d Temp=%d Hum=%d Batt=%d RSSI=%d",
                sensor_data[1], electronMsg.temp, electronMsg.humidity,
                electronMsg.battery, electronMsg.rssi);
        PRINTF(" DeltaT=%ld.%.2lds PMAC=",
                deltaT/CLOCK_SECOND, ((deltaT*100)/CLOCK_SECOND)%100);
        uint8_t *m = electronMsg.parent_mac;
        PRINTF("%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X\n", 
                m[0], m[1], m[2], m[3], m[4], m[5]);
        if (skipSeq)
            PRINTF("===>This Message Out of Sequence!! (Should be %d)\n",
                    expectingSeq);
        process_post_synch(&processCommWithElectron,
                eventSensorReading, &electronMsg);
    }
    return thisSensor;
}

//*****************************************************************************

static void tcpip_handler(void) {

    if (uip_newdata()) {
        uint8_t* data = uip_appdata;

        if (data[0] == mtSDT) {
            int     clientNum;
            int     localSeqNum = data[1];
            PRINTF("Sensor: ");
            clientNum = queueVSPacket(ipaddrToChar((&UIP_IP_BUF->srcipaddr)->u8+10),
                    &UIP_IP_BUF->srcipaddr, (uint8_t*)uip_appdata);
            // Acknowledge message
            uint8_t toSend[2];
            toSend[0] = mtSDA;
            toSend[1] = localSeqNum;
            uip_ipaddr_copy(&server_conn->ripaddr, &UIP_IP_BUF->srcipaddr);
            uip_udp_packet_send(server_conn, toSend, sizeof(toSend));
            PRINTF("%s  To Sensor: Ack %d (Seqnum=%d)\n", now_string(),
                    clientNum, toSend[1]);
            /* Restore server connection to allow data from any node */
            uip_create_unspecified(&server_conn->ripaddr);
            leds_toggle(LEDS_GREEN); 
        }
        else
            PRINTF("===>Incoming Packet specifies unknown Type (%X)\n",
                    data[0]);
    }
    else
        PRINTF("===>Spurious TCP/IP Invocation\n");
}

//*****************************************************************************

static void perform_self_test() {

    PRINTF("Humidity Self Test: ");
    humSensorOkay = self_test_humidity();
    PRINTF("%s\n", humSensorOkay ? "Works" : "Fails");

    PRINTF("Ext Flash Self Test: ");
    flashMemOkay = ext_flash_test();
    PRINTF("%s\n", flashMemOkay ? "Works" : "Fails");
}

//*****************************************************************************

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

//*****************************************************************************

PROCESS_THREAD(udp_server_process, ev, data) {

    uip_ipaddr_t            ipaddr;
    struct uip_ds6_addr     *root_if;

    PROCESS_BEGIN();
    PROCESS_PAUSE();

    PRINTF("\n\n ** Welcome to VeriSolutions %s **\n", SensorKindString_Hub);
    // Comm with Clients on channel Range 11-26 f/ IEEE 802.15.4
    PRINTF("Operating on RF Channel %d\n", RF_CORE_CONF_CHANNEL);
    PRINTF("Maximum Clients Preset is %d\n", MAX_VS_CLIENTS);
    PRINTF("Security Level is %d\n", NONCORESEC_CONF_SEC_LVL);

    uint32_t now = clock_seconds();
    struct tm then = {45, 32, 11, 6, 2, 117, 1, 0, 0};
    PRINTF("now=%lu  Time=%s\n", now, asctime(&then));

    uint8_t *m = ourMACAddress;
    ieee_addr_cpy_to(ourMACAddress, sizeof(ourMACAddress));
        PRINTF("Hub MAC Address=%.2X%.2X%.2X%.2X%.2X%.2X%.2X%.2X\n",
                    m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7]);

    eventSensorReading = process_alloc_event();
    process_start(&processCommWithElectron, NULL);

#if UIP_CONF_ROUTER
#if 0
/* Mode 1 - 64 bits inline */
    uip_ip6addr(&ipaddr, UIP_DS6_DEFAULT_PREFIX, 0, 0, 0, 0, 0, 0, 1);
#elif 1
/* Mode 2 - 16 bits inline */
    uip_ip6addr(&ipaddr, UIP_DS6_DEFAULT_PREFIX, 0, 0, 0, 0, 0x00ff, 0xfe00, 1);
#else
/* Mode 3 - derived from link local (MAC) address */
    uip_ip6addr(&ipaddr, UIP_DS6_DEFAULT_PREFIX, 0, 0, 0, 0, 0, 0, 0);
    uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
#endif

    // LLN = Low-Power and Lossy Networks
    // DAG = Directed Acyclic Graph
    // RPL = Routing Protocol for LLN
    uip_ds6_addr_add(&ipaddr, 0, ADDR_MANUAL);
    root_if = uip_ds6_addr_lookup(&ipaddr);
    if (root_if != NULL) {
        rpl_dag_t *dag;
        dag = rpl_set_root(RPL_DEFAULT_INSTANCE,(uip_ip6addr_t *)&ipaddr);
        uip_ip6addr(&ipaddr, UIP_DS6_DEFAULT_PREFIX, 0, 0, 0, 0, 0, 0, 0);
        rpl_set_prefix(dag, &ipaddr, 64);
        PRINTF("\nCreated New RPL DAG:\n");
     }
    else {
        PRINTF("\n===>Failed to Create New RPL DAG\n");
    }
#endif /* UIP_CONF_ROUTER */

    PRINTF("Server IPv6 addresses:\n");
    for (int i=0; i<UIP_DS6_ADDR_NB; i++) {
        int state = uip_ds6_if.addr_list[i].state;
        if (state == ADDR_TENTATIVE || state == ADDR_PREFERRED) {
            if (state == ADDR_TENTATIVE)
                uip_ds6_if.addr_list[i].state = ADDR_PREFERRED;
            extern uip_ds6_netif_t uip_ds6_if;
            uint8_t *u = uip_ds6_if.addr_list[i].ipaddr.u8;
            PRINTF("   [");
            for (int c=0; c<16; c+=2)
                PRINTF("%.2X%.2X", u[c], u[c+1]);
            PRINTF("]\n");
        }
    }

    /* The data sink runs with a 100% duty cycle in order to ensure high 
       packet reception rates. */
    NETSTACK_MAC.off(1);

    server_conn = udp_new(NULL, UIP_HTONS(UDP_CLIENT_PORT), NULL);

    if (server_conn == NULL) {
  	    PRINTF("===>No UDP connection available\n");
  	    PRINTF("===>Exiting...\n");
        PROCESS_EXIT();
    }
    udp_bind(server_conn, UIP_HTONS(UDP_SERVER_PORT));
    perform_self_test();
    PRINTF("\nCreated Connection with Remote Address: ");
    uint16_t *u = server_conn->ripaddr.u16;
    PRINTF("[%.4X:%.4X:%.4X:%.4X:%.4X:%.4X:%.4X:%.4X]\n",
            u[0], u[1], u[2], u[3], u[4], u[5], u[6], u[7]);
    PRINTF("\nLocal/Remote Port %u/%u\n", 
          UIP_HTONS(server_conn->lport), UIP_HTONS(server_conn->rport));

    while (true) {
        PROCESS_YIELD();

        PRINTF("%s  Type=", now_string());

        if (ev == tcpip_event) {
            PRINTF("TCP/IP: ");
            tcpip_handler();
        }
        else if (ev == eventSelfTest) {
            radio_value_t   current_chan;
            PRINTF("SELF TEST\n");
            NETSTACK_RADIO.get_value(RADIO_PARAM_CHANNEL, &current_chan);
            char buffer[64];
            uint8_t *m = ourMACAddress;
            snprintf(buffer, sizeof(buffer),
                    "31,%.2X%.2X%.2X%.2X%.2X%.2X,%d,%d,%d,%d,%s", 
                    m[2], m[3], m[4], m[6], m[6], m[7],
                    humSensorOkay, flashMemOkay, current_chan,
                    5/*why?*/, FW_VERSION);
            printf("%s,%d\n", buffer, cksum16(buffer, strlen(buffer)));
        }
        else if (ev == eventSensorReading)
            PRINTF("SENSOR READING\n");
        else if (ev == serial_line_event_message)
            PRINTF("SERIAL LINE EVENT\n");
        else
            PRINTF("UNKNOWN (ev=%X)\n", ev);
    }
    PROCESS_END();
}

//*****************************************************************************

static char *ipaddrToChar (const uint8_t *addr) {

    static char buff[16];
    snprintf(buff, sizeof(buff), "%02X%02X%02X%02X%02X%02X",
            addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
    return buff;
}

//*****************************************************************************

static void add_sensor(struct vsMsgData_t *sensor) {

    int i = sensor->index;
    memcpy(&vsMsgData[i], sensor, sizeof(struct vsMsgData_t));
}

//*****************************************************************************
/**
  * Send all Sensor data to Hub every X seconds
  * Continue sending until all data has been acknowledged by Hub
  */
static void xferPacketsToHub() {

    for (int i=0; i<MAX_VS_CLIENTS; i++) {
        if (strlen(vsMsgData[i].mac) > 0) {
            char buffer[64];
            int  temp = vsMsgData[i].temp;
            snprintf(buffer, sizeof(buffer), "s,%d,%s,%d.%02d,%d,%d,%d,%s", i,
                    vsMsgData[i].mac, temp/100, temp%100,
                    vsMsgData[i].humidity, vsMsgData[i].battery,
                    vsMsgData[i].rssi, ipaddrToChar(vsMsgData[i].parent_mac));
            printf("%s,%d\n", buffer, cksum16(buffer, strlen(buffer)));
        }
    }
    ctimer_set(&timerSendSensorPacket, HUB_COMM_WITH_ELECTRON_INTERVAL,
            xferPacketsToHub, NULL);
}

//*****************************************************************************

static void parse_command(char *recBuffer ) {

    int     recLength = strlen(recBuffer);
    char    *checkStr = strrchr(recBuffer, ',');

    PRINTF("%s  From VS: ", now_string());
    if (checkStr && (recLength=checkStr-recBuffer)) {
        char tempBuf[64];
        strcpy(tempBuf, recBuffer);
        int ourCksum = cksum16(recBuffer, recLength);
        int msgCksum = atoi(checkStr+1);
        if (ourCksum != msgCksum) 
            PRINTF("Packet w/ invalid Checksum\n");
        else {
            char    *cmdStr = strtok(tempBuf, ",");
            if (!strcmp(cmdStr, "31")) {    // self test
                process_post_synch(&udp_server_process,
                        eventSelfTest, NULL);
                PRINTF("SELF TEST  Pkt=\"%s\"\n", recBuffer);
            }
            else
                PRINTF("Unexpected Pkt=\"%s\"\n", recBuffer);
        } 
    }
    else if (!strncmp(recBuffer, "as", 2)) {
        int pktAck = atoi(recBuffer+2);
        PRINTF("Packet %d Ack\n", pktAck);
        if ((pktAck>=0) && (pktAck<MAX_VS_CLIENTS))
            memset(&vsMsgData[pktAck], 0, sizeof(struct vsMsgData_t));
    }
    else
        PRINTF("Trash Packet \"%s\"\n", recBuffer);
}

//*****************************************************************************

PROCESS_THREAD (processCommWithElectron, ev, data) {
    PROCESS_BEGIN();

    PRINTF("===> processCommWithElectron() Starting\n");

    eventSelfTest = process_alloc_event();

    cc26xx_uart_set_input(serial_line_input_byte);

    ctimer_set(&timerSendSensorPacket, HUB_COMM_WITH_ELECTRON_INTERVAL, 
            xferPacketsToHub, NULL);

    while (true) {
        PROCESS_YIELD();
        if (ev == serial_line_event_message && data)
            parse_command((char*)data);
        else if (ev == eventSensorReading && data)
            add_sensor((struct vsMsgData_t*)data);
        else
            PRINTF("==>Unexpected UART Event (ev=%X)\n", ev);
    }
    PROCESS_END();
}

/*----------------------------------------------------------------------------*/
