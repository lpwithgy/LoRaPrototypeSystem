/*
 * nshandle.c
 *
 *  Created on: Mar 2, 2016
 *      Author: paullu
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "nshandle.h"
#include "LoRaMacCrypto.h"
#include "parson.h"


/*keys just for testing*/
#define DEFAULT_APPEUI  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
#define DEFALUT_DEVEUI	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }

#define LORAMAC_FRAME_MAXPAYLOAD              255

/*stream direction*/
#define UP                                     0
#define DOWN                                   1

#define MAX_NB_B64                            341 /*255 bytes=340 char in b64*/
#define RANGE                                 1000000 /*1s*/


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
 * LoRaMAC frame control field definition (FCtrl)
 * Copy from LoRaMAC.h
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
/*
//session key and app keys
uint8_t nwkSKey[16]={ 0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6, 0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C };
uint8_t appSKey[16]={ 0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6, 0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C };
uint8_t appKey[16]= { 0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6, 0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C };
*/
/*reverse memory copy*/
static void revercpy( uint8_t *dst, const uint8_t *src, int size );
/*transform the array of uint8_t to hexadecimal string*/
static void i8_to_hexstr(uint8_t* uint,char* str,int size);

void msg_handle(struct jsondata* result,struct metadata* meta,uint8_t* payload){

	uint8_t nwkSKey[16];
	uint8_t appSKey[16];
	uint8_t appKey[16];

	int size=meta->size; 	/*the length of payload*/
	LoRaMacHeader_t macHdr; /*the MAC header*/
	/* json args for parsing data to json string*/
	JSON_Value *root_value;
	JSON_Object * root_object;
	char* str=NULL;
	MYSQL mysql;/*access the MYSQL database*/

	/*typical fields for join request message*/
	uint8_t appEui[8];
	uint8_t devEui[8];
	uint16_t devNonce=0;
	/*store the hexadecimal format of eui*/
	char  appEui_hex[17];
	char  devEui_hex[17];

	/*typical fields for confirmed/unconfirmed message*/
	uint32_t devAddr=0;
	LoRaMacFrameCtrl_t fCtrl;
	uint32_t fopts_len;/*the size of foptions field*/
	uint8_t adr;       /*indicate if ADR is permitted*/
	uint16_t upCnt=0;
	uint8_t fopts[15];
	uint8_t fport;
	uint8_t fpayload[LORAMAC_FRAME_MAXPAYLOAD];/*the frame payload*/
	char fpayload_b64[341];
	char fopts_b64[21];
	char* maccmd;
	int  encmd; /*indicate whether the MAC command is encrypted */
	int size_b64; /*the size of the fpayload_b64*/

	uint32_t mic=0;
	uint32_t cal_mic;/*the MIC value calculated by the payload*/
	int i;

	/*varibles used to distinguish the repeated message*/
	unsigned int pre_timestamp=0;
	unsigned int pre_devNonce=0;
	unsigned int pre_upCnt=0;
	unsigned int pre_rfchannel=2;
	unsigned int pre_tmst_max;
	unsigned int pre_tmst_min;


	//fpayload_b64=calloc(341,sizeof(char));
	//fopts_b64=calloc(21,sizeof(char));
	/*analyse the type of the message*/
	macHdr.Value=payload[0];
	switch(macHdr.Bits.MType){
		/*join request message*/
		case FRAME_TYPE_JOIN_REQ:{
			revercpy(appEui,payload+1,8);
			revercpy(devEui,payload+9,8);
			i8_to_hexstr(appEui,appEui_hex,8);
			i8_to_hexstr(devEui,devEui_hex,8);
			devNonce|=(uint16_t)payload[17];
			devNonce|=((uint16_t)payload[18])<<8;
			mic|=(uint32_t)payload[19];
			mic|=((uint32_t)payload[20])<<8;
			mic|=((uint32_t)payload[21])<<16;
			mic|=((uint32_t)payload[22])<<24;

			if(connect_db("localhost","root","1005541787","LoRaWAN",&mysql)==FAILED){
				MSG("WARNING: [up] access the database failed\n");
			}
			/*judge whether it is a  repeated message*/
			if(query_db_by_deveui_uint("tmst","lastjoininfo",devEui_hex,&mysql,&pre_timestamp)==FAILED||
			   query_db_by_deveui_uint("devnonce","lastjoininfo",devEui_hex,&mysql,&pre_devNonce)==FAILED){
				MSG("WARNING: [up] query the database failed\n");
				/*The device has not registered in the network server */
				result->to=IGNORE;
				close_db(&mysql);
				break;
			}
			else{
				if(pre_timestamp<RANGE)
					pre_tmst_min=0;
				else
					pre_tmst_min=pre_timestamp-RANGE;
				pre_tmst_max=pre_timestamp+RANGE;
				if(devNonce==(uint16_t)pre_devNonce){
					/*it is a repeated message,just ignore*/
					result->to=IGNORE;
					close_db(&mysql);
					break;
				}
				else{
					if(update_db_by_deveui_uint("tmst","lastjoininfo",devEui_hex,&mysql,meta->tmst)==FAILED||
					   update_db_by_deveui_uint("devnonce","lastjoininfo",devEui_hex,&mysql,devNonce)==FAILED){
						MSG("WARNING: [up] update the database failed\n");
						result->to=IGNORE;
						close_db(&mysql);
						break;
					}
				}
			}
			if(query_db_by_deveui("appkey","nsdevinfo",devEui_hex,&mysql,appKey,16)==FAILED){
				MSG("WARNING: [up] query the database failed\n");
				/*The device has not registered in the network server */
				result->to=IGNORE;
				close_db(&mysql);
				break;
			}
			LoRaMacJoinComputeMic(payload,23-4,appKey,&cal_mic);
			/*if mic is wrong,the join request will be ignored*/
			if(mic!=cal_mic){
				MSG("WARNING: [up] join request payload mic is wrong,just ignore it\n");
				result->to=IGNORE;
				close_db(&mysql);
				break;
			}
			else{
				result->to=APPLICATION_SERVER;
				root_value=json_value_init_object();
				root_object=json_value_get_object(root_value);
				json_object_dotset_string(root_object,"join.gwaddr",meta->gwaddr);
				json_object_dotset_string(root_object,"join.appeui",appEui_hex);
				json_object_dotset_string(root_object,"join.deveui",devEui_hex);
				json_object_dotset_number(root_object,"join.request.devnonce",devNonce);
				/*assign the device address by rand(),just for experiment*/
				srand(time(NULL));
				devAddr=rand();
				/*update the devaddr filed in the database*/
				if(update_db_by_deveui_uint("devaddr","nsdevinfo",devEui_hex,&mysql,(unsigned int)devAddr)==FAILED){
					MSG("WARNING: [up] update the database failed\n");
					result->to=IGNORE;
					close_db(&mysql);
					json_value_free(root_value);
					break;
				}
				/*update the gwaddr filed in the table transarg
				 *reset other fileds to default value
				 */
				if(update_db_by_deveui("gwaddr","transarg",devEui_hex,&mysql,meta->gwaddr,1)==FAILED ||
				   update_db_by_deveui_uint("rx1datarate","transarg",devEui_hex,&mysql,0)==FAILED ||
				   update_db_by_deveui_uint("rx2datarate","transarg",devEui_hex,&mysql,0)==FAILED ||
				   update_db_by_deveui_uint("rx2frequency","transarg",devEui_hex,&mysql,8695250)==FAILED||
				   update_db_by_deveui_uint("delay","transarg",devEui_hex,&mysql,1)==FAILED ||
				   update_db_by_deveui_uint("upcnt","lastappinfo",devEui_hex,&mysql,0)==FAILED ||
				   update_db_by_deveui_uint("downcnt","nsdevinfo",devEui_hex,&mysql,1)==FAILED){
					MSG("WARNING: [up] update the database failed\n");
					result->to=IGNORE;
					close_db(&mysql);
					json_value_free(root_value);
					break;
				}
				result->join=true;
				strcpy(result->deveui_hex,devEui_hex);
				json_object_dotset_number(root_object,"join.request.devaddr",devAddr);
				str=json_serialize_to_string_pretty(root_value);
				strcpy(result->json_string_as,str);
				json_free_serialized_string(str);
				json_value_free(root_value);
			}
			close_db(&mysql);
			break;
		}
		/*unconfirmed message*/
		case FRAME_TYPE_DATA_UNCONFIRMED_UP:/*fall through,just handle like unconfirmed message*/
		/*confirmed message*/
		case FRAME_TYPE_DATA_CONFIRMED_UP:{
			result->join=false;
			devAddr|=(uint32_t)payload[1];
			devAddr|=((uint32_t)payload[2])<<8;
			devAddr|=((uint32_t)payload[3])<<16;
			devAddr|=((uint32_t)payload[4])<<24;
			fCtrl.Value=payload[5];
			fopts_len=fCtrl.Bits.FOptsLen;
			adr=fCtrl.Bits.Adr;
			upCnt|=(uint16_t)payload[6];
			upCnt|=((uint16_t)payload[7])<<8;
			fport=payload[8+fopts_len];
			memcpy(fpayload,payload+9+fopts_len,size-13-fopts_len);
			mic|=(uint32_t)payload[size-4];
			mic|=((uint32_t)payload[size-3])<<8;
			mic|=((uint32_t)payload[size-2])<<16;
			mic|=((uint32_t)payload[size-1])<<24;

			if(connect_db("localhost","root","1005541787","LoRaWAN",&mysql)==FAILED){
				MSG("WARNING: [up] access the database failed\n");
			}
			/*query the deveui according to the devaddr*/
			if(query_db_by_addr_str("deveui","nsdevinfo",devAddr,&mysql,devEui_hex)==FAILED){
				MSG("WARNING: [up] query the database failed\n");
				/*The device has not joined in the LoRaWAN */
				result->to=IGNORE;
				close_db(&mysql);
				break;
			}
			/*judge whether it is a  repeated message*/
			if(query_db_by_deveui_uint("tmst","lastappinfo",devEui_hex,&mysql,&pre_timestamp)==FAILED||
			   query_db_by_deveui_uint("upcnt","lastappinfo",devEui_hex,&mysql,&pre_upCnt)==FAILED||
			   query_db_by_deveui_uint("rfch","lastappinfo",devEui_hex,&mysql,&pre_rfchannel)==FAILED){
				MSG("WARNING: [up] query the database failed\n");
				result->to=IGNORE;
				close_db(&mysql);
				break;
			}
			else{
				if(pre_timestamp<RANGE)
					pre_tmst_min=0;
				else
					pre_tmst_min=pre_timestamp-RANGE;
				pre_tmst_max=pre_timestamp+RANGE;
				if(upCnt==(uint16_t)pre_upCnt && meta->rfch==(uint8_t)pre_rfchannel){
					/*it is a repeated message,just ignore*/
					MSG("INFO: [up] reduplicated unconfirmed message\n");
					result->to=IGNORE;
					close_db(&mysql);
					break;
				}
				else{
					if(update_db_by_deveui_uint("tmst","lastappinfo",devEui_hex,&mysql,meta->tmst)==FAILED||
					   update_db_by_deveui_uint("upcnt","lastappinfo",devEui_hex,&mysql,upCnt)==FAILED||
					   update_db_by_deveui_uint("rfch","lastappinfo",devEui_hex,&mysql,meta->rfch)==FAILED){
						MSG("WARNING: [up] update the database failed\n");
						result->to=IGNORE;
						close_db(&mysql);
						break;
					}
				}
			}
			if(query_db_by_addr("nwkskey","nsdevinfo",devAddr,&mysql,nwkSKey,16)==FAILED){
				MSG("WARNING: [up] query the database failed\n");
				/*The device has not joined in the LoRaWAN */
				result->to=IGNORE;
				close_db(&mysql);
				break;
			}
			LoRaMacComputeMic(payload,meta->size-4,nwkSKey,devAddr,UP,(uint32_t)upCnt,&cal_mic);
			if(cal_mic!=mic){
				MSG("WARNING: [up] push data payload mic is wrong,just ignore it\n");
				result->to=IGNORE;
				close_db(&mysql);
				break;
			}
			size_b64=bin_to_b64(fpayload,size-13-fopts_len,fpayload_b64,MAX_NB_B64);
			/*when the message contains both MAC command and userdata*/
			MSG("len:%d\n",fopts_len);
			if(fopts_len>0){
				memcpy(fopts,payload+8,fopts_len);
				if(fport==0){
					MSG("WARNING: [up] push data payload fport is wrong,just ignore it\n");
					result->to=IGNORE;
					close_db(&mysql);
					break;
				}
				result->to=BOTH;
				size_b64=bin_to_b64(fopts,fopts_len,fopts_b64,21);
				maccmd=malloc(size_b64+1);
				memcpy(maccmd,fopts_b64,size_b64+1);
				encmd=0;
			}
			else{
				/*when the message contains only MAC command */
				if(fport==0){
					result->to=NETWORK_CONTROLLER;
					encmd=1;
					maccmd=malloc(size_b64+1);
					memcpy(maccmd,fpayload_b64,size_b64+1);
				}
				/*when the message contains only user data*/
				else{
					result->to=APPLICATION_SERVER;
				}
			}
			/*update the gwaddr filed in the table transarg*/
			if(update_db_by_deveui("gwaddr","transarg",devEui_hex,&mysql,meta->gwaddr,1)==FAILED){
				MSG("WARNING: [up] update the database failed\n");
				result->to=IGNORE;
				close_db(&mysql);
				break;
			}
			/*packet the json string which will send to the application server*/
			if(result->to==APPLICATION_SERVER || result->to==BOTH){
				result->devaddr=devAddr;
				root_value=json_value_init_object();
				root_object=json_value_get_object(root_value);
				json_object_dotset_string(root_object,"app.gwaddr",meta->gwaddr);
				json_object_dotset_string(root_object,"app.deveui",devEui_hex);
				json_object_dotset_number(root_object,"app.devaddr",devAddr);
				json_object_dotset_string(root_object,"app.dir","up");
				json_object_dotset_number(root_object,"app.userdata.seqno",upCnt);
				json_object_dotset_number(root_object,"app.userdata.port",fport);
				json_object_dotset_string(root_object,"app.userdata.payload",fpayload_b64);
				json_object_dotset_number(root_object,"app.userdata.devx.freq",meta->freq);
				json_object_dotset_string(root_object,"app.userdata.devx.modu",meta->modu);
				if(strcmp(meta->modu,"LORA")==0){
					json_object_dotset_string(root_object,"app.userdata.devx.datr",meta->datrl);
				}
				else{
					json_object_dotset_number(root_object,"app.userdata.devx.datr",meta->datrf);
				}
				json_object_dotset_string(root_object,"app.userdata.devx.codr",meta->codr);
				json_object_dotset_boolean(root_object,"app.userdata.devx.adr",adr);
				json_object_dotset_string(root_object,"app.userdata.gwrx.time",meta->time);
				json_object_dotset_number(root_object,"app.userdata.gwrx.chan",meta->chan);
				json_object_dotset_number(root_object,"app.userdata.gwrx.rfch",meta->rfch);
				json_object_dotset_number(root_object,"app.userdata.gwrx.rssi",meta->rssi);
				json_object_dotset_number(root_object,"app.userdata.gwrx.lsnr",meta->lsnr);
				str=json_serialize_to_string_pretty(root_value);
				strcpy(result->json_string_as,str);
				json_free_serialized_string(str);
				json_value_free(root_value);
			}

			/*packet the json string that will send to the network controller */
			if(result->to==NETWORK_CONTROLLER || result->to==BOTH){
				result->devaddr=devAddr;
				root_value=json_value_init_object();
				root_object=json_value_get_object(root_value);
				json_object_dotset_string(root_object,"app.gwaddr",meta->gwaddr);
				json_object_dotset_string(root_object,"app.deveui",devEui_hex);
				json_object_dotset_number(root_object,"app.devaddr",devAddr);
				json_object_dotset_string(root_object,"app.dir","up");
				json_object_dotset_number(root_object,"app.seqno",upCnt);
				json_object_dotset_number(root_object,"app.devx.freq",meta->freq);
				json_object_dotset_string(root_object,"app.devx.modu",meta->modu);
				if(strcmp(meta->modu,"LORA")==0){
					json_object_dotset_string(root_object,"app.devx.datr",meta->datrl);
				}
				else
					json_object_dotset_number(root_object,"app.devx.datr",meta->datrf);
				json_object_dotset_string(root_object,"app.devx.codr",meta->codr);
				json_object_dotset_boolean(root_object,"app.devx.adr",adr);
				json_object_dotset_string(root_object,"app.gwrx.time",meta->time);
				json_object_dotset_number(root_object,"app.gwrx.chan",meta->chan);
				json_object_dotset_number(root_object,"app.gwrx.rfch",meta->rfch);
				json_object_dotset_number(root_object,"app.gwrx.rssi",meta->rssi);
				json_object_dotset_number(root_object,"app.gwrx.lsnr",meta->lsnr);
				json_object_dotset_string(root_object,"app.maccmd.command",maccmd);
				json_object_dotset_boolean(root_object,"app.maccmd.isencrypt",encmd);
				str=json_serialize_to_string_pretty(root_value);
				strcpy(result->json_string_nc,str);
				json_free_serialized_string(str);
				json_value_free(root_value);
				free(maccmd);
			}
			close_db(&mysql);
			break;
		}
		/*proprietary message*/
		case FRAME_TYPE_PROPRIETARY:{
			memcpy(fpayload,payload+1,size-1);
			break;
		}
	}
	if(result->to==APPLICATION_SERVER|| result->to==BOTH){
		MSG("###########Message that will be transfered to the application server#############\n");
		MSG("%s\n",result->json_string_as);
	}
	if(result->to==NETWORK_CONTROLLER|| result->to==BOTH){
		MSG("###########Message that will be transfered to the network controller#############\n");
		MSG("%s\n",result->json_string_nc);
	}
}

bool serialize_msg_to_gw(const char* data,int size,const char* deveui_hex,char* json_data,char* gwaddr,uint32_t tmst,int delay){
	MYSQL  mysql;
	JSON_Value  *root_val_x=NULL;
	JSON_Object *root_obj_x=NULL;
	unsigned int rx1_dr;
	unsigned int rx2_dr;
	unsigned int rx2_freq;
	char dr[16];
	double freq;
	struct timespec time;/*storing local timestamp*/
	char* json_str=NULL;
	if(connect_db("localhost","root","1005541787","LoRaWAN",&mysql)==FAILED){
		 MSG("WARNING: [down] access the database failed\n");
	 }
	if(query_db_by_deveui_str("gwaddr","transarg",deveui_hex,&mysql,gwaddr)==FAILED){
		MSG("WARNING: [down] query the database failed\n");
		close_db(&mysql);
		return false;
	}
	if(query_db_by_deveui_uint("rx1datarate","transarg",deveui_hex,&mysql,&rx1_dr)==FAILED ||
		query_db_by_deveui_uint("rx2datarate","transarg",deveui_hex,&mysql,&rx2_dr)==FAILED ||
		query_db_by_deveui_uint("rx2frequency","transarg",deveui_hex,&mysql,&rx2_freq)==FAILED){
		MSG("WARNING: [down] query the database failed\n");
		close_db(&mysql);
		return false;
	}
	close_db(&mysql);
	switch(rx2_dr){
		case 0:{
			strcpy(dr,"SF12BW125");
			break;
		}
		case 1:{
			strcpy(dr,"SF11BW125");
			break;
		}
		case 2:{
			strcpy(dr,"SF10BW125");
			break;
		}
		case 3:{
			strcpy(dr,"SF9BW125");
			break;
		}
		case 4:{
			strcpy(dr,"SF8BW125");
			break;
		}
		case 5:{
			strcpy(dr,"SF7BW125");
			break;
		}
		case 6:{
			strcpy(dr,"SF7BW250");
			break;
		}
		case 7:{
			strcpy(dr,"FSK");
			break;
		}
		default:{
			strcpy(dr,"SF12BW125");
		}
	}
	freq=(double)rx2_freq/(double)10000;
	MSG("%s %.4f\n",dr,freq);
	clock_gettime(CLOCK_REALTIME,&time);
	root_val_x=json_value_init_object();
	root_obj_x=json_value_get_object(root_val_x);
	if(delay==NO_DELAY){
		json_object_dotset_boolean(root_obj_x,"txpk.imme",true);
	}
	else{
		json_object_dotset_number(root_obj_x,"txpk.tmst",tmst+delay);
	}
	json_object_dotset_number(root_obj_x,"txpk.freq",freq);
	json_object_dotset_number(root_obj_x,"txpk.rfch",0);
	json_object_dotset_number(root_obj_x,"txpk.powe",14);
	json_object_dotset_string(root_obj_x,"txpk.modu","LORA");
	json_object_dotset_string(root_obj_x,"txpk.datr",dr);
	json_object_dotset_string(root_obj_x,"txpk.codr","4/5");
	json_object_dotset_boolean(root_obj_x,"txpk.ipol",1);
	json_object_dotset_number(root_obj_x,"txpk.size",size);
	json_object_dotset_string(root_obj_x,"txpk.data",data);
	json_str= json_serialize_to_string_pretty(root_val_x);
	strncpy(json_data,json_str,strlen(json_str)+1);
	json_free_serialized_string(json_str);
	json_value_free(root_val_x);
	return true;
}

int compare_msg_down(const void* data,const void* key){
	return strcmp(((struct msg_down*)data)->gwaddr,(const char*)key);
}

int compare_msg_delay(const void* data,const void* key){
	if(((struct msg_delay*)data)->devaddr==*(uint32_t*)key)
		return  0;
	else
		return  1;
}

int compare_msg_join(const void* data,const void*key){
	return strcmp(((struct msg_join*)data)->deveui_hex,(const char*)key);
}

void assign_msg_up(void* data,const void* msg){
	struct msg_up* data_x=(struct msg_up*)data;
	struct msg_up* msg_x=(struct msg_up*)msg;
	data_x->json_string=msg_x->json_string;
}

void assign_msg_down(void* data,const void* msg){
	struct msg_down* data_x=(struct msg_down*)data;
	struct msg_down* msg_x=(struct msg_down*)msg;
	//strcpy(data_x->json_string,msg_x->json_string);
	//strcpy(data_x->gwaddr,msg_x->gwaddr);
	data_x->gwaddr=msg_x->gwaddr;
	data_x->json_string=msg_x->json_string;
}

void assign_msg_join(void* data,const void* msg){
	struct msg_join* data_x=(struct msg_join*)data;
	struct msg_join* msg_x=(struct msg_join*)msg;
	strcpy(data_x->deveui_hex,msg_x->deveui_hex);
	data_x->tmst=msg_x->tmst;
}

void assign_msg_delay(void* data,const void* msg){
	struct msg_delay* data_x=(struct msg_delay*)data;
	struct msg_delay* msg_x=(struct msg_delay*)msg;
	data_x->devaddr=msg_x->devaddr;
	strcpy(data_x->deveui_hex,msg_x->deveui_hex);
	data_x->frame=msg_x->frame;
	data_x->size=msg_x->size;
}

void copy_msg_down(void* data,const void* msg){
	struct msg_down* data_x=(struct msg_down*)data;
	struct msg_down* msg_x=(struct msg_down*)msg;
	strcpy(data_x->json_string,msg_x->json_string);
	strcpy(data_x->gwaddr,msg_x->gwaddr);
}

void copy_msg_delay(void* data,const void* msg){
	struct msg_delay* data_x=(struct msg_delay*)data;
	struct msg_delay* msg_x=(struct msg_delay*)msg;
	data_x->devaddr=msg_x->devaddr;
	strcpy(data_x->deveui_hex,msg_x->deveui_hex);
	strcpy(data_x->frame,msg_x->frame);
	data_x->size=msg_x->size;
}

void copy_msg_join(void* data,const void* msg){
	assign_msg_join(data,msg);
}

void destroy_msg_up(void* msg){
	struct msg_up* message=(struct msg_up*)msg;
	free(message->json_string);
}

void destroy_msg_down(void* msg){
	struct msg_down* message=(struct msg_down*)msg;
	free(message->json_string);
	free(message->gwaddr);
}

void destroy_msg_delay(void* msg){
	struct msg_delay* message=(struct msg_delay*)msg;
	free(message->frame);
}

void i8_to_hexstr(uint8_t* uint,char* str,int size){
	/*in case that the str has a value,strcat() seems not safe*/
	bzero(str,size*2+1);
	char tempstr[3];
	int i;
	for(i=0;i<size;i++){
		snprintf(tempstr,sizeof(tempstr),"%02x",uint[i]);
		strcat(str,tempstr);
	}
}

/*reverse memory copy*/
void revercpy( uint8_t *dst, const uint8_t *src, int size )
{
    dst = dst + ( size - 1 );
    while( size-- )
    {
        *dst-- = *src++;
    }
}

