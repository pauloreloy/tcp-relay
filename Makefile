all:
	$(CC) -Wall server.c -O2 -std=c11 -lpthread -o server

debug:
	$(CC) -Wall -g server.c -O0 -std=c11 -lpthread -o server_dbg

clean:
	$(RM) -rf server server_dbg