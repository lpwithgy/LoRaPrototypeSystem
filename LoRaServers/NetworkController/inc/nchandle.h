/*
 * nchandle.h
 *
 *  Created on: Mar 28, 2016
 *      Author: paullu
 */

#ifndef NCHANDLE_H_
#define NCHANDLE_H_


#endif /* NCHANDLE_H_ */
#include <stdint.h>
#include "generic_list.h"
#define JSON_MAX 1024

/*!
 * LoRaMAC server MAC commands
 * Copy from LoRaMac.h
 * LoRaWAN Specification V1.0, chapter 5, table 4
 */
typedef enum eLoRaMacSrvCmd
{
    /*!
     * LinkCheckAns
     */
    SRV_MAC_LINK_CHECK_ANS           = 0x02,
    /*!
     * LinkADRReq
     */
    SRV_MAC_LINK_ADR_REQ             = 0x03,
    /*!
     * DutyCycleReq
     */
    SRV_MAC_DUTY_CYCLE_REQ           = 0x04,
    /*!
     * RXParamSetupReq
     */
    SRV_MAC_RX_PARAM_SETUP_REQ       = 0x05,
    /*!
     * DevStatusReq
     */
    SRV_MAC_DEV_STATUS_REQ           = 0x06,
    /*!
     * NewChannelReq
     */
    SRV_MAC_NEW_CHANNEL_REQ          = 0x07,
    /*!
     * RXTimingSetupReq
     */
    SRV_MAC_RX_TIMING_SETUP_REQ      = 0x08,
}LoRaMacSrvCmd_t;

/*!
 * LoRaMAC mote MAC commands
 * copy from LoRaMac.h
 * LoRaWAN Specification V1.0, chapter 5, table 4
 */
typedef enum eLoRaMacMoteCmd
{
    /*!
     * LinkCheckReq
     */
    MOTE_MAC_LINK_CHECK_REQ          = 0x02,
    /*!
     * LinkADRAns
     */
    MOTE_MAC_LINK_ADR_ANS            = 0x03,
    /*!
     * DutyCycleAns
     */
    MOTE_MAC_DUTY_CYCLE_ANS          = 0x04,
    /*!
     * RXParamSetupAns
     */
    MOTE_MAC_RX_PARAM_SETUP_ANS      = 0x05,
    /*!
     * DevStatusAns
     */
    MOTE_MAC_DEV_STATUS_ANS          = 0x06,
    /*!
     * NewChannelAns
     */
    MOTE_MAC_NEW_CHANNEL_ANS         = 0x07,
    /*!
     * RXTimingSetupAns
     */
    MOTE_MAC_RX_TIMING_SETUP_ANS     = 0x08,
}LoRaMacMoteCmd_t;

/*structure used to transfer connected sockfd*/
struct th_arg{
	int connfd;
};

/*structure used for sending and receiving data*/
struct pkt{
	char json_content[JSON_MAX];
};

struct msg_cmd{
	char*  json_string;
};

struct msg_trans{
	uint32_t devaddr;
	uint8_t rx1_dr;
	uint8_t rx2_dr;
	uint32_t rx2_freq;
};

struct msg_rxdelay{
	uint32_t devaddr;
	uint8_t delay;
};

struct command_info{
	uint32_t devaddr;
	uint8_t cmd_num;
	uint8_t type[15];
	bool isworked[15];
};

/*destory the linked list node*/
void destroy_msg_cmd(void* msg);

/*clone the linked list node*/
void assign_msg_cmd(void* data,const void* msg);
void assign_msg_trans(void* data,const void* msg);
void assign_msg_rxdelay(void* data,const void* msg);

/*compare with the node*/
int compare_msg_trans(const void* data,const void* key);
int compare_msg_rxdelay(const void* data,const void* key);

/*deep clone*/
void copy_msg_trans(void* data,const void* msg);
void copy_msg_rxdelay(void* data,const void* msg);

/*packet the command to JSON*/
int command_handle(int,uint32_t,char*,...);

/*handle the command message in upstream*/
void msg_handle(const char*,int,struct command_info*);


