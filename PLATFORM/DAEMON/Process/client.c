/************Client Code***********/
//CLIENT ITERATIVE

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
 int sockfd;//to create socket

 struct sockaddr_in serverAddress;//client will connect on this

 int n,res;
 char msg1[MAXSZ];
 char msg2[MAXSZ];

 //create socket
 sockfd=socket(AF_INET,SOCK_STREAM,0);
 //initialize the socket addresses
 memset(&serverAddress,0,sizeof(serverAddress));
 serverAddress.sin_family=AF_INET;
 serverAddress.sin_addr.s_addr=inet_addr(SERVER_IP);
 serverAddress.sin_port=htons(PORT);

 //client  connect to server on port

 //send to sever and receive from server
 
     if( (res=daemonize("mydaemon","~/CODE/PLATFORM",NULL,NULL,NULL)) != 0 ) {
        fprintf(stderr,"error: daemonize failed\n");
        exit(EXIT_FAILURE);
    }
 
 connect(sockfd,(struct sockaddr *)&serverAddress,sizeof(serverAddress));
 while(1)
 {
  printf("\nEnter message to send to server:\n");
  //fgets(msg1,MAXSZ,stdin);
  
  memcpy(msg1,"INDRA",strlen("INDRA"));
  
  if(msg1[0]=='#')
   break;

  n=strlen(msg1)+1;
  send(sockfd,msg1,n,0);

  n=recv(sockfd,msg2,MAXSZ,0);

  printf("Receive message from  server::%s\n",msg2);
  sleep(3);
 }

 return 0;
}

