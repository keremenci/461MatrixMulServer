client: client.c server
	cc -lrt client.c -o client -g

server: server.c
	cc -pthread -lrt server.c -o server -g 
