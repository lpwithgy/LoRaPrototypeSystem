/*
 * db_mysql.h
 *
 *  Created on: Mar 7, 2016
 *      Author: paullu
 */

#ifndef DB_MYSQL_H_
#define DB_MYSQL_H_



#endif /* DB_MYSQL_H_ */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "mysql.h"

#define FAILED     0
#define SUCCESS    1

int connect_db(const char* host,const char* user,const char* passwd,const char* db,MYSQL* mysql);
int query_db_by_addr(const char* column_name, const char* table_name,unsigned int devaddr,MYSQL* mysql,uint8_t* data,int size);
int query_db_by_addr_str(const char* column_name, const char* table_name,unsigned int devaddr,MYSQL* mysql,char* data);
int query_db_by_addr_uint(const char* column_name, const char* table_name,unsigned int devaddr,MYSQL* mysql,unsigned int* data);
int query_db_by_deveui(const char* column_name,const char* table_name,const char* deveui,MYSQL* mysql,uint8_t* data,int size);
int query_db_by_deveui_str(const char* column_name, const char* table_name,const char* deveui,MYSQL* mysql,char* data);
int query_db_by_deveui_uint(const char* column_name, const char* table_name,const char* deveui,MYSQL* mysql,unsigned int* data);
int update_db_by_deveui(const char* column_name,const char* table_name,const char* deveui,MYSQL* mysql,const char* data,int seed);
int update_db_by_deveui_uint(const char* column_name,const char* table_name,const char* deveui,MYSQL* mysql,unsigned int data);
int update_db_by_addr_uint(const char* column_name,const char* table_name,unsigned int devaddr,MYSQL* mysql,unsigned int data);
void close_db(MYSQL* mysql);
