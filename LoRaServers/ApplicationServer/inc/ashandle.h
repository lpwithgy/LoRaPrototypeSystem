/*
 * ashandle.h
 *
 *  Created on: Mar 5, 2016
 *      Author: paullu
 */

#ifndef ASHANDLE_H_
#define ASHANDLE_H_



#endif /* ASHANDLE_H_ */

#include <stdint.h>
#include "generic_list.h"

#define LORAMAC_FRAME_MAXPAYLOAD              255
#define JSON_MAX  1024 /*1024 is big enough, not calculate the accurate size of json string*/

/*structure used for transfering the connected socked fd*/
struct arg{
	int connfd;
};
/*structure used for sending and receiving data*/
struct pkt{
	char json_content[JSON_MAX];
};

struct msg{
	char*  json_string;
};

/*structure used for storing the json string and appSKey*/
struct res_handle{
	uint8_t signal;/*indicates whether the struct stores the infomation*/
	char appSKey[33];/*store the appSkey which will be transferred the network controller*/
	char json_string[JSON_MAX];
};

/*destory the linked list node*/
void destroy_msg(void* msg);

/*clone the linked list node*/
void assign_msg(void* data,const void* msg);

/*handle the message sent by the networkserver*/
struct res_handle msg_handle(char*,int);


