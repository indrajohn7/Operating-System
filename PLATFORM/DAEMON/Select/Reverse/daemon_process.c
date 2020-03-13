/************* UDP CLIENT CODE *******************/

#include<stdio.h>
#include<sys/types.h>//socket
#include<sys/socket.h>//socket
#include<string.h>//memset
#include<stdlib.h>//sizeof
#include<netinet/in.h>//INADDR_ANY
#include <syslog.h>
#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>
#include<sys/time.h>
#include<fcntl.h>
#include<errno.h>
#include<string.h>
#include<math.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define PORT 8000
#define CLIENTPORT 8001
#define SERVER_IP "127.0.0.1"
#define MAXSZ 100
#define BufferLength 100

void call_server_thread();
void call_client_thread();

int semaphore=0;

char* stream[6]={	"Indra",
					"Natasha",
					"Soumya",
					"Tanu",
					"Kous",
					"njknk" 
				};
pthread_t t1;
pthread_t t2;


int
daemonize(char* name, char* path, char* outfile, char* errfile, char* infile )
{
    if(!path) { path="~/CODE/PLATFORM/"; }
    if(!name) { name="medaemon"; }
    if(!infile) { infile="/dev/null"; }
    if(!outfile) { outfile="/dev/null"; }
    if(!errfile) { errfile="/dev/null"; }
    //printf("%s %s %s %s\n",name,path,outfile,infile);
    pid_t child;
    //fork, detach from process group leader
    if( (child=fork())<0 ) { //failed fork
        fprintf(stderr,"error: failed fork\n");
        exit(EXIT_FAILURE);
    }
    if (child>0) { //parent
        exit(EXIT_SUCCESS);
    }
    if( setsid()<0 ) { //failed to become session leader
        fprintf(stderr,"error: failed setsid\n");
        exit(EXIT_FAILURE);
    }

    //catch/ignore signals
    signal(SIGCHLD,SIG_IGN);
    signal(SIGHUP,SIG_IGN);

    //fork second time
    if ( (child=fork())<0) { //failed fork
        fprintf(stderr,"error: failed fork\n");
        exit(EXIT_FAILURE);
    }
    if( child>0 ) { //parent
        exit(EXIT_SUCCESS);
    }

	//new file permissions
    umask(0);
    //change to path directory
    chdir(path);

    //Close all open file descriptors
    int fd;
/*    for( fd=sysconf(_SC_OPEN_MAX); fd>0; --fd )
    {
        close(fd);
    }
*/
    //reopen stdin, stdout, stderr
    stdin=fopen(infile,"r");   //fd=0
    stdout=fopen(outfile,"w+");  //fd=1
    stderr=fopen(errfile,"w+");  //fd=2

    //open syslog
    openlog(name,LOG_PID,LOG_DAEMON);
    return(0);
}


int main()
{
	pthread_attr_t attr;
 	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr,1024*1024);
 
	int status;
	printf("Creating Thread 1 in Daemon Process::\n");
 	status=pthread_create(&t1,&attr,(void*)&call_server_thread,NULL);
 	if(status!=0){
		 printf("Failed to create Thread1 with Status:%d\n",status);
 	}
 
	
 	printf("Creating Threads 2 in Daemon Process::\n");
 	status=pthread_create(&t2,&attr,(void*)&call_client_thread,NULL);
 	if(status!=0){
		printf("Failed to create Thread2 with Status:%d\n",status);
 	}
 	

 	pthread_join(t1,NULL);
 	pthread_join(t2,NULL);
		
	return 0;

}


void call_server_thread()
{
    int shmid;
    key_t key;
    char *shm, *s;
	
    /*
     * We need to get the segment named
     * "5678", created by the server.
     */
    key = 5678;

    /*
     * Locate the segment.
     */
    while(1){
	if ((shmid = shmget(key, MAXSZ, 0666)) < 0) {
        perror("shmget");
   //     exit(1);
    }

    /*
     * Now we attach the segment to our data space.
     */
    if ((shm = shmat(shmid, NULL, 0)) == (char *) -1) {
        perror("shmat");
  //      exit(1);
    }

    /*
     * Now read what the server put in the memory.
     */
	int i=0;
	s=shm;
    for (i = 0; i < strlen(shm); i++)
        putchar(shm[i]);
    putchar('\n');

    /*
     * Finally, change the first character of the 
     * segment to '*', indicating we have read 
     * the segment.
     */

	sleep(4);
	}
    exit(0);
}


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
	serverAddr.sin_port = htons(PORT);
	serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	memset(serverAddr.sin_zero, '\0', sizeof(serverAddr.sin_zero));
	/*Initialize size variable to be used later on*/
	addr_size = sizeof(serverAddr);
    

 	int n,res;
 	char msg1[MAXSZ];
 	char msg2[MAXSZ];



     if( (res=daemonize("mydaemon","~/CODE/PLATFORM/",NULL,NULL,NULL)) != 0 ) {
        fprintf(stderr,"error: daemonize failed\n");
        exit(EXIT_FAILURE);
    }

	connect(clientSocket,(struct sockaddr *)&serverAddr,sizeof(serverAddr));
	
	while(1){
		printf("Type a sentence to send to Main server:\n");
		//fgets(buffer,1024,stdin);
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
		nBytes=recv(clientSocket,buffer,MAXSZ,0);
	//	nBytes = recvfrom(clientSocket,buffer,1024,0,NULL, NULL);
		printf("Received from Main server: %s\n",buffer);
		sleep(3);
	}
	
}

#if 0
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
                printf("Daemon Serversocket() is OK \n");

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
                printf("Daemon Serversetsockopt()is OK\n");


        /* bind to an address */
        memset(&serveraddr, 0x00, sizeof(struct sockaddr_in));
        serveraddr.sin_family = AF_INET;
        serveraddr.sin_port = htons(CLIENTPORT);
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
                printf("Daemon Serverbind()is OK\n");

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
		printf("Daemon ServerReadyfor client connection...\n");
	
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
	//	close(sd);
	//	exit (1);
	}else
		printf("Daemon Serveraccept()is OK\n");
		
	/*client IP*/
	printf("Daemon Server new socket, sd2 is OK...\n");
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
				rc = read(sd2, buffer,BufferLength);
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
					printf("Data recieved from Main Client to Daemon Server:%s\n",buffer);
					totalcnt += rc;
					printf("Daemon Serverread() is OK\n");
				}
	//		}	
		}
	
	
		/* Shows the data */
		printf("Received data from the Main client: %s\n", buffer);	
	
		/* Echo some bytes of string, back */
		/* to the client by using the write() */
		/* function. */
		/*************************************/
		/* write() some bytes of string, */
		/* back to the client. */
		
		printf("Server Echoing back to Main client...\n");
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
			printf("Recived data to Daemon Server:%s\n",buffer);
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

#endif 
