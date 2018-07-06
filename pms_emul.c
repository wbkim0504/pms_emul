/*
 * Copyright 2018
 *
 * Author:		Wonbae Kim
 * Description: PMS emulator (modified from Dinux's Simple chatroom in C)
 * Version:		1.0
 *
 */

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

#define PMS_SERVER_PORT 5000

#define MAX_CLIENTS	100

typedef enum print_option_t {
	PRINT_NONE = 0,
	PRINT_MPU = 1,
	PRINT_SPU = 2,
	PRINT_INV = 3,
	PRINT_PCS = 4,
	PRINT_LIP = 5,
	PRINT_ESS = 6,
	PRINT_PRU = 7,
	PRINT_ALL = 8
} print_option_t;

int print_option = PRINT_NONE;
int print_spu_id = 0;


static unsigned int cli_count = 0;
static int uid = 10;

#define MSG_BUFF_SIZE (20480)

/* Client structure */
typedef struct {
	struct sockaddr_in addr;	/* Client remote address */
	int connfd;			/* Connection file descriptor */
	int uid;			/* Client unique identifier */
	char name[32];			/* Client name */
	unsigned char msg_buff[MSG_BUFF_SIZE];
	unsigned int pos_curr;
} client_t;

client_t *clients[MAX_CLIENTS];

/* Add client to queue */
void queue_add(client_t *cl){
	int i;
	for(i=0;i<MAX_CLIENTS;i++){
		if(!clients[i]){
			clients[i] = cl;
			return;
		}
	}
}

/* Delete client from queue */
void queue_delete(int uid){
	int i;
	for(i=0;i<MAX_CLIENTS;i++){
		if(clients[i]){
			if(clients[i]->uid == uid){
				clients[i] = NULL;
				return;
			}
		}
	}
}

/* Send message to all clients but the sender */
void send_message(char *s, int uid){
	int i;
	for(i=0;i<MAX_CLIENTS;i++){
		if(clients[i]){
			if(clients[i]->uid != uid){
				write(clients[i]->connfd, s, strlen(s));
			}
		}
	}
}

/* Send message to all clients */
void send_message_all(char *s){
	int i;
	for(i=0;i<MAX_CLIENTS;i++){
		if(clients[i]){
			write(clients[i]->connfd, s, strlen(s));
		}
	}
}

/* Send message to sender */
void send_message_self(const char *s, int connfd){
	write(connfd, s, strlen(s));
}

/* Send message to client */
void send_message_client(char *s, int uid){
	int i;
	for(i=0;i<MAX_CLIENTS;i++){
		if(clients[i]){
			if(clients[i]->uid == uid){
				write(clients[i]->connfd, s, strlen(s));
			}
		}
	}
}

/* Send list of active clients */
void send_active_clients(int connfd){
	int i;
	char s[64];
	for(i=0;i<MAX_CLIENTS;i++){
		if(clients[i]){
			sprintf(s, "<<CLIENT %d | %s\r\n", clients[i]->uid, clients[i]->name);
			send_message_self(s, connfd);
		}
	}
}

/* Strip CRLF */
void strip_newline(char *s){
	while(*s != '\0'){
		if(*s == '\r' || *s == '\n'){
			*s = '\0';
		}
		s++;
	}
}

/* Print ip address */
void print_client_addr(struct sockaddr_in addr){
	printf("%d.%d.%d.%d",
		addr.sin_addr.s_addr & 0xFF,
		(addr.sin_addr.s_addr & 0xFF00)>>8,
		(addr.sin_addr.s_addr & 0xFF0000)>>16,
		(addr.sin_addr.s_addr & 0xFF000000)>>24);
}

// ----------------------------------------------------------------------------
/* parsing command */
void parse_command(unsigned char *buff_in, int rlen)
{
	if (rlen < 4)
		return;

	if (rlen >= 5)
		print_spu_id = buff_in[3] - '0';
	else
		print_spu_id = 0;

	if (!strncmp(buff_in, "quit", 4))
	{
		printf("\'quit\' is received : program end !");
		exit(0);
	}
	else if (!strncmp(buff_in, "ALL", 3))
		print_option = PRINT_ALL;
	else if (!strncmp(buff_in, "MPU", 3))
		print_option = PRINT_MPU;
	else if (!strncmp(buff_in, "SPU", 3))
		print_option = PRINT_SPU;
	else if (!strncmp(buff_in, "INV", 3))
		print_option = PRINT_INV;
	else if (!strncmp(buff_in, "PCS", 3))
		print_option = PRINT_PCS;
	else if (!strncmp(buff_in, "LIP", 3))
		print_option = PRINT_LIP;
	else if (!strncmp(buff_in, "ESS", 3))
		print_option = PRINT_ESS;
	else if (!strncmp(buff_in, "PRU", 3))
		print_option = PRINT_PRU;
	else
		print_option = PRINT_NONE;
}

// ------------------------------------------------------------------
void print_client_status()
{
	int i;
	int	count = 0;

	printf("-------------------------\n");
	printf("current mpu list ... \n");
	printf("-------------------------\n");
	for(i=0; i<MAX_CLIENTS; i++)
	{
		if(clients[i])
		{
			count++;
			printf(" [%d] ", count);
			print_client_addr(clients[i]->addr);
			printf("\n");
		}
	}
	printf("-------------------------\n");
	printf("total %d MPUs are connected \n", count);
}


// ----------------------------------------------------------------------------
void print_hex(unsigned char *buff, int buff_size)
{
	for (int i=0; i<buff_size; i++)
	{
		printf("0x%02X ", buff[i]);

		if ((i+1)%10 == 0)
			printf("\n");
	}
	printf("\n");
}

// ----------------------------------------------------------------------------
void print_packet(unsigned char *buff, int buff_size)
{
	if (print_option == PRINT_NONE)
		return;

	if (print_option == PRINT_ALL)
	{
		printf("ALL ... \n");
		print_hex(buff, buff_size);
		print_option = PRINT_NONE;
		return;
	}

	char *pbuff = buff;
	int packet_header_size = 10;
	int spu_header_size = 9;
	int ecu_header_size = 1;

	/* mpu_header */
	char num_SPU = pbuff[8];
	char num_HIP = pbuff[9];

	if (print_option == PRINT_MPU)
	{
		printf("MPU (%d) ... \n", pbuff[6]);
		print_hex(pbuff, packet_header_size);
	}

	/* mpu_body */
	pbuff += packet_header_size;

	/* SPU */
	for (int i=0; i<num_SPU; i++)
	{
		/* SPU header */
		unsigned char spu_id = pbuff[0];
		int spu_size = (((int) pbuff[1]) << 8) + pbuff[2];

		char nINV = pbuff[4];
		char nPCS = pbuff[5];
		char nLIP = pbuff[6];
		char nESS = pbuff[7];
		char nPRU = pbuff[8];

		int num_ECU = nINV + nPCS + nLIP + nESS + nPRU;
		 
		if (print_option == PRINT_SPU)
		{
			printf("SPU (%d) ... \n", spu_id);
			print_hex(pbuff, spu_header_size);
		}

		/* SPU body */
		pbuff += spu_header_size;
		for (int k=0; k<num_ECU; k++)
		{
			unsigned char ecu_data_size = pbuff[0];
			unsigned char ecu_size = ecu_data_size + ecu_header_size;

			if ( i == print_spu_id)
			{
				if (print_option == PRINT_INV)
				{
					if (k == 0)
					{
						printf("INV (%d) ... \n", i);
						print_hex(pbuff, ecu_size);
					}
				}

				if (print_option == PRINT_PCS)
				{
					if (k == 1)
					{
						printf("PCS (%d) ... \n", i);
						print_hex(pbuff, ecu_size);
					}
				}

				if (print_option == PRINT_LIP)
				{
					if (k == 2)
					{
						printf("LIP (%d) ... \n", i);
						print_hex(pbuff, ecu_size);
					}
				}

				if (print_option == PRINT_ESS)
				{
					int m = k - 2;
					if ( (m > 0) && (m <= nESS) )
					{
						printf("ESS (%d,%d) ... \n", i,m);
						print_hex(pbuff, ecu_size);
					}
				}

				if (print_option == PRINT_PRU)
				{
					int m = k - 2 - nESS;
					if ( (m > 0) && (m <= nPRU) )
					{
						printf("PRU (%d,%d) ... \n", i,m);
						print_hex(pbuff, ecu_size);
					}
				}
			}

			pbuff += (ecu_size);
		}

	}

	/* HIP */
	// do nothing

	print_option = PRINT_NONE;
}


// ----------------------------------------------------------------------------
/* Handle all communication with the client */
#define	BUFF_SIZE	(2048)

void *handle_client(void *arg){
	char buff_out[1024];
	unsigned char buff_in[BUFF_SIZE];
	int rlen;
	int packet_header_size = 10;

	cli_count++;
	client_t *cli = (client_t *)arg;

	printf("<<ACCEPT ");
	print_client_addr(cli->addr);
	printf(" REFERENCED BY %d\n", cli->uid);

	sprintf(buff_out, "<<JOIN, HELLO %s\r\n", cli->name);
	send_message_all(buff_out);

	/* Receive input from client */
	while((rlen = read(cli->connfd, buff_in, sizeof(buff_in)-1)) > 0)
	{
		printf("-> received from ");
		print_client_addr(cli->addr);
		printf(" : %d byte data --> %c (0x%02X) \n", rlen, buff_in[0], buff_in[0]);

		if (rlen < packet_header_size)
		{
			parse_command(buff_in, rlen);
			continue;
		}

		if ((cli->pos_curr + rlen) < MSG_BUFF_SIZE)
		{
			unsigned char *pbuff = cli->msg_buff;
			pbuff += cli->pos_curr;
			memcpy(pbuff, buff_in, rlen);
			cli->pos_curr += rlen;
		}
		else
		{
			printf("packet size exceed MSG_BUFF_SIZE (%d) --> skip message \n", MSG_BUFF_SIZE);
			cli->pos_curr = 0;
		}

		if (cli->pos_curr > packet_header_size)
		{
			unsigned char *p = cli->msg_buff;
			unsigned int packet_size = (p[1] << 24) + (p[2] << 16) + (p[3] << 8) + p[4];
			printf("packet size = %d, msg received = %d\n", packet_size, cli->pos_curr);


			if (cli->pos_curr >= packet_size)
			{
				printf("print_packet \n");
				print_packet(cli->msg_buff, packet_size);
				cli->pos_curr = 0;
			}
		}
	}

	/* Close connection */
	close(cli->connfd);
	sprintf(buff_out, "<<LEAVE, BYE %s\r\n", cli->name);
	send_message_all(buff_out);

	/* Delete client from queue and yeild thread */
	queue_delete(cli->uid);
	printf("<<LEAVE ");
	print_client_addr(cli->addr);
	printf(" REFERENCED BY %d\n", cli->uid);
	free(cli);
	cli_count--;
	pthread_detach(pthread_self());
	
	return NULL;
}

// ------------------------------------------------------------------
int alarm_count = 0;

void fn_alarm(int sig_num)
{
	if (sig_num == SIGALRM)
	{
		if (alarm_count++ >= 60)
		{
			print_client_status();
			alarm_count = 0;
		}
	}
}

// ------------------------------------------------------------------
int main(int argc, char *argv[])
{
	int listenfd = 0, connfd = 0;
	struct sockaddr_in serv_addr;
	struct sockaddr_in cli_addr;
	pthread_t tid;

	/* Socket settings */
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(PMS_SERVER_PORT); 

	/* Ignore pipe signals */
	signal(SIGPIPE, SIG_IGN);
	
	/* Bind */
	if(bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0){
		perror("Socket binding failed");
		return 1;
	}

	/* Listen */
	if(listen(listenfd, 10) < 0){
		perror("Socket listening failed");
		return 1;
	}

	/* set alarm */
	signal(SIGALRM, fn_alarm);
	ualarm(1000000, 1000000);		// 1 sec timer

	printf("<[SERVER STARTED]>\n");

	/* Accept clients */
	while(1)
	{
		socklen_t clilen = sizeof(cli_addr);
		connfd = accept(listenfd, (struct sockaddr*)&cli_addr, &clilen);

		/* Check if max clients is reached */
		if((cli_count+1) == MAX_CLIENTS){
			printf("<<MAX CLIENTS REACHED\n");
			printf("<<REJECT ");
			print_client_addr(cli_addr);
			printf("\n");
			close(connfd);
			continue;
		}

		/* Client settings */
		client_t *cli = (client_t *)malloc(sizeof(client_t));
		cli->addr = cli_addr;
		cli->connfd = connfd;
		cli->uid = uid++;
		sprintf(cli->name, "%d", cli->uid);
		cli->pos_curr = 0;
		memset(cli->msg_buff, 0, MSG_BUFF_SIZE);

		/* Add client to the queue and fork thread */
		queue_add(cli);
		pthread_create(&tid, NULL, &handle_client, (void*)cli);

		/* Reduce CPU usage */
		sleep(1);
	}
}

