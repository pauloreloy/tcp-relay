#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>
#include <strings.h>

#define MAX_SERVERS 10
#define BUFFER_SZ 4096


typedef struct {
    struct sockaddr_in addr; /* Server remote address */
    int connfd;              /* Connection file descriptor */
    int connected;
} server_t, client_t;

server_t *servers[MAX_SERVERS];
pthread_mutex_t servers_mutex = PTHREAD_MUTEX_INITIALIZER;

void server_add(char ip[100], int port){
    pthread_mutex_lock(&servers_mutex);
    for (int i = 0; i < MAX_SERVERS; ++i) {
        if (!servers[i]) {
			servers[i] = (server_t *)malloc(sizeof(server_t));
			servers[i]->addr.sin_family = AF_INET;
			servers[i]->addr.sin_addr.s_addr = inet_addr(ip);
			servers[i]->addr.sin_port = htons(port);
            servers[i]->connfd = socket(AF_INET, SOCK_STREAM, 0);
            if (servers[i]->connfd == -1) {
                printf("Socket creation failed.\n");
                exit(0);
            }
            break;
        }
		else {
			if(strcmp(ip, inet_ntoa(servers[i]->addr.sin_addr))) {
				continue;
			}
		}
    }
    pthread_mutex_unlock(&servers_mutex);
}

void send_message_all(char *s, int connfd){
    char retbuf[BUFFER_SZ];
    int rlen;
    pthread_mutex_lock(&servers_mutex);
    for (int i = 0; i <MAX_SERVERS; ++i){
        if (servers[i]) {
            servers[i]->connfd = socket(AF_INET, SOCK_STREAM, 0);
            if (connect(servers[i]->connfd, (struct sockaddr *)&servers[i]->addr, sizeof(servers[i]->addr)) != 0) 
            {
                printf("Connection with the server %s failed...\n", inet_ntoa(servers[i]->addr.sin_addr));
                exit(0);
            }
            else 
            {
				printf("Connection with server %s established.\n", inet_ntoa(servers[i]->addr.sin_addr));
				servers[i]->connected = 1;
            }

            if (write(servers[i]->connfd, s, strlen(s) + 1) < 0) {
                perror("Write to descriptor failed");
                break;
            }
            
            while ((rlen = read(servers[i]->connfd, retbuf, sizeof(retbuf) - 1))) {
				if(rlen == 0) {
					write(connfd, '\0', 0);
				}
				if (!strlen(retbuf)) 
				{
					continue;
				}
                
                if(i == 0) {
                    write(connfd, retbuf, strlen(retbuf));
                }
                bzero(retbuf, sizeof(retbuf));
            }

            close(servers[i]->connfd);
            servers[i]->connected = 0;
        }
    }
    pthread_mutex_unlock(&servers_mutex);
}

void *handle_client(void *arg)
{
	client_t *cli = (client_t *)arg;
	printf("New client: %s\n", inet_ntoa(cli->addr.sin_addr));
	char buff_in[BUFFER_SZ];
	int rlen = 0;
	
	bzero(buff_in, sizeof(buff_in));
	while ((rlen = read(cli->connfd, buff_in, sizeof(buff_in) + 1)) > 0) {
        if (!strlen(buff_in)) 
        {
			continue;
        }
		send_message_all(buff_in, cli->connfd);
        bzero(buff_in, sizeof(buff_in));
	}
	close(cli->connfd);
	free(cli);
	pthread_detach(pthread_self());
	
	return NULL;
}

int main(int argc, char *argv[])
{

    int listenfd = 0, connfd = 0;
    struct sockaddr_in serv_addr;
    struct sockaddr_in cli_addr;
    pthread_t tid;
	int l_port = atoi(argv[1]);

    /* Socket settings */
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(l_port);
    signal(SIGPIPE, SIG_IGN);


    if (bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Socket binding failed");
        return EXIT_FAILURE;
    }

    if (listen(listenfd, 10) < 0) {
        perror("Socket listening failed");
        return EXIT_FAILURE;
    }

	/* Add destinations servers */
    server_add("10.0.0.149", l_port);

    while (1) {
        socklen_t clilen = sizeof(cli_addr);
        connfd = accept(listenfd, (struct sockaddr*)&cli_addr, &clilen);
		
		/* Client settings */
        client_t *cli = (client_t *)malloc(sizeof(client_t));
        cli->addr = cli_addr;
        cli->connfd = connfd;
		
		/* Create thread */
        pthread_create(&tid, NULL, &handle_client, (void*)cli);
		
        sleep(1);
    }
	
	return EXIT_SUCCESS;
}
