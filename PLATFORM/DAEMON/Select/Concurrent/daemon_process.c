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
void connect_socket(int*);
void create_socket(int*,int*);

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
/*  int fd_open;
    for( fd_open=sysconf(_SC_OPEN_MAX); fd_open>0; --fd_open )
    {
        close(fd_open);
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

int fd;

int main()
{
	pthread_attr_t attr;
 	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr,1024*1024);
 
	int status;
	
	connect_socket(&fd);
	
 	printf("Creating Threads 2 in Daemon Process::\n");
 	status=pthread_create(&t2,&attr,(void*)&call_client_thread,NULL);
 	if(status!=0){
		printf("Failed to create Thread2 with Status:%d\n",status);
 	}

	printf("Creating Thread 1 in Daemon Process::\n");
 	status=pthread_create(&t1,&attr,(void*)&call_server_thread,NULL);
 	if(status!=0){
		 printf("Failed to create Thread1 with Status:%d\n",status);
 	}
 

 	

 	pthread_join(t1,NULL);
 	pthread_join(t2,NULL);
		
	return 0;

}




void connect_socket(int* fp1)
{
	int clientSocket, sd2,portNum, nBytes;
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


	*fp1=clientSocket;
	int ret;

connection:
	ret = connect(clientSocket,(struct sockaddr *)&serverAddr,sizeof(serverAddr));

	if(ret != 0)
		goto connection;
}	
	

void call_client_thread()
{
	int nBytes;
	char buffer[1024];
	int clientSocket=fd;
	while(1){
		printf("**************Type a sentence to send to Main server**************\n");
		//fgets(buffer,1024,stdin);
		int stream_size=sizeof(stream)/sizeof(char*);
		int t=rand()%stream_size;
		memset(buffer,'\0',1024);
		memcpy(buffer,stream[t],strlen(stream[t]));
		printf("You typed: %s :: ",buffer);
		nBytes = strlen(buffer) + 1;
		/*Send message to server*/
	//	sendto(clientSocket,buffer,nBytes,0,(struct sockaddr *)&serverAddr,addr_size);
//		if(semaphore){	
			send(clientSocket,buffer,nBytes,0);
			/*Receive message from server*/
			nBytes=recv(clientSocket,buffer,MAXSZ,0);
		//	nBytes = recvfrom(clientSocket,buffer,1024,0,NULL, NULL);
			printf("Received from Main server: %s\n",buffer);
			semaphore=1;
//		}
		sleep(3);
	}
	
}



void call_server_thread()
{
	int sd2=fd;
	int rc,temp,length;	
	char buffer[1024];
	fd_set read_fd; 
	while(1){

//		FD_ZERO(&read_fd);
//		FD_SET(sd2, &read_fd);
//		rc = select(sd2+1, &read_fd, NULL, NULL, NULL /*&timeout*/);

			/* Read data from the client. */
			int totalcnt = 0;
//			if((rc == 1) && (FD_ISSET(sd2, &read_fd))){
//			if(semaphore){
				rc = read(sd2, buffer,BufferLength);
				if(rc < 0){
					perror("Serverread() error");
					close(sd2);
					exit (1);			
				}else if(rc == 0){
					printf("Client program has issued a close()\n");
					close(sd2);
					exit(1); //dont exit():: break from here	
				}else{
					printf("Data recieved from Main Client to Daemon Server:%s\n",buffer);
					totalcnt += rc;
					printf("Daemon Serverread() is OK\n");
				}
//			}
//			}
	
	
		/* Shows the data */
		//printf("Received data from the Main client: %s\n", buffer);	
	
		/* Echo some bytes of string, back */
		/* to the client by using the write() */
		/* function. */
		/*************************************/
		/* write() some bytes of string, */
		/* back to the client. */
		
		printf("********************Server Echoing back to Main client*********************\n");
		int i;
		for(i=0;i<strlen(buffer);i++)
			buffer[i] = toupper(buffer[i]);
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
			printf("Sent data from Daemon  Server:%s\n",buffer);
		}
		
		semaphore = 1;
		sleep(3);
	
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
	exit(0);
}

