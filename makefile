all: server admin client_2 main

main: main.c server.o
	gcc -o main server.o main.c -lpthread

server: server.c
	gcc -c -o server.o server.c -lpthread

admin: admin.c 
	gcc -o admin admin.c -lpthread

client_2: client_2.c
	gcc -o client_2 client_2.c -lpthread
