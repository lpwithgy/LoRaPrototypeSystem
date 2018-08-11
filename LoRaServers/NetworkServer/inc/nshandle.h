/*
 * nshandle.h
 *
 *  Created on: Mar 1, 2016
 *      Author: paullu
 *      define the some useful functions and data struct
 */

#ifndef NSHANDLE_H_
#define NSHANDLE_H_



#endif /* NSHANDLE_H_ */

#include <stdint.h>
#include "db_mysql.h"
#include "generic_list.h"

#define NB_PKT_MAX  8 /*the max size of the "rxpk" array*/
#define BUFF_SIZE ((540 * NB_PKT_MAX) + 30)

#define JSON_MAX  1024 /*1024 is big enough, not calculate the accurate size of json string*/

#define APPLICATION_SERVER     1
#define NETWORK_CONTROLLER     2
#define BOTH                   3
#define ERR                    4
#define IGNORE                 5

#define NO_DELAY               0
#define JOIN_ACCEPT_DELAY      6000000 /*6s*/
#define RECV_DELAY             2000000 /*2s*/

#define CLASS_A                0
#define CLASS_B                1
#define CLASS_C                2

/* packet payload  struct to handle*/
struct pkt_info{
	uint8_t pkt_payload[BUFF_SIZE-12];/*packet payload*/
	int  pkt_no;/*packet number*/
	char gwaddr[16];/*gateway address*/
};

/*meta data of the packet */
struct metadata{
	char     gwaddr[16];
	uint32_t tmst;     /*raw time stamp*/
	char     time[28];
	uint8_t  chan;     /* IF channel*/
	uint8_t  rfch;     /* RF channel*/
	double 	 freq;     /*frequency of IF channel*/
	uint8_t  stat;     /* packet status*/
	char     modu[5];  /* modulation:LORA or FSK*/
	char     datrl[10];/* data rate for LORA*/
	uint32_t datrf;    /*data rate for FSK*/
	char     codr[4];  /*ECC coding rate*/
	float    lsnr;     /*SNR in dB*/
	float    rssi;	   /*rssi in dB*/
	uint16_t size;     /*payload size in bytes*/
};

/* json data return*/
struct jsondata{
	int to;				/*which server to send to*/
	/*
	 * in default,json_string_as is sent for application server
	 * json_string_ns is sent for network controller
	 */
	char json_string_as[JSON_MAX];
	char json_string_nc[JSON_MAX];
	uint32_t devaddr;
	char deveui_hex[17];
	bool join;/*is it a join request message*/
};

/*structure used to transfer connected sockfd*/
struct arg{
	int connfd;
};

struct th_check_arg{
	uint32_t devaddr;
	uint32_t tmst;
};

/*data structure of linked list node*/
struct msg_up{
	char*  json_string;
};

struct msg_down{
	char*  json_string;
	char*  gwaddr;
};

struct msg_delay{
	uint32_t devaddr;
	char deveui_hex[17];
	char* frame;
	int size;
};

struct msg_join{
	char deveui_hex[17];
	uint32_t tmst;
};

/*structure used for sending and receiving data*/
struct pkt{
	char json_content[JSON_MAX];
};

/*compare the node element*/
int compare_msg_down(const void* data,const void* key);
int compare_msg_delay(const void* data,const void* key);
int compare_msg_join(const void* data,const void*key);

/*destory the linked list node
 * free the memory allocated in the heap
 */
void destroy_msg_up(void* msg);
void destroy_msg_down(void* msg);
void destroy_msg_delay(void* msg);

/*shallow copy
 * for the data allocated in the heap,
 * just copy the pointer
 */
void assign_msg_up(void* data,const void* msg);
void assign_msg_down(void* data,const void* msg);
void assign_msg_delay(void* data,const void* msg);
void assign_msg_join(void* data,const void* msg);

/*deep copy*/
void copy_msg_down(void* data,const void* msg);
void copy_msg_delay(void* data,const void* msg);
void copy_msg_join(void* data,const void* msg);

/*recognize the type of message and parse it to json string*/
void msg_handle(struct jsondata*,struct metadata*,uint8_t*);

/*packet the data that will be sent to the gateaway*/
bool serialize_msg_to_gw(const char* data,int size,const char* deveui_hex,char* json_data,char* gwaddr,uint32_t tmst,int delay);

