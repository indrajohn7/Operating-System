/**********************    tcpserver.c   *************/
/* header files needed to use the sockets API */
/* File contain Macro, Data Type and Structure */
/***********************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include<netinet/in.h>//INADDR_ANY
#include <syslog.h>
#include <sys/stat.h>
#include<sys/time.h>
#include<fcntl.h>
#include<errno.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#define MAXSZ 100
#define BufferLength 100
#define SERVER_IP "127.0.0.1"

/*Server Port*/

#define SERVPORT 8000
#define CLIENTPORT 7891

void call_server_thread();
void call_client_thread();
pthread_t t1;
pthread_t t2;

int semaphore=0;

char* stream[6]={"socket",
				"tcp/ip",
				"UDP",
				"protocol",
				"layer",
				"architecture"
			};

int main()
{
	pthread_attr_t attr;
 	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr,1024*1024);
 
	int status;
	printf("Creating Threads1::\n");
 	status=pthread_create(&t1,&attr,(void*)&call_server_thread,NULL);
 	if(status!=0){
		 printf("Failed to create Thread1 with Status:%d\n",status);
 	}
 
	
 	printf("Creating Threads2::\n");
 	status=pthread_create(&t2,&attr,(void*)&call_client_thread,NULL);
 	if(status!=0){
		printf("Failed to create Thread2 with Status:%d\n",status);
 	}
 	

 	pthread_join(t1,NULL);
 	pthread_join(t2,NULL);
		
	return 0;

}


void call_client_thread()
{
    int shmid;
    key_t key;
    char *shm, *s;
	char* buffer=(char*)malloc(1024);
    /*
     * We need to get the segment named
     * "5678", created by the server.
     */
    key = 5678;

    /*
     * Locate the segment.
     */
    while(1){
	if ((shmid = shmget(key, MAXSZ, IPC_CREAT | 0666)) < 0) {
        perror("shmget");
        printf("shmget ID: %d\n",shmid);
//		exit(1);
    }

    /*
     * Now we attach the segment to our data space.
     */
    if ((shm = shmat(shmid, NULL, 0)) == (char *) -1) {
        perror("shmat");
 //       exit(1);
    }

    /*
     * Now read what the server put in the memory.
     */
		int stream_size=sizeof(stream)/sizeof(char*);
		int t=rand()%stream_size;


		buffer=shm;
		buffer=(char*)malloc(1024);	
		memset(buffer,'\0',1024);
		memcpy(buffer,stream[t],strlen(stream[t]));
		printf("You typed: %s",buffer);

	/*
	int i=0;
	s=shm;
    for (i = 0; i < strlen(shm); i++)
        putchar(shm[i]);
    putchar('\n');
	*/
    /*
     * Finally, change the first character of the 
     * segment to '*', indicating we have read 
     * the segment.
     */
	sleep(5);
	}
    exit(0);
}

#if 0

void call_client_thread()
{
	int clientSocket, portNum, nBytes;
	char buffer[1024];
	struct sockaddr_in serverAddr;
	socklen_t addr_size;
	/*Create UDP socket*/
	clientSocket = socket(AF_INET, SOCK_STREAM, 0);
	/*Configure settings in address struct*/
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(CLIENTPORT);
	serverAddr.sin_addr.s_addr = inet_addr("127.0.0.2");
	memset(serverAddr.sin_zero, '\0', sizeof(serverAddr.sin_zero));
	/*Initialize size variable to be used later on*/
	addr_size = sizeof(serverAddr);
    

 	int n,res;
 	char msg1[MAXSZ];
 	char msg2[MAXSZ];



/*     if( (res=daemonize("mydaemon","~/CODE/PLATFORM/",NULL,NULL,NULL)) != 0 ) {
        fprintf(stderr,"error: daemonize failed\n");
        exit(EXIT_FAILURE);
    }
*/
	connect(clientSocket,(struct sockaddr *)&serverAddr,sizeof(serverAddr));
	
	while(1){
		printf("Type a sentence to send to Daemon server from Main Client:\n");
		//fgets(buffer,1024,stdin);
		if(semaphore){
		int stream_size=sizeof(stream)/sizeof(char*);
		int t=rand()%stream_size;
		memset(buffer,'\0',1024);
		memcpy(buffer,stream[t],strlen(stream[t]));
		printf("You typed: %s",buffer);
		nBytes = strlen(buffer) + 1;
		/*Send message to server*/
	//	sendto(clientSocket,buffer,nBytes,0,(struct sockaddr *)&serverAddr,addr_size);
		
		send(clientSocket,buffer,nBytes,0);
		/*Receive message from server*/
		nBytes = recv(clientSocket,buffer,MAXSZ,0);
	//	nBytes = recvfrom(clientSocket,buffer,1024,0,NULL, NULL);
		printf("Received from Daemon server: %s\n",buffer);
		}
		sleep(3);
	}
	
}

#endif

void call_server_thread()
{
        /*Variable Structure Definition*/
        int sd, sd2, rc,length;
        length = sizeof(int);
        int totalcnt = 0, on = 1;
        char temp;
        char buffer[BufferLength];
        struct sockaddr_in serveraddr;
        struct sockaddr_in their_addr;
        fd_set read_fd;
        struct timeval timeout;
        timeout.tv_sec = 15;
        timeout.tv_usec = 0;

        /* The socket() function returns a socket descriptor */
        /* representing an endpoint. The statement also */
        /* identifies that the INET (Internet Protocol) */
        /* address family with the TCP transport (SOCK_STREAM) */
        /* will be used for this socket. */
        /************************************************/
        /* Get a socket descriptor */

        if((sd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
                perror("Serversocket()error");
                /* exit */
                exit(1);
        }else
                printf("Serversocket() is OK \n");

        /* The setsockopt() function is used to allow */
        /* the local address to be reused when the server */
        /* is restarted before the required wait time */
        /* expires */
        /***********************************************/
        /* Allow socket descriptor to be reusable */

        if((rc = setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (char*)&on,sizeof(on))) < 0)
        {
                perror("Serversetsockopt()error");
                close(sd);
                exit(1);
        }else
                printf("Serversetsockopt()is OK\n");


        /* bind to an address */
        memset(&serveraddr, 0x00, sizeof(struct sockaddr_in));
        serveraddr.sin_family = AF_INET;
        serveraddr.sin_port = htons(SERVPORT);
        serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
        //printf("Using %s, listening at %d\n",inet_ntoa(serveraddr.sin_addr), SERVPORT);

        /*After the socket descriptor is created, a bind() */
        /* function gets a unique name for the socket. */
        /* In this case , the user sets the */
        /* s_addr to 0, which allows the system to */
        /* connect to any client that used port 8000. */

        if((rc = bind(sd, (struct sockaddr*)&serveraddr, sizeof(serveraddr))) < 0)
        {
                perror("Serverbind() error");
                /* close the socket descriptor */
                close(sd);
                exit(1);
        }else
                printf("Serverbind()is OK\n");

	/* The listen() function allows the server to accept */
	/* incoming client connections. In this example, */
	/* the backlog is set to 10. This means that the */
	/* system can queue up to 10 connection requests before */
	/* the system starts rejecting incoming requests.*/
	/*************************************************/
	/* Up to 10 clients can be queued */
	
	if((rc = listen(sd, 10)) < 0)
	{
		perror("Serverlisten() error");
		close(sd);
		exit(1);	
	}else
		printf("ServerReadyfor client connection...\n");
	
	/* The server will accept a connection request */
	/* The server will accept a connection request */
	/* connection request does the following: */
	/* Is part of the same address family */
	/* Uses streams sockets (TCP) */
	/* Attempts to connect to the specified port */
	/***********************************************/
	/* accept() the incoming connection request. */
	
	int sin_size = sizeof(struct sockaddr_in);
	if((sd2 = accept(sd, (struct sockaddr *)&their_addr, &sin_size)) < 0)
	{
		perror("Serveraccept() error");
		close(sd);
		exit (1);
	}else
		printf("Serveraccept()is OK\n");
		
	/*client IP*/
	printf("Server new socket, sd2 is OK...\n");
	//printf("Got connection from the client: %s\n",inet_ntoa(their_addr.sin_addr));
	
	/* The select() function allows the process to */
	/* the process when the event occurs. In this */
	/* example, the system notifies the process */
	/* only when data is available to read. */
	/***********************************************/
	/* Wait for up to 15 seconds on */
	/* select() for data to be read. */
	while(1){
		FD_ZERO(&read_fd);
		FD_SET(sd2, &read_fd);
		rc = select(sd2+1, &read_fd, NULL, NULL, NULL /*&timeout*/);
		if((rc == 1) && (FD_ISSET(sd2, &read_fd)))
		{
			/* Read data from the client. */
			totalcnt = 0;
	//		while(totalcnt < BufferLength)
	//		{
				/* When select() indicates that there is data */
				/* available, use the read() function to read */
				/* 100 bytes of the string that the */
				/* client sent. */
				/***********************************************/
				/* read() from client */
				rc = read(sd2, buffer, BufferLength);
				if(rc < 0){
					perror("Serverread() error");
					close(sd);
					close(sd2);
					exit (1);			
				}else if(rc == 0){
					printf("Client program has issued a close()\n");
					close(sd);
					close(sd2);
					exit(1); //dont exit():: break from here	
				}else{
					semaphore=1;
					printf("Data recieved from Client:%s\n",buffer);
					totalcnt += rc;
					printf("Serverread() is OK\n");
				}
	//		}	
		}
	
	
		/* Shows the data */
		printf("Received data from the client: %s\n", buffer);	
	
		/* Echo some bytes of string, back */
		/* to the client by using the write() */
		/* function. */
		/*************************************/
		/* write() some bytes of string, */
		/* back to the client. */
		
		printf("Server Echoing back to client...\n");
		rc = write(sd2, buffer, totalcnt);
		if(rc != totalcnt){
			perror("Server write() error");
			/* Get the error number. */
			rc = getsockopt(sd2, SOL_SOCKET, SO_ERROR, &temp, &length);
			if(rc == 0){
				/* Print out the asynchronously */
				/* received error. */
				errno = temp;
				perror("SO_ERROR was: ");
			}		
		}else{
			printf("Recived data to Server:%s\n",buffer);
		}
		
	
	}
		/* When the data has been sent, close() */
		/* the socket descriptor that was returned */
		/* from the accept() verb and close() the */
		/* original socket descriptor. */
		/*****************************************/
		/* Close the connection to the client and */
		/* close the server listening socket. */
		/******************************************/

	
	close(sd2);
	close(sd);
	exit(0);
}

