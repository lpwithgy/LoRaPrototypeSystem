/*
 * nchandle.c
 *
 *  Created on: Mar 28, 2016
 *      Author: paullu
 */
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "nchandle.h"
#include "LoRaMacCrypto.h"
#include "db_mysql.h"
#include "parson.h"

#define MSG(args...)    printf(args)

#define CMD_FRAME_DOWN_MAX 34 /*1+4+1+2+15+1+6+4*/
#define UP            0
#define DOWN          1
#define CMD_UP_MAX    15

/*!
 * LoRaMAC header field definition (MHDR field)
 * Copy from LoRaMac.h
 * LoRaWAN Specification V1.0, chapter 4.2
 */
typedef union uLoRaMacHeader
{
    /*!
     * Byte-access to the bits
     */
    uint8_t Value;
    /*!
     * Structure containing single access to header bits
     */
    struct sHdrBits
    {
        /*!
         * Major version
         */
        uint8_t Major           : 2;
        /*!
         * RFU
         */
        uint8_t RFU             : 3;
        /*!
         * Message type
         */
        uint8_t MType           : 3;
    }Bits;
}LoRaMacHeader_t;

/*!
 * LoRaMAC frame control field definition (FCtrl)
 * Copy from LoRaMac.h
 * LoRaWAN Specification V1.0, chapter 4.3.1
 */
typedef union uLoRaMacFrameCtrl
{
    /*!
     * Byte-access to the bits
     */
    uint8_t Value;
    struct sCtrlBits
    {
        /*!
         * Frame options length
         */
        uint8_t FOptsLen        : 4;
        /*!
         * Frame pending bit
         */
        uint8_t FPending        : 1;
        /*!
         * Message acknowledge bit
         */
        uint8_t Ack             : 1;
        /*!
         * ADR acknowledgment request bit
         */
        uint8_t AdrAckReq       : 1;
        /*!
         * ADR control in frame header
         */
        uint8_t Adr             : 1;
    }Bits;
}LoRaMacFrameCtrl_t;
/*!
 * MAC command DataRate_TxPower definition
 * LoRaWAN Specification V1.0, chapter 5.2
 */
typedef union uMacCommandDataRateTxPower{
	uint8_t Value;
	struct sDataRateTxPower{
		uint8_t TxPower             :4;
		uint8_t DataRate            :4;
	}Bits;
}MacCmdDataRateTxPower_t;

/*!
 * MAC command Redundancy definition
 * LoRaWAN Specification V1.0, chapter 5.2
 */
typedef union uMacCommandRedundancy{
	uint8_t Value;
	struct sRedundancy{
		uint8_t NbRep               :4;
		uint8_t chMaskCntl          :3;
		uint8_t RFU                 :1;
	}Bits;
}MacCmdRedundancy_t;

/*!
 * MAC command DLSettings definition
 * LoRaWAN Specification V1.0, chapter 5.4
 */
typedef union uMacCommandDLSettings{
	uint8_t Value;
	struct sDLSettings{
		uint8_t RX2DataRate         :4;
		uint8_t RX1DRoffset         :3;
		uint8_t RFU                 :1;
	}Bits;
}MacCmdDLSettings_t;

/*!
 * MAC command DLSettings definition
 * LoRaWAN Specification V1.0, chapter 5.4
 */
typedef union uMacCommandDrRange{
	uint8_t Value;
	struct sDrRange{
		uint8_t MinDR         :4;
		uint8_t MaxDR         :4;
	}Bits;
}MacCmdDrRange_t;

/*!
 * MAC command Settings definition
 * LoRaWAN Specification V1.0, chapter 5.7
 */
typedef union uMacCommandSettings{
	uint8_t Value;
	struct sSettings{
		uint8_t Del           :4;
		uint8_t RFU           :4;
	}Bits;
}MacCmdSettings_t;

typedef union uMacCommandADRStatus{
	uint8_t Value;
	struct sADRStatus{
		uint8_t ChannelMaskAck :1;
		uint8_t DatarateAck    :1;
		uint8_t PowerAck       :1;
		uint8_t RFU            :5;
	}Bits;
}MacCmdADRStatus_t;

typedef union uMacCommandRXStatus{
	uint8_t Value;
	struct sRXStatus{
		uint8_t ChannelAck     :1;
		uint8_t RX2DatarateAck :1;
		uint8_t RX1DRoffsetAck :1;
		uint8_t RFU            :5;
	}Bits;
}MacCmdRXStatus_t;

typedef union uMacCommandChannelStatus{
	uint8_t Value;
	struct sChannelStatus{
		uint8_t ChannelFreqAck      :1;
		uint8_t DatarateRangeAck    :1;
		uint8_t RFU                 :6;
	}Bits;
}MacCmdChannelStatus_t;

typedef union uMacCommandMargin{
	uint8_t Value;
	struct sMargin{
		uint8_t Margin  :6;
		uint8_t RFU     :2;
	}Bits;
}MacCmdMargin_t;

/*!
 * LoRaMAC frame types
 * Copy from LoRaMAC.h
 * LoRaWAN Specification V1.0, chapter 4.2.1, table 1
 */
typedef enum eLoRaMacFrameType
{
    /*!
     * LoRaMAC join request frame
     */
    FRAME_TYPE_JOIN_REQ              = 0x00,
    /*!
     * LoRaMAC join accept frame
     */
    FRAME_TYPE_JOIN_ACCEPT           = 0x01,
    /*!
     * LoRaMAC unconfirmed up-link frame
     */
    FRAME_TYPE_DATA_UNCONFIRMED_UP   = 0x02,
    /*!
     * LoRaMAC unconfirmed down-link frame
     */
    FRAME_TYPE_DATA_UNCONFIRMED_DOWN = 0x03,
    /*!
     * LoRaMAC confirmed up-link frame
     */
    FRAME_TYPE_DATA_CONFIRMED_UP     = 0x04,
    /*!
     * LoRaMAC confirmed down-link frame
     */
    FRAME_TYPE_DATA_CONFIRMED_DOWN   = 0x05,
    /*!
     * LoRaMAC RFU frame
     */
    FRAME_TYPE_RFU                   = 0x06,
    /*!
     * LoRaMAC proprietary frame
     */
    FRAME_TYPE_PROPRIETARY           = 0x07,
}LoRaMacFrameType_t;

/*!
 * LoRaMAC frame counter. Each time a packet is sent the counter is incremented.
 * Only the 16 LSB bits are sent
 */

/*prepare the frame of downstream*/
static void prepare_frame(uint8_t type,uint32_t devAddr,uint32_t downcnt,const uint8_t* payload,const uint8_t* nwkSKey,int payload_size,uint8_t* frame,int* frame_size);

int command_handle(int cid,uint32_t devAddr,char* json_data,...){

	va_list arg_ptr;

	MYSQL mysql;
	uint8_t *payload;/*mac commmand*/
	uint8_t frame_raw[CMD_FRAME_DOWN_MAX];/*enough to storing downstream data*/
	char* frame;
	uint8_t nwkSKey[16];
	int frame_size;
	uint32_t downlink_counter;

	/*variables for generating mac command*/
	uint8_t margin;
	uint8_t gw_cnt;
	uint8_t datarate;
	uint8_t tx_power;
	uint16_t ch_mask;
	uint8_t  ch_mask_cntl;
	uint8_t  nb_rep;
	uint8_t  max_dcycle;
	uint8_t  rx1_droffset;
	uint8_t  rx2_datarate;
	uint32_t frequency;
	uint8_t ch_index;
	uint32_t freq;
	uint8_t  max_dr;
	uint8_t  min_dr;
	uint8_t  del;

	/*JSON parsing variables*/
	JSON_Value* root_val=NULL;
	JSON_Object* root_obj=NULL;
	char* json_str;

	int residue;
	int max_len;

	bzero(frame_raw,sizeof(frame_raw));
	bzero(nwkSKey,sizeof(nwkSKey));

	if(connect_db("localhost","root","1005541787","LoRaWAN",&mysql)==FAILED){
		MSG("WARNING: [down] access the database failed\n");
		close_db(&mysql);
		return -1;
	}
	if(query_db_by_addr("nwkskey","nsdevinfo",devAddr,&mysql,nwkSKey,16)==FAILED){
		MSG("WARNING: [down] query the database failed\n");
		close_db(&mysql);
		return -1;
	}
	if(query_db_by_addr_uint("downcnt","nsdevinfo",devAddr,&mysql,&downlink_counter)==FAILED){
		MSG("WARNING: [down] query the database failed\n");
		close_db(&mysql);
		return -1;
	}
	va_start(arg_ptr,json_data);
	switch(cid){
		case SRV_MAC_LINK_CHECK_ANS:{
			margin=(uint8_t)va_arg(arg_ptr,int);
			gw_cnt=(uint8_t)va_arg(arg_ptr,int);
			payload=malloc(sizeof(uint8_t)*3);
			payload[0]=SRV_MAC_LINK_CHECK_ANS;
			payload[1]=margin;
			payload[2]=gw_cnt;
			prepare_frame(FRAME_TYPE_DATA_UNCONFIRMED_DOWN,devAddr,downlink_counter,payload,nwkSKey,3,frame_raw,&frame_size);
			free(payload);
			break;
		}
		case SRV_MAC_LINK_ADR_REQ:{
			/*
			 * TODO:
			 * query the adr field in the database
			 */
			datarate=(uint8_t)va_arg(arg_ptr,int);
			tx_power=(uint8_t)va_arg(arg_ptr,int);
			ch_mask=(uint16_t)va_arg(arg_ptr,int);
			ch_mask_cntl=(uint8_t)va_arg(arg_ptr,int);
			nb_rep=(uint8_t)va_arg(arg_ptr,int);
			MacCmdDataRateTxPower_t datarate_txpower;
			MacCmdRedundancy_t redundancy;
			datarate_txpower.Bits.DataRate=datarate;
			datarate_txpower.Bits.TxPower=tx_power;
			redundancy.Value=0;
			redundancy.Bits.chMaskCntl=ch_mask_cntl;
			redundancy.Bits.NbRep=nb_rep;
			payload=malloc(sizeof(uint8_t)*5);
			payload[0]=SRV_MAC_LINK_ADR_REQ;
			payload[1]=datarate_txpower.Value;
			payload[2]=ch_mask&0xFF;
			payload[3]=(ch_mask>>8)&0xFF;
			payload[4]=redundancy.Value;
			prepare_frame(FRAME_TYPE_DATA_CONFIRMED_DOWN,devAddr,downlink_counter,payload,nwkSKey,5,frame_raw,&frame_size);
			free(payload);
			break;
		}
		case SRV_MAC_DUTY_CYCLE_REQ:{
			max_dcycle=(uint8_t)va_arg(arg_ptr,int);
			payload=malloc(sizeof(uint8_t)*2);
			payload[0]=SRV_MAC_DUTY_CYCLE_REQ;
			payload[1]=max_dcycle;
			prepare_frame(FRAME_TYPE_DATA_CONFIRMED_DOWN,devAddr,downlink_counter,payload,nwkSKey,2,frame_raw,&frame_size);
			free(payload);
			break;
		}
		case SRV_MAC_RX_PARAM_SETUP_REQ:{
			rx1_droffset=(uint8_t)va_arg(arg_ptr,int);
			rx2_datarate=(uint8_t)va_arg(arg_ptr,int);
			frequency=(uint32_t)va_arg(arg_ptr,int);
			MacCmdDLSettings_t dlsettings;
			dlsettings.Value=0;
			dlsettings.Bits.RX1DRoffset=rx1_droffset;
			dlsettings.Bits.RX2DataRate=rx2_datarate;
			payload=malloc(sizeof(uint8_t)*5);
			payload[0]=SRV_MAC_RX_PARAM_SETUP_REQ;
			payload[1]=dlsettings.Value;
			payload[2]=frequency&0xFF;
			payload[3]=(frequency>>8)&0xFF;
			payload[4]=(frequency>>16)&0xFF;
			prepare_frame(FRAME_TYPE_DATA_CONFIRMED_DOWN,devAddr,downlink_counter,payload,nwkSKey,5,frame_raw,&frame_size);
			free(payload);
			break;
		}
		case SRV_MAC_DEV_STATUS_REQ:{
			payload=malloc(sizeof(uint8_t)*1);
			payload[0]=SRV_MAC_DEV_STATUS_REQ;
			prepare_frame(FRAME_TYPE_DATA_CONFIRMED_DOWN,devAddr,downlink_counter,payload,nwkSKey,1,frame_raw,&frame_size);
			free(payload);
			break;
		}
		case SRV_MAC_NEW_CHANNEL_REQ:{
			ch_index=(uint8_t)va_arg(arg_ptr,int);
			freq=(uint32_t)va_arg(arg_ptr,int);
			max_dr=(uint8_t)va_arg(arg_ptr,int);
			min_dr=(uint8_t)va_arg(arg_ptr,int);
			MacCmdDrRange_t dr_range;
			dr_range.Bits.MaxDR=max_dr;
			dr_range.Bits.MinDR=min_dr;
			payload=malloc(sizeof(uint8_t)*6);
			payload[0]=SRV_MAC_NEW_CHANNEL_REQ;
			payload[1]=ch_index;
			payload[2]=freq&0xFF;
			payload[3]=(freq>>8)&0xFF;
			payload[4]=(freq>>16)&0xFF;
			payload[5]=dr_range.Value;
			prepare_frame(FRAME_TYPE_DATA_CONFIRMED_DOWN,devAddr,downlink_counter,payload,nwkSKey,6,frame_raw,&frame_size);
			free(payload);
			break;
		}
		case SRV_MAC_RX_TIMING_SETUP_REQ:{
			del=(uint8_t)va_arg(arg_ptr,int);
			MacCmdSettings_t settings;
			settings.Value=0;
			settings.Bits.Del=del;
			payload=malloc(sizeof(uint8_t)*2);
			payload[0]=SRV_MAC_RX_TIMING_SETUP_REQ;
			payload[1]=settings.Value;
			prepare_frame(FRAME_TYPE_DATA_CONFIRMED_DOWN,devAddr,downlink_counter,payload,nwkSKey,2,frame_raw,&frame_size);
			free(payload);
			break;
		}
		default:{
			va_end(arg_ptr);
			close_db(&mysql);
			return -1;
		}
	}
	va_end(arg_ptr);
	downlink_counter++;
	if(update_db_by_addr_uint("downcnt","nsdevinfo",devAddr,&mysql,downlink_counter)==FAILED){
		MSG("WARNING: [down] update the database failed\n");
		close_db(&mysql);
		return -1;
	}
	close_db(&mysql);
	residue=frame_size%3;
	if(residue==1){
		max_len=(frame_size+2)/3*4+1;
		frame=malloc(sizeof(char)*max_len);
	}
	else if(residue==2){
		max_len=(frame_size+1)/3*4+1;
		frame=malloc(sizeof(char)*max_len);
	}
	else{
		max_len=frame_size/3*4+1;
		frame=malloc(sizeof(char)*max_len);
	}
	bin_to_b64(frame_raw,frame_size,frame,max_len);
	/*parsing the frame to json string*/
	root_val=json_value_init_object();
	root_obj=json_value_get_object(root_val);
	json_object_dotset_number(root_obj,"app.devaddr",devAddr);
	json_object_dotset_string(root_obj,"app.control.frame",frame);
	json_object_dotset_number(root_obj,"app.control.size",frame_size);
	json_str=json_serialize_to_string_pretty(root_val);
	strcpy(json_data,json_str);
	json_free_serialized_string(json_str);
	json_value_free(root_val);
	free(frame);
	return 1;
}

void msg_handle(const char* msg,int index,struct command_info* cmd_info){
	/*JSON parsing variables*/
	JSON_Value *root_val=NULL;
	JSON_Object *app_obj=NULL;
	JSON_Object *devx_obj=NULL;
	JSON_Object *gwrx_obj=NULL;
	JSON_Object *maccmd_obj=NULL;
	JSON_Value *val=NULL;

	char msgname[64];
	char tempstr[64];
	char content[512];
	char gwaddr[16];
	char deveui_hex[17];
	uint32_t devaddr;
	uint32_t seqno;
	int isencrypt;
	const char* str;
	uint8_t cmd_payload[CMD_UP_MAX];
	uint8_t command[CMD_UP_MAX];
	int psize;
	MYSQL mysql;
	uint8_t nwkSKey[16];

	int i=0;
	int j=0;
	uint8_t cid;

	bzero(msgname,sizeof(msgname));
	bzero(content,sizeof(content));
	bzero(gwaddr,sizeof(gwaddr));
	bzero(deveui_hex,sizeof(deveui_hex));
	root_val=json_parse_string_with_comments(msg);
	if(root_val==NULL){
		MSG("WARNING: [up] message_%d contains invalid JSON\n",index);
		json_value_free(root_val);
	}
	app_obj=json_object_get_object(json_value_get_object(root_val),"app");
	if(app_obj==NULL){
		MSG("WARNING: [up] message received contains no \"app\"\n");
		json_value_free(root_val);
	}
	else{
		snprintf(msgname,sizeof(msgname),"###############command_message_%d:###############\n",index);
		strcat(content,msgname);
		val=json_object_get_value(app_obj,"gwaddr");
		if(val!=NULL){
			strcpy(gwaddr,json_value_get_string(val));
			snprintf(tempstr,sizeof(tempstr),"gwaddr:%s",json_value_get_string(val));
			strcat(content,tempstr);
		}
		val=json_object_get_value(app_obj,"deveui");
		if(val!=NULL){
			strcpy(deveui_hex,json_value_get_string(val));
			snprintf(tempstr,sizeof(tempstr)," deveui:%s",json_value_get_string(val));
			strcat(content,tempstr);
		}
		val=json_object_get_value(app_obj,"devaddr");
		if(val!=NULL){
			devaddr=(uint32_t)json_value_get_number(val);
			snprintf(tempstr,sizeof(tempstr)," devaddress:%.0f",json_value_get_number(val));
			strcat(content,tempstr);
		}
		val=json_object_get_value(app_obj,"dir");
		if(val!=NULL){
			snprintf(tempstr,sizeof(tempstr)," direction:%s",json_value_get_string(val));
			strcat(content,tempstr);
		}
		val=json_object_get_value(app_obj,"seqno");
		if(val!=NULL){
			seqno=json_value_get_number(val);
			snprintf(tempstr,sizeof(tempstr)," sequenceNo:%.0f",json_value_get_number(val));
			strcat(content,tempstr);
		}
		devx_obj=json_object_get_object(app_obj,"devx");
		if(devx_obj==NULL){
			MSG("WARNING: [up] message received contains no \"devx\" in \"app\"\n");
		}
		else{
			val=json_object_get_value(devx_obj,"freq");
			if(val!=NULL){
				snprintf(tempstr,sizeof(tempstr)," frequence:%.6f",json_value_get_number(val));
				strcat(content,tempstr);
			}
			val=json_object_get_value(devx_obj,"modu");
			if(val!=NULL){
				snprintf(tempstr,sizeof(tempstr)," modulation:%s",json_value_get_string(val));
				strcat(content,tempstr);
				if(strcmp("LORA",json_value_get_string(val))==0){
					val=json_object_get_value(devx_obj,"datr");
					if(val!=NULL){
						snprintf(tempstr,sizeof(tempstr)," datr:%s",json_value_get_string(val));
						strcat(content,tempstr);
					}
				}
				else{
					val=json_object_get_value(devx_obj,"datr");
					if(val!=NULL){
						snprintf(tempstr,sizeof(tempstr)," datarate:%.2f",json_value_get_number(val));
						strcat(content,tempstr);
					}
				}
			}
			val=json_object_get_value(devx_obj,"codr");
			if(val!=NULL){
				snprintf(tempstr,sizeof(tempstr)," coderate:%s",json_value_get_string(val));
				strcat(content,tempstr);
			}
			val=json_object_get_value(devx_obj,"adr");
			if(val!=NULL){
				snprintf(tempstr,sizeof(tempstr)," adr:%d",json_value_get_boolean(val));
				strcat(content,tempstr);
			}
		}
		gwrx_obj=json_object_get_object(app_obj,"gwrx");
		if(gwrx_obj==NULL){
			MSG("WARNING: [up] message received contains no \"gwrx\" in \"app\"\n");
		}
		else{
			val=json_object_get_value(gwrx_obj,"time");
			if(val!=NULL){
				snprintf(tempstr,sizeof(tempstr)," time:%s",json_value_get_string(val));
				strcat(content,tempstr);
			}
			val=json_object_get_value(gwrx_obj,"chan");
			if(val!=NULL){
				snprintf(tempstr,sizeof(tempstr)," channel:%.0f",json_value_get_number(val));
				strcat(content,tempstr);
			}
			val=json_object_get_value(gwrx_obj,"rfch");
			if(val!=NULL){
				snprintf(tempstr,sizeof(tempstr)," rfchannel:%.0f",json_value_get_number(val));
				strcat(content,tempstr);
			}
			val=json_object_get_value(gwrx_obj,"rssi");
			if(val!=NULL){
				snprintf(tempstr,sizeof(tempstr)," rssi:%.0f",json_value_get_number(val));
				strcat(content,tempstr);
			}
			val=json_object_get_value(gwrx_obj,"lsnr");
			if(val!=NULL){
				snprintf(tempstr,sizeof(tempstr)," lsnr:%.2f",json_value_get_number(val));
				strcat(content,tempstr);
			}
		}
		maccmd_obj=json_object_get_object(app_obj,"maccmd");
		if(maccmd_obj==NULL){
			MSG("WARNING: [up] message received contains no \"maccmd\" in \"app\"\n");
		}
		else{
			val=json_object_get_value(maccmd_obj,"isencrypt");
			if(val!=NULL){
				isencrypt=json_value_get_boolean(val);
				snprintf(tempstr,sizeof(tempstr)," isencrypt:%d",json_value_get_boolean(val));
				strcat(content,tempstr);
			}
			val=json_object_get_value(maccmd_obj,"command");
			if(val!=NULL){
				str=json_value_get_string(val);
				psize=b64_to_bin(str,strlen(str),cmd_payload,CMD_UP_MAX);
				if(isencrypt==1){
					if(connect_db("localhost","root","1005541787","LoRaWAN",&mysql)==FAILED){
							MSG("WARNING: [down] query the database failed\n");
							return;
					}
					if(query_db_by_addr("nwkskey","nsdevinfo",devaddr,&mysql,nwkSKey,16)==FAILED){
							MSG("WARNING: [down] query the database failed\n");
							return;
					}
					close_db(&mysql);
					/*compute the LoRaMAC frame payload decryption*/
					LoRaMacPayloadDecrypt(cmd_payload,psize,nwkSKey,devaddr,UP,seqno,command);
				}
				else{
					memcpy(command,cmd_payload,psize);
				}
				cmd_info->devaddr=devaddr;
				while(i<psize){
					cid=command[i];
					switch(cid){
						case MOTE_MAC_LINK_CHECK_REQ:{
							/*
							 * TODO:
						 	 * call command_handle()
						 	 */
							cmd_info->type[j]=MOTE_MAC_LINK_CHECK_REQ;
							cmd_info->isworked[j]=true;
							strcpy(tempstr,"\ncommand:LINK_CHECK_REQUEST");
							strcat(content,tempstr);
							break;
						}
						case MOTE_MAC_LINK_ADR_ANS:{
							MacCmdADRStatus_t status;
							cmd_info->type[j]=MOTE_MAC_LINK_ADR_ANS;
							status.Value=command[++i];
							strcpy(tempstr,"\ncommand:LINK_ADR_ANSWER");
							strcat(content,tempstr);
							status.Bits.ChannelMaskAck==1 ? strcpy(tempstr,"   channel mask OK"):strcpy(tempstr,"   channel mask INVALID");
							strcat(content,tempstr);
							status.Bits.DatarateAck==1 ? strcpy(tempstr,"   datarate OK"):strcpy(tempstr,"   datarate INVALID");
							strcat(content,tempstr);
							status.Bits.PowerAck==1 ? strcpy(tempstr,"   power OK"):strcpy(tempstr,"   power INVALID");
							if(status.Bits.ChannelMaskAck==1&&status.Bits.DatarateAck==1&&status.Bits.PowerAck==1)
								cmd_info->isworked[j]=true;
							else
								cmd_info->isworked[j]=false;
							strcat(content,tempstr);
							break;
						}
						case MOTE_MAC_DUTY_CYCLE_ANS:{
							cmd_info->type[j]=MOTE_MAC_DUTY_CYCLE_ANS;
							cmd_info->isworked[j]=true;
							strcpy(tempstr,"\ncommand:DUTY_CYCLE_ANSWER");
							strcat(content,tempstr);
							break;
						}
						case MOTE_MAC_RX_PARAM_SETUP_ANS:{
							MacCmdRXStatus_t status;
							cmd_info->type[j]=MOTE_MAC_RX_PARAM_SETUP_ANS;
							status.Value=command[++i];
							strcpy(tempstr,"\ncommand:RX_PARAM_SETUP_ANSWER");
							strcat(content,tempstr);
							status.Bits.ChannelAck==1 ? strcpy(tempstr,"   channel OK"):strcpy(tempstr,"   channel INVALID");
							strcat(content,tempstr);
							status.Bits.RX2DatarateAck==1 ? strcpy(tempstr,"   RX2 datarate OK"):strcpy(tempstr,"   RX2 datarate INVALID");
							strcat(content,tempstr);
							status.Bits.RX1DRoffsetAck==1 ? strcpy(tempstr,"   RX1 datarate offset OK"):strcpy(tempstr,"   RX1 datarate offset INVALID");
							strcat(content,tempstr);
							if(status.Bits.ChannelAck==1&&status.Bits.RX2DatarateAck==1&&status.Bits.RX2DatarateAck==1)
								cmd_info->isworked[j]=true;
							else
								cmd_info->isworked[j]=false;
							break;
						}
						case MOTE_MAC_DEV_STATUS_ANS:{
							uint8_t battery;
							MacCmdMargin_t margin;
							cmd_info->type[j]=MOTE_MAC_DEV_STATUS_ANS;
							cmd_info->isworked[j]=true;
							battery=command[++i];
							margin.Value=command[++i];
							strcpy(tempstr,"\ncommand:DEVICE_STATUS_ANSWER");
							strcat(content,tempstr);
							snprintf(tempstr,sizeof(tempstr),"   battery:%d",battery);
							strcat(content,tempstr);
							snprintf(tempstr,sizeof(tempstr),"   margin:%d",margin.Bits.Margin);
							strcat(content,tempstr);
							break;
						}
						case MOTE_MAC_NEW_CHANNEL_ANS:{
							MacCmdChannelStatus_t status;
							cmd_info->type[j]=MOTE_MAC_NEW_CHANNEL_ANS;
							status.Value=command[++i];
							strcpy(tempstr,"\ncommand:NEW_CHANNEL_ANSWER");
							strcat(content,tempstr);
							status.Bits.ChannelFreqAck==1 ? strcpy(tempstr,"   channel frequency OK"):strcpy(tempstr,"   channel frequency INVALID");
							strcat(content,tempstr);
							status.Bits.DatarateRangeAck==1 ? strcpy(tempstr,"   datarate range OK"):strcpy(tempstr,"   datarate range INVALID");
							strcat(content,tempstr);
							if(status.Bits.ChannelFreqAck==1&&status.Bits.DatarateRangeAck)
								cmd_info->isworked[j]=true;
							else
								cmd_info->isworked[j]=false;
							break;
						}
						case MOTE_MAC_RX_TIMING_SETUP_ANS:{
							cmd_info->type[j]=MOTE_MAC_RX_TIMING_SETUP_ANS;
							cmd_info->isworked[j]=true;
							strcpy(tempstr,"\ncommand:RX_TIMING_SETUP_ANSWER");
							strcat(content,tempstr);
							break;
						}
						default:
							break;
					}
					++i;
					++j;
				}
				cmd_info->cmd_num=j;
			}
		}
	json_value_free(root_val);
	}
	MSG("%s\n",content);
}

void prepare_frame(uint8_t type,uint32_t devAddr,uint32_t downcnt,const uint8_t* payload,const uint8_t* nwkSKey,int payload_size,uint8_t* frame,int* frame_size){
	LoRaMacHeader_t hdr;
	LoRaMacFrameCtrl_t fctrl;
	uint8_t index=0;
	uint8_t* encpayload;
	uint32_t mic;

	/*MHDR*/
	hdr.Value=0;
	hdr.Bits.MType=type;
	frame[index]=hdr.Value;

	/*DevAddr*/
	frame[++index]=devAddr&0xFF;
	frame[++index]=(devAddr>>8)&0xFF;
	frame[++index]=(devAddr>>16)&0xFF;
	frame[++index]=(devAddr>>24)&0xFF;

	/*FCtrl*/
	fctrl.Value=0;
	if(type==FRAME_TYPE_DATA_UNCONFIRMED_DOWN){
		fctrl.Bits.Ack=1;
	}
	fctrl.Bits.Adr=1;
	frame[++index]=fctrl.Value;

	/*FCnt*/
	frame[++index]=(downcnt)&0xFF;
	frame[++index]=(downcnt>>8)&0xFF;

	/*FOpts*/
	/*Fport*/
	frame[++index]=0;

	/*encrypt the payload*/
	encpayload=malloc(sizeof(uint8_t)*payload_size);
	LoRaMacPayloadEncrypt(payload,payload_size,nwkSKey,devAddr,DOWN,downcnt,encpayload);
	++index;
	memcpy(frame+index,encpayload,payload_size);
	free(encpayload);
	index+=payload_size;

	/*calculate the mic*/
	LoRaMacComputeMic(frame,index,nwkSKey,devAddr,DOWN,downcnt,&mic);
	frame[index]=mic&0xFF;
	frame[++index]=(mic>>8)&0xFF;
	frame[++index]=(mic>>16)&0xFF;
	frame[++index]=(mic>>24)&0xFF;
	*frame_size=index+1;
}

void destroy_msg_cmd(void* msg){
	struct msg_cmd* data=(struct msg_cmd*)msg;
	free(data->json_string);
}

void assign_msg_cmd(void* data,const void* msg){
	struct msg_cmd* data_x=(struct msg_cmd*)data;
	struct msg_cmd* msg_x=(struct msg_cmd*)msg;
	data_x->json_string=msg_x->json_string;
}

void assign_msg_trans(void* data,const void* msg){
	struct msg_trans* data_x=(struct msg_trans*)data;
	struct msg_trans* msg_x=(struct msg_trans*)msg;
	data_x->devaddr=msg_x->devaddr;
	data_x->rx1_dr=msg_x->rx1_dr;
	data_x->rx2_dr=msg_x->rx2_dr;
	data_x->rx2_freq=msg_x->rx2_freq;
}

void assign_msg_rxdelay(void* data,const void* msg){
	struct msg_rxdelay* data_x=(struct msg_rxdelay*)data;
	struct msg_rxdelay* msg_x=(struct msg_rxdelay*)msg;
	data_x->devaddr=msg_x->devaddr;
	data_x->delay=msg_x->delay;
}

void copy_msg_trans(void* data,const void* msg){
	assign_msg_trans(data,msg);
}

void copy_msg_rxdelay(void* data,const void* msg){
	assign_msg_rxdelay(data,msg);
}

int compare_msg_trans(const void* data,const void* key){
	if(((struct msg_trans*)data)->devaddr==*(uint32_t*)key)
		return 0;
	else
		return 1;
}

int compare_msg_rxdelay(const void* data,const void* key){
	if(((struct msg_rxdelay*)data)->devaddr==*(uint32_t*)key)
		return 0;
	else
		return 1;
}

