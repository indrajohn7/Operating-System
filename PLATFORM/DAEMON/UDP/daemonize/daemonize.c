/************* UDP SERVER/ CLIENT DEMONIZED CODE *******************/

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


#define PORT 8000
#define SERVER_IP "127.0.0.1"
#define MAXSZ 100



char* stream[6]={	"Indra",
					"Natasha",
					"Soumya",
					"Tanu",
					"Kous",
					"njknk" 
				};




void call_server_thread();
void call_client_thread();

pthread_t t1;
pthread_t t2;

int main()
{
 int sockfd;//to create socket
 int newsockfd;//to accept connection
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




/*************************************  SERVER THREAD ******************************/

void call_server_thread()
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


	
	//return 0;
}

/*********************  Daemonization of client *****************************/

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

/*************************************** CLIENT CODE  **********************************/

void call_client_thread()
{
	int clientSocket, portNum, nBytes;
	char buffer[1024];
	struct sockaddr_in serverAddr;
	socklen_t addr_size;
	/*Create UDP socket*/
	clientSocket = socket(PF_INET, SOCK_DGRAM, 0);
	/*Configure settings in address struct*/
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(PORT);
	serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);
	/*Initialize size variable to be used later on*/
	addr_size = sizeof serverAddr;


 	int n,res;
 	char msg1[MAXSZ];
 	char msg2[MAXSZ];



     if( (res=daemonize("mydaemon","~/CODE/PLATFORM",NULL,NULL,NULL)) != 0 ) {
        fprintf(stderr,"error: daemonize failed\n");
        exit(EXIT_FAILURE);
    }

	connect(clientSocket,(struct sockaddr *)&serverAddr,sizeof(serverAddr));
	
	while(1){
		printf("Type a sentence to send to server:\n");
		//fgets(buffer,1024,stdin);
		int stream_size=sizeof(stream)/sizeof(char*);
		int t=rand()%stream_size;
		memset(buffer,'\0',1024);
		memcpy(buffer,stream[t],strlen(stream[t]));
		printf("You typed: %s",buffer);
		nBytes = strlen(buffer) + 1;
		/*Send message to server*/
		sendto(clientSocket,buffer,nBytes,0,(struct sockaddr *)&serverAddr,addr_size);
		/*Receive message from server*/
		nBytes = recvfrom(clientSocket,buffer,1024,0,NULL, NULL);
		printf("Received from server: %s\n",buffer);
		
		sleep(3);
	}
	
	//return 0;
}
