APP_NAME:=ApplicationServer
OBJECTS:=obj/$(APP_NAME).o obj/base64.o obj/parson.o obj/aes.o obj/cmac.o obj/utilities.o obj/LoRaMacCrypto.o obj/generic_list.o obj/db_mysql.o obj/ashandle.o 
CFLAGS_1 := -Iinc
CFLAGS_2 :=-Iinc -I/usr/include/mysql
LIBS := -L/usr/include/mysql -lmysqlclient -lpthread

all: $(APP_NAME)

clean:
	rm -f obj/*.o
	rm -f $(APP_NAME)

obj/base64.o: src/base64.c inc/base64.h
	gcc -c $(CFLAGS_1)  $< -o $@ 

obj/parson.o: src/parson.c inc/parson.h
	gcc -c $(CFLAGS_1) $< -o $@

obj/aes.o: src/aes.c inc/aes.h
	gcc -c $(CFLAGS_1) $< -o $@

obj/cmac.o: src/cmac.c inc/cmac.h
	gcc -c $(CFLAGS_1) $< -o $@

obj/utilities.o: src/utilities.c inc/utilities.h
	gcc -c $(CFLAGS_1) $< -o $@

obj/LoRaMacCrypto.o: src/LoRaMacCrypto.c inc/LoRaMacCrypto.h inc/aes.h inc/cmac.h inc/utilities.h
	gcc -c $(CFLAGS_1) $< -o $@

obj/generic_list.o: src/generic_list.c inc/generic_list.h
	gcc -c $(CFLAGS_1) $< -o $@

obj/db_mysql.o: src/db_mysql.c inc/db_mysql.h
	gcc -c $(CFLAGS_2) $< -o $@ 

obj/ashandle.o: src/ashandle.c inc/ashandle.h inc/LoRaMacCrypto.h inc/db_mysql.h inc/parson.h inc/base64.h
	gcc -c $(CFLAGS_2) $< -o $@ 

obj/$(APP_NAME).o: src/main.c inc/ashandle.h  inc/base64.h inc/parson.h 
	gcc -c $(CFLAGS_2) $< -o $@ 

$(APP_NAME): $(OBJECTS)
	gcc  $(OBJECTS) -o $@ $(LIBS)

