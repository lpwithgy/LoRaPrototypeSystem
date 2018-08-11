/*
 * ashandle.c
 *
 *  Created on: Mar 5, 2016
 *      Author: paullu
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "parson.h"
#include "base64.h"
#include "ashandle.h"
#include "LoRaMacCrypto.h"
#include "db_mysql.h"

/*frame direction*/
#define UP                                     0
#define DOWN                                   1
#define JOIN_ACC_SIZE                          17
#define MAX_NB_B64                             25 /* 18/3*4+1 */

#define MSG(args...)    printf(args)

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
 * LoRaMAC DLSettings field definition
 * LoRaWAN Specification V1.0, chapter 5.4
 */
typedef union uLoRaMacDLSettings{
	uint8_t Value;
	struct sDLSBits{
		uint8_t RX2DataRate      : 4;
		uint8_t RX1DRoffset      : 3;
		uint8_t RFU              : 1;
	}Bits;
}LoRaMacDLSettings_t;

/*!
 * LoRaMAC DLSettings field definition
 * LoRaWAN Specification V1.0, chapter 6.2.5
 */
typedef union uLoRaMacRxDelay{
	uint8_t Value;
	struct sRxDBits{
		uint8_t Del              : 4;
		uint8_t RFU              : 4;
	}Bits;
}LoRaMacRxDelay_t;
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

/*prepare the frame payload and compute the session keys*/
static void prepare_frame(uint8_t *frame_payload,uint16_t devNonce,uint8_t* appKey,uint32_t devAddr,uint8_t *nwkSKey,uint8_t *appSKey);

struct res_handle msg_handle(char* msg,int index){
	struct res_handle res;
	/*session keys and appkey*/
	uint8_t appSKey[16];
	uint8_t nwkSKey[16];
	uint8_t appKey[16];

	/*acess the MYSQL database*/
	MYSQL mysql;

	/*JSON parsing variables*/
	JSON_Value *root_val=NULL;
	JSON_Object *app_obj=NULL;
	JSON_Object *udata_obj=NULL;
	JSON_Object *devx_obj=NULL;
	JSON_Object *gwrx_obj=NULL;
	JSON_Object *join_obj=NULL;
	JSON_Object *request_obj=NULL;
	JSON_Object *accept_obj=NULL;
	JSON_Value *val=NULL;

    JSON_Value *root_val_x=NULL;
    JSON_Object * root_obj_x=NULL;

	char content[1024];/*1024 is big enough*/
	char msgname[64];
	char tempstr[32];
	const char* str;
	char* json_str;
	uint32_t devAddr;
	uint32_t seqno;
	uint8_t userdata_payload[LORAMAC_FRAME_MAXPAYLOAD];/*the frame payload defined in LORAWAN specification */
	uint8_t dec_userdata[LORAMAC_FRAME_MAXPAYLOAD];/*decrypted frame payload*/
	int psize;/*length of frame payload size*/
	int j;

	uint64_t deveui;
	uint64_t appeui;
	char  deveui_hex[17];
	char  appeui_hex[17];
	uint16_t devNonce;
	uint8_t frame_payload[JOIN_ACC_SIZE];
	uint8_t frame_payload_b64[MAX_NB_B64];
	char nwkskey_hex[33];/*store the nwkskey*/
	char appskey_hex[33];/*store the appskey*/
	char gwaddr[16];

	/*initialize some variable in case it uses uncertain value*/
	bzero(&res,sizeof(res));
	bzero(content,sizeof(content));
	bzero(dec_userdata,sizeof(dec_userdata));
	bzero(deveui_hex,sizeof(deveui_hex));
	bzero(appeui_hex,sizeof(appeui_hex));
	bzero(nwkskey_hex,sizeof(nwkskey_hex));
	bzero(appskey_hex,sizeof(appskey_hex));
	root_val=json_parse_string_with_comments(msg);
	if(root_val==NULL){
		MSG("WARNING: [up] message received contains invalid JSON\n");
		json_value_free(root_val);
	}
	app_obj=json_object_get_object(json_value_get_object(root_val),"app");
	if(app_obj==NULL){
		join_obj=json_object_get_object(json_value_get_object(root_val),"join");
		if(join_obj==NULL){
			MSG("WARNING: [up] message received contains neither \"app\" nor \"join\"\n");
			res.signal=0;
			json_value_free(root_val);
		}
		else{
			res.signal=1;
			/*handling request json data*/
			snprintf(msgname,sizeof(msgname),"###############join_request_%d:###############\n",index);
			strcat(content,msgname);
			val=json_object_get_value(join_obj,"gwaddr");
			if(val!=NULL){
				strcpy(gwaddr,json_value_get_string(val));
				snprintf(tempstr,sizeof(tempstr),"gwaddr:%s",json_value_get_string(val));
				strcat(content,tempstr);
			}
			val=json_object_get_value(join_obj,"deveui");
			if(val!=NULL){
				strcpy(deveui_hex,json_value_get_string(val));
				snprintf(tempstr,sizeof(tempstr)," deveui:%s",json_value_get_string(val));
				strcat(content,tempstr);
			}
			val=json_object_get_value(join_obj,"appeui");
			if(val!=NULL){
				strcpy(appeui_hex,json_value_get_string(val));
				snprintf(tempstr,sizeof(tempstr)," appeui:%s",json_value_get_string(val));
				strcat(content,tempstr);
			}
			request_obj=json_object_get_object(join_obj,"request");
			if(request_obj==NULL){
				MSG("WARNING:[up] join-request message received contains no \"request\" in \"join\"\n");
			}
			else{
				val=json_object_get_value(request_obj,"devnonce");
				devNonce=(uint16_t)json_value_get_number(val);
				if(val!=NULL){
					snprintf(tempstr,sizeof(tempstr)," devnonce:%.0f",json_value_get_number(val));
					strcat(content,tempstr);
				}
				val=json_object_get_value(request_obj,"devaddr");
				devAddr=(uint32_t)json_value_get_number(val);
				if(val!=NULL){
					snprintf(tempstr,sizeof(tempstr)," devaddr:%.0f",json_value_get_number(val));
					strcat(content,tempstr);
				}
			}
			/*prepare the frame payload*/
			if(connect_db("localhost","root","1005541787","LoRaWAN",&mysql)==FAILED){
					MSG("WARNING: [up] access the database failed\n");
					res.signal=0;/*it means there will be no message returning to the NS*/
			}
			/*query the appkey from the database*/
			if(query_db_by_deveui("appkey","appsdevinfo",deveui_hex,&mysql,appKey,16)==FAILED){
					/*the device isn't registered in the application server*/
					MSG("WARNING: [up] query the database failed\n");
					res.signal=0;
			}
			else{
				prepare_frame(frame_payload,devNonce,appKey,devAddr,nwkSKey,appSKey);
				memcpy(res.appSKey,appSKey,sizeof(appSKey));
				/*binary to b64*/
				bin_to_b64(frame_payload,JOIN_ACC_SIZE,frame_payload_b64,MAX_NB_B64);
				/*get the nwkSKey and appSKey change it to char* */
				for(j=0;j<16;j++){
					snprintf(tempstr,sizeof(tempstr),"%02x",nwkSKey[j]);
					strcat(nwkskey_hex,tempstr);
				}
				for(j=0;j<16;j++){
					snprintf(tempstr,sizeof(tempstr),"%02x",appSKey[j]);
					strcat(appskey_hex,tempstr);
				}
				/*store the appSKey into the database*/
				if(update_db_by_deveui("appskey","appsdevinfo",deveui_hex,&mysql,appskey_hex,0)==FAILED){
					/*the device isn't registered in the application server*/
					MSG("WARNING: [up] update the database failed\n");
					res.signal=0;
				}
				else{
					/*parsing data to json string*/
					root_val_x=json_value_init_object();
					root_obj_x=json_value_get_object(root_val_x);
					json_object_dotset_string(root_obj_x,"join.gwaddr",gwaddr);
					json_object_dotset_string(root_obj_x,"join.appeui",appeui_hex);
					json_object_dotset_string(root_obj_x,"join.deveui",deveui_hex);
					json_object_dotset_string(root_obj_x,"join.accept.frame",frame_payload_b64);
					json_object_dotset_string(root_obj_x,"join.accept.nwkskey",nwkskey_hex);
					json_str=json_serialize_to_string_pretty(root_val_x);
					strcpy(res.json_string,json_str);
					strcpy(res.appSKey,appskey_hex);
					json_free_serialized_string(json_str);
					json_value_free(root_val_x);
				}
			}
			json_value_free(root_val);
			/*close the connection to db*/
			close_db(&mysql);
		}
	}
	/*handling app json data*/
	else{
		res.signal=2;
		snprintf(msgname,sizeof(msgname),"###############common_message_%d:###############\n",index);
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
			devAddr=(uint32_t)json_value_get_number(val);
			snprintf(tempstr,sizeof(tempstr)," devaddress:%.0f",json_value_get_number(val));
			strcat(content,tempstr);
		}
		val=json_object_get_value(app_obj,"dir");
		if(val!=NULL){
			snprintf(tempstr,sizeof(tempstr)," direction:%s",json_value_get_string(val));
			strcat(content,tempstr);
		}
		udata_obj=json_object_get_object(app_obj,"userdata");
		if(udata_obj==NULL){
			MSG("WARNING: [up] message received contains no \"userdata\" in \"app\"\n");
		}
		else{
			val=json_object_get_value(udata_obj,"seqno");
			if(val!=NULL){
				seqno=(uint32_t)json_value_get_number(val);
				snprintf(tempstr,sizeof(tempstr)," sequenceNo:%.0f",json_value_get_number(val));
				strcat(content,tempstr);
			}
			val=json_object_get_value(udata_obj,"port");
			if(val!=NULL){
				snprintf(tempstr,sizeof(tempstr)," port:%.0f",json_value_get_number(val));
				strcat(content,tempstr);
			}
			val=json_object_get_value(udata_obj,"payload");
			if(val!=NULL){
				str=json_value_get_string(val);
				/*convert to binary*/
				psize=b64_to_bin(str,strlen(str),userdata_payload,LORAMAC_FRAME_MAXPAYLOAD);
				if(connect_db("localhost","root","1005541787","LoRaWAN",&mysql)==FAILED){
					MSG("WARNING: [up] access the database failed\n");
				}
				/*query the appkey from the database*/
				if(query_db_by_deveui("appskey","appsdevinfo",deveui_hex,&mysql,appSKey,16)==FAILED){
					/*the device isn't registered in the application server*/
					MSG("WARNING: [up] query the database failed\n");
				}
				else{
					/*compute the LoRaMAC frame payload decryption*/
					LoRaMacPayloadDecrypt(userdata_payload,psize,appSKey,devAddr,UP,seqno,dec_userdata);
					strcat(content," payload:");
					for(j=0;j<psize;j++){
						snprintf(tempstr,sizeof(tempstr),"0x%02x ",dec_userdata[j]);
						strcat(content,tempstr);
					}
				}
			}
			devx_obj=json_object_get_object(udata_obj,"devx");
			if(devx_obj==NULL){
				MSG("WARNING: [up] message received contains no \"devx\" in \"userdata\"\n");
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
			gwrx_obj=json_object_get_object(udata_obj,"gwrx");
			if(gwrx_obj==NULL){
				MSG("WARNING: [up] message received contains no \"gwrx\" in \"userdata\"\n");
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
		}
		json_value_free(root_val);
		/*close the connection to db*/
		close_db(&mysql);
	}
	MSG("%s\n",content);
	if(res.signal==1){
		MSG("###########Message that will be transfered to the network server#############\n");
		MSG("%s\n",res.json_string);
	}
	return res;
}
void prepare_frame(uint8_t *frame_payload,uint16_t devNonce,uint8_t* appKey,uint32_t devAddr,uint8_t *nwkSKey,uint8_t *appSKey){
	uint8_t  payload[JOIN_ACC_SIZE];
	uint8_t index=0;
	LoRaMacHeader_t hdr;
	int rand_num;
	uint8_t appNonce[3];
	uint8_t NetID[3]={0x00,0x00,0x00};
	LoRaMacDLSettings_t dls;
	LoRaMacRxDelay_t rxd;
	uint32_t mic;

	/*the header of mac LoRaMac packet*/
	hdr.Value=0;
	hdr.Bits.MType=FRAME_TYPE_JOIN_ACCEPT;
	payload[index]=hdr.Value;
	/*generate the appNonce,here just for experienment using random number*/
	srand(time(NULL));
	rand_num=(uint32_t)rand();
	/*AppNonce*/
	payload[++index]=appNonce[2]=rand_num&0xFF;
	payload[++index]=appNonce[1]=(rand_num>>8)&0xFF;
	payload[++index]=appNonce[0]=(rand_num>>16)&0xFF;
	/*NetID*/
	payload[++index]=NetID[2];
	payload[++index]=NetID[1];
	payload[++index]=NetID[0];
	/*DevAddr*/
	payload[++index]=devAddr&0xFF;
	payload[++index]=(devAddr>>8)&0xFF;
	payload[++index]=(devAddr>>16)&0xFF;
	payload[++index]=(devAddr>>24)&0xFF;
	/*DLSettings*/
	dls.Value=0;
	dls.Bits.RX1DRoffset=0;
	dls.Bits.RX2DataRate=0;
	payload[++index]=dls.Value;
	/*RxDelay*/
	rxd.Value=0;
	rxd.Bits.Del=1;
	payload[++index]=rxd.Value;

	LoRaMacJoinComputeMic(payload,(uint16_t)17-4,appKey,&mic);
	payload[++index]=mic&0xFF;
	payload[++index]=(mic>>8)&0xFF;
	payload[++index]=(mic>>16)&0xFF;
	payload[++index]=(mic>>24)&0xFF;
	/*compute the two session key
	 *the second argument is corresponding to the LoRaMac.c(v4.0.0)
	 *it seems that it makes a mistake,because the byte-order is adverse
	*/
	LoRaMacJoinComputeSKeys(appKey,payload+1,devNonce,nwkSKey,appSKey);
	/*encrypt join accept message*/
	LoRaMacJoinEncrypt(payload+1,(uint16_t)JOIN_ACC_SIZE-1,appKey,frame_payload+1);
	frame_payload[0]=payload[0];
}

void destroy_msg(void* msg){
	struct msg* data=(struct msg*)msg;
	free(data->json_string);
}

void assign_msg(void* data,const void* msg){
		struct msg* data_x=(struct msg*)data;
		struct msg* msg_x=(struct msg*)msg;
		data_x->json_string=msg_x->json_string;
}

