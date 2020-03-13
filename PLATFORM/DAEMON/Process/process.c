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
#define MAXSZ 100
#define SERVER_IP "127.0.0.1"

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
 
/*
 printf("Creating Threads2::\n");
 status=pthread_create(&t2,&attr,(void*)&call_client_thread,NULL);
 if(status!=0){
	 printf("Failed to create Thread2 with Status:%d\n",status);
 }
 */

 pthread_join(t1,NULL);
 //pthread_join(t2,NULL);

 return 0;

}

void call_server_thread()
{
 //create socket
 int sockfd;//to create socket
 int newsockfd;//to accept connection

 struct sockaddr_in serverAddress;//server receive on this address
 struct sockaddr_in clientAddress;//server sends to client on this address

 int n,exec_sec=3;
 char msg[MAXSZ];
 int clientAddressLength;
 int pid;
 sockfd=socket(AF_INET,SOCK_STREAM,0);
 //initialize the socket addresses
 memset(&serverAddress,0,sizeof(serverAddress));
 serverAddress.sin_family=AF_INET;
 serverAddress.sin_addr.s_addr=htonl(INADDR_ANY);
 serverAddress.sin_port=htons(PORT);

 //bind the socket with the server address and port
 bind(sockfd,(struct sockaddr *)&serverAddress, sizeof(serverAddress));

 //listen for connection from client
 listen(sockfd,5);

 while(1)
 {
  //parent process waiting to accept a new connection
  printf("\n*****server waiting for new client connection:*****\n");
  clientAddressLength=sizeof(clientAddress);
  newsockfd=accept(sockfd,(struct sockaddr*)&clientAddress,&clientAddressLength);
 // printf("connected to client: %d\n",inet_ntoa(clientAddress.sin_addr));

  //child process is created for serving each new clients
  pid=fork();
  if(pid==0)//child process rec and send
  {
   //rceive from client
   while(1)
   {
    n=recv(newsockfd,msg,MAXSZ,0);
    if(n==0)
    {
     close(newsockfd);
     break;
    }
    msg[n]='\0';
    send(newsockfd,msg,n,0);

    printf("Receive and set:%s\n",msg);
  	printf("Server Thread will sleep for %d seconds\n",exec_sec);
  	sleep(exec_sec);
   }//close interior while
  exit(0);
  }
  else
  {
   close(newsockfd);//sock is closed BY PARENT
  }
  printf("Server Thread will sleep for %d seconds\n",exec_sec);
  sleep(exec_sec);
 }//close exterior while

}

/************Client Code***********/
//CLIENT ITERATIVE


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
    for( fd=sysconf(_SC_OPEN_MAX); fd>0; --fd )
    {
        close(fd);
    }

    //reopen stdin, stdout, stderr
    stdin=fopen(infile,"r");   //fd=0
    stdout=fopen(outfile,"w+");  //fd=1
    stderr=fopen(errfile,"w+");  //fd=2

    //open syslog
    openlog(name,LOG_PID,LOG_DAEMON);
    return(0);
}



void  call_client_thread()
{
 int sockfd;//to create socket

 struct sockaddr_in serverAddress;//client will connect on this

 int n,exec_sec,res;
 char msg1[MAXSZ];
 char msg2[MAXSZ];

 //create socket
 sockfd=socket(AF_INET,SOCK_STREAM,0);
 //initialize the socket addresses
 memset(&serverAddress,0,sizeof(serverAddress));
 serverAddress.sin_family=AF_INET;
 serverAddress.sin_addr.s_addr=inet_addr(SERVER_IP);
 //serverAddress.sin_addr.s_addr=htonl(INADDR_ANY);
 serverAddress.sin_port=htons(PORT);

 //client  connect to server on port
 connect(sockfd,(struct sockaddr *)&serverAddress,sizeof(serverAddress));
 //send to sever and receive from server
  if( (res=daemonize("mydaemon","~/CODE/PLATFORM",NULL,NULL,NULL)) != 0 ) {
      fprintf(stderr,"error: daemonize failed\n");
      exit(EXIT_FAILURE);
  }

  FILE* fp=fopen("syslog.txt","w+");
  
 while(1)
 {
  fprintf(fp,"\nEnter message to send to server:\n");
 // fgets(msg1,MAXSZ,stdin);
  memcpy(msg1,"INDRA",strlen("INDRA"));
  if(msg1[0]=='#')
   break;
   fprintf(fp,"logging info...+msg:%s\n",msg1);
   fflush(fp);
//Dop your operation here before sending
  n=strlen(msg1)+1;
  send(sockfd,msg1,n,0);

  n=recv(sockfd,msg2,MAXSZ,0);

  fprintf(fp,"Receive message from  server::%s\n",msg2);
 }
 syslog(LOG_NOTICE,"daemon ttl expired");
 closelog();
 fclose(fp);

}

