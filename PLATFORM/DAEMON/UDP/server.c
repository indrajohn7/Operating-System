/************* UDP SERVER CODE *******************/
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>


#define PORT 8000

int main()
{
	int udpSocket, nBytes;
	char buffer[1024];
	struct sockaddr_in serverAddr, clientAddr;
	struct sockaddr_storage serverStorage;
	socklen_t addr_size, client_addr_size;
	int i;
	/*Create UDP socket*/
	udpSocket = socket(PF_INET, SOCK_DGRAM, 0);
	/*Configure settings in address struct*/
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(PORT);
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY); //  inet_addr("127.0.0.1");
	memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);
	/*Bind socket with address struct*/
	bind(udpSocket, (struct sockaddr *) &serverAddr, sizeof(serverAddr));
	/*Initialize size variable to be used later on*/
	addr_size = sizeof serverStorage;
	
	while(1){
		/* Try to receive any incoming UDP datagram. Address and port of
		requesting client will be stored on serverStorage variable */
		printf("Connecting to client:\n");
		nBytes = recvfrom(udpSocket,buffer,1024,0,(struct sockaddr *)&serverStorage, &addr_size);
		printf("Recieved packet from client:%s\n:",buffer);
		/*Convert message received to uppercase*/
		for(i=0;i<nBytes-1;i++)
			buffer[i] = toupper(buffer[i]);
			/*Send uppercase message back to client, using serverStorage as the address*/
			sendto(udpSocket,buffer,nBytes,0,(struct sockaddr *)&serverStorage,addr_size);
		sleep(3);
	}

	while(1){
		char* stream=malloc(100);
		printf("Write something:");
		gets(stream);
		for(i=0;i<strlen(stream);i++)
			toupper(stream[i]);
		puts(stream);
	}

	
	return 0;
}
