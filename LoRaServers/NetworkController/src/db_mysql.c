/*
 * db_mysql.c
 *
 *  Created on: Mar 7, 2016
 *      Author: paullu
 */

#include <string.h>
#include "db_mysql.h"


uint8_t* result;
char sql[128];
//char array to hex array
static void str_to_hex(uint8_t* dest,char* src,int len);

/*try to connect the database*/
int connect_db(const char* host,const char* user,const char* passwd,const char* db,MYSQL* mysql){
	mysql_init(mysql);
	if(mysql_real_connect(mysql,host,user,passwd,db,0,NULL,0)){
		return SUCCESS;
	}
	else
		return FAILED;
}

/*try to query one field from one table by devaddr filed
 *there is only one record in query results
 */
int query_db_by_addr(const char* column_name, const char* table_name,unsigned int devaddr,MYSQL* mysql,uint8_t* data,int size){
	MYSQL_RES *res;
	MYSQL_ROW row;
	int flag;

	bzero(sql,sizeof(sql));
	snprintf(sql,sizeof(sql),"SELECT HEX(%s) AS %s FROM %s WHERE devaddr=%u",column_name,column_name,table_name,devaddr);
	flag=mysql_real_query(mysql,sql,strlen(sql));
	if(flag){
		return FAILED;
	}
	res=mysql_store_result(mysql);
	row=mysql_fetch_row(res);
	if(row==NULL){
		mysql_free_result(res);
		return FAILED;
	}
	if(strlen(row[0])!=size*2){
		mysql_free_result(res);
		return FAILED;
	}
	//char array to hex array
	str_to_hex(data,row[0],size);
	mysql_free_result(res);
	return SUCCESS;
}

int query_db_by_addr_str(const char* column_name, const char* table_name,unsigned int devaddr,MYSQL* mysql,char* data){
	MYSQL_RES *res;
	MYSQL_ROW row;
	int flag;

	bzero(sql,sizeof(sql));
	snprintf(sql,sizeof(sql),"SELECT HEX(%s) AS %s FROM %s WHERE devaddr=%u",column_name,column_name,table_name,devaddr);
	flag=mysql_real_query(mysql,sql,strlen(sql));
	if(flag){
		return FAILED;
	}
	res=mysql_store_result(mysql);
	row=mysql_fetch_row(res);
	if(row==NULL){
		mysql_free_result(res);
		return FAILED;
	}
	strcpy(data,row[0]);
	mysql_free_result(res);
	return SUCCESS;
}

int query_db_by_addr_uint(const char* column_name, const char* table_name,unsigned int devaddr,MYSQL* mysql,unsigned int* data){
	MYSQL_RES *res;
	MYSQL_ROW row;
	int flag;

	bzero(sql,sizeof(sql));
	snprintf(sql,sizeof(sql),"SELECT %s FROM %s WHERE devaddr=%u",column_name,table_name,devaddr);
	flag=mysql_real_query(mysql,sql,strlen(sql));
	if(flag){
		return FAILED;
	}
	res=mysql_store_result(mysql);
	row=mysql_fetch_row(res);
	if(row==NULL){
		mysql_free_result(res);
		return FAILED;
	}
	*data=(unsigned int)atoi(row[0]);
	mysql_free_result(res);
	return SUCCESS;
}

/*try to query one field from one table by devaddr filed
 *there is only one record in query results
 */
int query_db_by_deveui(const char* column_name, const char* table_name,const char* deveui,MYSQL* mysql,uint8_t* data,int size){
	MYSQL_RES *res;
	MYSQL_ROW row;
	int flag;

	bzero(sql,sizeof(sql));
	snprintf(sql,sizeof(sql),"SELECT HEX(%s) AS %s FROM %s WHERE deveui=x'%s'",column_name,column_name,table_name,deveui);
	flag=mysql_real_query(mysql,sql,strlen(sql));
	if(flag){
		return FAILED;
	}
	res=mysql_store_result(mysql);
	row=mysql_fetch_row(res);
	if(row==NULL){
		return FAILED;
	}
	if(strlen(row[0])!=size*2){
		return FAILED;
	}
	//char array to hex array
	str_to_hex(data,row[0],size);
	mysql_free_result(res);
	return SUCCESS;
}

int query_db_by_deveui_str(const char* column_name, const char* table_name,const char* deveui,MYSQL* mysql,char* data){
	MYSQL_RES *res;
	MYSQL_ROW row;
	int flag;

	bzero(sql,sizeof(sql));
	snprintf(sql,sizeof(sql),"SELECT %s FROM %s WHERE deveui=x'%s'",column_name,table_name,deveui);
	flag=mysql_real_query(mysql,sql,strlen(sql));
	if(flag){
		return FAILED;
	}
	res=mysql_store_result(mysql);
	row=mysql_fetch_row(res);
	if(row==NULL){
		mysql_free_result(res);
		return FAILED;
	}
	strcpy(data,row[0]);
	mysql_free_result(res);
	return SUCCESS;
}

int query_db_by_deveui_uint(const char* column_name, const char* table_name,const char* deveui,MYSQL* mysql,unsigned int* data){
	MYSQL_RES *res;
	MYSQL_ROW row;
	int flag;

	bzero(sql,sizeof(sql));
	snprintf(sql,sizeof(sql),"SELECT %s FROM %s WHERE deveui=x'%s'",column_name,table_name,deveui);
	flag=mysql_real_query(mysql,sql,strlen(sql));
	if(flag){
		return FAILED;
	}
	res=mysql_store_result(mysql);
	row=mysql_fetch_row(res);
	if(row==NULL){
		mysql_free_result(res);
		return FAILED;
	}
	*data=(unsigned int)atoi(row[0]);
	mysql_free_result(res);
	return SUCCESS;
}

int update_db_by_deveui(const char* column_name,const char* table_name,const char* deveui,MYSQL* mysql,const char* data,int seed){
	int flag;
	bzero(sql,sizeof(sql));
	if(seed==0){
		snprintf(sql,sizeof(sql),"UPDATE %s SET %s=x'%s' WHERE deveui=x'%s'",table_name,column_name,data,deveui);
	}
	else{
		snprintf(sql,sizeof(sql),"UPDATE %s SET %s=\"%s\" WHERE deveui=x'%s'",table_name,column_name,data,deveui);
	}
	flag=mysql_real_query(mysql,sql,strlen(sql));
	if(flag){
		return FAILED;
	}
	else{
		return SUCCESS;
	}
}
int update_db_by_deveui_uint(const char* column_name,const char* table_name,const char* deveui,MYSQL* mysql,unsigned int data){
	int flag;
	bzero(sql,sizeof(sql));
	snprintf(sql,sizeof(sql),"UPDATE %s SET %s=%u WHERE deveui=x'%s'",table_name,column_name,data,deveui);
	flag=mysql_real_query(mysql,sql,strlen(sql));
	if(flag){
		return FAILED;
	}
	else{
		return SUCCESS;
	}
}

int update_db_by_addr_uint(const char* column_name,const char* table_name,unsigned int devaddr,MYSQL* mysql,unsigned int data){
	int flag;
	bzero(sql,sizeof(sql));
	snprintf(sql,sizeof(sql),"UPDATE %s SET %s=%u WHERE devaddr=%u",table_name,column_name,data,devaddr);
	flag=mysql_real_query(mysql,sql,strlen(sql));
	if(flag){
		return FAILED;
	}
	else{
		return SUCCESS;
	}
}

/*close the connectiont*/
void close_db(MYSQL* mysql){
	mysql_close(mysql);
	mysql_library_end();
}
void str_to_hex(uint8_t* dest,char* src,int len){
	int i;
	char ch1;
	char ch2;
	uint8_t ui1;
	uint8_t ui2;
	i=0;
	for(i=0;i<len;i++){
		ch1=src[i*2];
		ch2=src[i*2+1];
		ui1=toupper(ch1)-0x30;
		if(ui1>9)
			ui1-=7;
		ui2=toupper(ch2)-0x30;
		if(ui2>9)
			ui2-=7;
		dest[i]=ui1*16+ui2;
	}
}



