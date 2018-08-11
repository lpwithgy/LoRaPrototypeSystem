/*
 * main.c
 *
 *  Created on: Mar 4, 2016
 *      Author: paullu
 */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netdb.h>

#include <sys/select.h>
#include <time.h>
#include <signal.h>
#include <sys/time.h>

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#include "base64.h"
#include "parson.h"
#include "ashandle.h"

#define STRINGIFY(x)	#x
#define STR(x)			STRINGIFY(x)
#define MSG(args...)    printf(args)

#define JSON_MAX   		 1024/*same as the size defined in nshandle.h*/
#define LISTENQ     	 16
#define CONNFD_NUM_MAX   16

#define NET_SERV_ADDR 192.168.0.102
#define APP_SERV_ADDR 192.168.0.102
#define APP_PORT_UP        9000
#define APP_PORT_DOWN      9002

/*interval time defined to try to connect the network server*/
#define SOCKET_INTERVAL_S   10
#define SOCKET_INTERVAL_MS  0
/*interval time defined to wait for filling data to the linked list*/
#define  SEND_INTERVAL_S   1
#define  SEND_INTERVAL_MS  0

/* --- PRIVATE VARIABLES ------------------------------------------- */
/*network configuration variables*/
static char appserv_addr[64]=STR(APP_SERV_ADDR);
static char netserv_addr[64]=STR(NET_SERV_ADDR);
static char port_up[8]=STR(APP_PORT_UP);
static char port_down[8]=STR(APP_PORT_DOWN);
static int listenfd_up;/* listening socket for upstream from network server*/
static int connfd_up;/*connecting socket for upstream form network server*/
static int sockfd_down;/* socket for downstream to network server*/
static int connfds[CONNFD_NUM_MAX];/*array for connected socket file descriptor*/
static int conn_num=0;

/*exit signal*/
static bool exit_sig=false;

/*mutex*/
static pthread_mutex_t mx_ns_list=PTHREAD_MUTEX_INITIALIZER;/*mutex lock for controlling acess to the linked list*/
static pthread_mutex_t mx_db=PTHREAD_MUTEX_INITIALIZER;/*mutex lock for controlling access to the database*/
static pthread_mutex_t mx_conn=PTHREAD_MUTEX_INITIALIZER;/*mutex lock for controlling rewriting gloable variable conn_num*/

linked_list  ns_list;/*head of the linked list storing json string for NS*/

/* --- PRIVATE FUNCTIONS ------------------------------------------- */
/*signal handle function*/
static void signal_handle(int signo);
/* threads*/
static void  thread_up();/* thread for establishing connection with network server in upstream direction*/
static void  thread_down();/*thread for sending downstream to network server*/
static void  thread_up_handle(void* arg);/*thread for receiving and handling upstream message*/

static void tcp_connect(const char* servaddr,const char* port,int* sockfd);
static void tcp_bind(const char* servaddr,const char* port,int* listenfd);

/*timer*/
static void set_timer(int sec,int msec);

/* --- MAIN FUNCTION ------------------------------------------- */
int main(int argc,char** argv){
	/* threads*/
	pthread_t  th_up;
	pthread_t  th_down;
	struct sigaction sigact;
	int i;

	/*initialize the array of connected socket*/
	for(i=0;i<CONNFD_NUM_MAX;i++){
		connfds[i]=-1;
	}
	/*configure the signal handling*/
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags=SA_NOMASK;
	sigact.sa_handler=signal_handle;
	sigaction(SIGINT,&sigact,NULL);
	sigaction(SIGTERM,&sigact,NULL);

	/*create the linked list to store json string*/
	list_init(&ns_list);

	/*create threads*/
	if(pthread_create(&th_up,NULL,(void*(*)(void*))thread_up,NULL)!=0){
		MSG("ERROR: [main] impossible to create thread for upstream communicating from network server\n");
		exit(EXIT_FAILURE);
	}
	if(pthread_create(&th_down,NULL,(void*(*)(void*))thread_down,NULL)!=0){
			MSG("ERROR: [main] impossible to create thread for downstream communicating to network server\n");
			exit(EXIT_FAILURE);
	}

	while(!exit_sig){

	}

	if(exit_sig==true){
		/*shutdown all socket to alarm the threads from system call(like recv()) */
		shutdown(listenfd_up,SHUT_RDWR);
		/*shutdown all connected sockfd*/
		for(i=0;i<CONNFD_NUM_MAX;i++){
			if(connfds[i]==-1){
				break;
			}
			shutdown(connfds[i],SHUT_RDWR);
		}
		shutdown(sockfd_down,SHUT_RDWR);
		/*free linked list*/
		list_destroy(&ns_list,destroy_msg);
	}

	pthread_join(th_up,NULL);
	pthread_join(th_down,NULL);
	MSG("INFO: the main program on application server exit successfully\n");
	exit(EXIT_SUCCESS);
}

void signal_handle(int signo){
	if(signo==SIGINT || signo==SIGTERM){
		exit_sig=true;
		MSG("######################waiting for exiting#######################\n");
	}
	return;
}

void thread_up(){
	struct sockaddr_in cliaddr;
	socklen_t clilen;
	int j;
	pthread_t th_up_handle;
	/*try to open and bind TCP listening socket*/
	tcp_bind(appserv_addr,port_up,&listenfd_up);
	listen(listenfd_up,LISTENQ);
	while(!exit_sig){
		if((connfd_up=accept(listenfd_up,(struct sockaddr*)&cliaddr,&clilen))<0){
			if(exit_sig!=true){
				MSG("ERROR: [up] thread_up failed to accept:%s",strerror(errno));
			}
			else{
				MSG("INFO: listening socketfd is shut down\n");
			}
			break;
		}
		pthread_mutex_lock(&mx_conn);
		conn_num++;
		pthread_mutex_unlock(&mx_conn);
		struct arg arg;
		arg.connfd=connfd_up;
		connfds[j]=connfd_up;
		/*create threads*/
		if(pthread_create(&th_up_handle,NULL,(void*)thread_up_handle,(void*)&arg)!=0){
			MSG("ERROR: [up] impossible to create thread for receiving and handling upstream message\n");
			exit(EXIT_FAILURE);
		}
		j++;
		if(conn_num==CONNFD_NUM_MAX){
			break;
		}
	}
	close(listenfd_up);
	MSG("INFO: thread_up exit successfully\n");
}

/*connect the network server and send message*/
void thread_down(){
	struct pkt pkt_send;
	/*try to open and connect socket for network server*/
	reconnect:tcp_connect(netserv_addr,port_down,&sockfd_down);
	while(!exit_sig){
		/*read the content of linked list and send*/
		pthread_mutex_lock(&mx_ns_list);
		while(list_is_empty(&ns_list)==false){
			struct msg* temp=(struct msg*)((ns_list.head)->data);
			strcpy(pkt_send.json_content,temp->json_string);
			if(send(sockfd_down,&pkt_send,sizeof(pkt_send),0)<0){
				MSG("WARNING: [down] failed to send data to network server\n");
				MSG("INFO: trying to reconnect to the network server\n");
				pthread_mutex_unlock(&mx_ns_list);
				goto reconnect;
			}
			else{
				MSG("INFO: [down]send join_accept message successfully\n");
				list_delete_at_head(&ns_list,destroy_msg);
			}
		}
		pthread_mutex_unlock(&mx_ns_list);
		/*set timer for preparing data in thread_up_handle */
		set_timer(SEND_INTERVAL_S,SEND_INTERVAL_MS);
	}
	MSG("INFO: thread_down exit successfully\n");
}
void thread_up_handle(void* arg){

	pthread_detach(pthread_self());
	struct arg* conn=arg;
	//char pkt_recv[JSON_MAX];
	int msg_len;
	int index=0;
	struct res_handle result;
	struct msg data;
	struct pkt pkt_recv;

	while(!exit_sig){
		if((msg_len=recv(conn->connfd,&pkt_recv,sizeof(pkt_recv),0))<0){
			MSG("WARNING: [up] thread_up_handle recv returned %s\n", strerror(errno));
			break;
		}
		if(msg_len==0){
			if(exit_sig==true){
				/*socket is shut down*/
				MSG("INFO: the connected socket is shut down\n ");
			}
			else{
				pthread_mutex_lock(&mx_conn);
				conn_num--;
				pthread_mutex_unlock(&mx_conn);
				MSG("WARNING: [up] the client is close,the corresponding connected socket is %d \n",conn->connfd);
			}
			break;
		}
//		MSG(pkt_recv);
//		MSG("\n");
		/*msg_handle() will do some operations on the data in the database*/
		pthread_mutex_lock(&mx_db);
		result=msg_handle(pkt_recv.json_content,index);
		pthread_mutex_unlock(&mx_db);
		if(result.signal==1){
			data.json_string=malloc(strlen(result.json_string)+1);
			strcpy(data.json_string,result.json_string);
			pthread_mutex_lock(&mx_ns_list);
			list_insert_at_tail(&ns_list,&data,sizeof(struct msg),assign_msg);
			pthread_mutex_unlock(&mx_ns_list);
		}
		index++;
		bzero(&pkt_recv,sizeof(pkt_recv));
	}
	close(conn->connfd);
}
/*get the tcp socket fd and bind*/
void  tcp_bind(const char* servaddr,const char* port,int* listenfd){
	struct addrinfo hints;
	struct addrinfo *results,*r;
	int i;
	char host_name[64];
	char service_name[64];
	int listen;

	/*try to open and bind TCP listening socket*/
	bzero(&hints,sizeof(hints));
	hints.ai_socktype=SOCK_STREAM;
	hints.ai_family=AF_INET;
	if(getaddrinfo(servaddr,port,&hints,&results)!=0){
		MSG("ERROR: [down] getaddrinfo on address %s (PORT %s) returned %s\n", servaddr,port, gai_strerror(i));
		exit(EXIT_FAILURE);
	}
	r=results;
	do{
		listen=socket(r->ai_family,r->ai_socktype,r->ai_protocol);
		if(listen<0)
			continue;
		if(bind(listen,r->ai_addr,r->ai_addrlen)==0)
			break;			/*success*/
		close(listen);		/*bind error,close and try next one*/
	}while((r=r->ai_next)!=NULL);
	if(r==NULL){
		MSG("ERROR: [down] failed to open listening socket to any of server %s addresses (port %s)\n", servaddr, port);
		i=1;
		for(r=results;r!=NULL;r=r->ai_next){
			getnameinfo(r->ai_addr,r->ai_addrlen,host_name,sizeof(host_name),service_name,sizeof(service_name), NI_NUMERICHOST);
			MSG("INFO: [down] result %i host:%s service:%s\n", i, host_name, service_name);
			i++;
		}
		exit(EXIT_FAILURE);
	}
	*listenfd=listen;
	freeaddrinfo(results);
}
/*get the socket fd and connect to the server*/
void tcp_connect(const char* servaddr,const char* port,int* sockfd){
	/*some  variables for building TCP sockets*/
	struct addrinfo hints;
	struct addrinfo *results;
	struct addrinfo *r;
	int i;
	char host_name[64];
	char service_name[64];
	int sock;
	const int on=1;

	/*try to open and connect socket */
	while(!exit_sig){
		bzero(&hints,sizeof(hints));
		hints.ai_family=AF_INET;
		hints.ai_socktype=SOCK_STREAM;
		if(getaddrinfo(servaddr,port,&hints,&results)!=0){
			MSG("ERROR: [up] getaddrinfo on address %s (PORT %s) returned %s\n", servaddr, port, gai_strerror(i));
			exit(EXIT_FAILURE);
		}
		for(r=results;r!=NULL;r=r->ai_next){
			sock=socket(r->ai_family,r->ai_socktype,r->ai_protocol);
			if(sock<0)
				continue;
			setsockopt(sock,IPPROTO_TCP,TCP_NODELAY,&on,sizeof(on));
			if(connect(sock,r->ai_addr,r->ai_addrlen)==0)
				break;
			close(sock);/*connect error,close and try again*/
		}
		if(r==NULL){
			MSG("ERROR: [up] failed to open socket to any of server %s addresses (port %s)\n", servaddr, port);
			i=1;
			for(r=results;r!=NULL;r=r->ai_next){
				getnameinfo(r->ai_addr,r->ai_addrlen,host_name,sizeof(host_name),service_name,sizeof(service_name), NI_NUMERICHOST);
				MSG("INFO: [up] result %i host:%s service:%s\n", i, host_name, service_name);
				i++;
			}
		}
		else{
			/*the connection to the the server is established, break*/
			*sockfd=sock;
			MSG("INFO: connect to the server %s addresses (port %s) successfully\n",servaddr,port);
			freeaddrinfo(results);
			break;
		}
		freeaddrinfo(results);
		set_timer(SOCKET_INTERVAL_S,SOCKET_INTERVAL_MS);
	}
}
/*timer function*/
void set_timer(int sec,int msec){
	struct timeval timeout;
	timeout.tv_sec=sec;
	timeout.tv_usec=msec;
	select(0,NULL,NULL,NULL,&timeout);
}

