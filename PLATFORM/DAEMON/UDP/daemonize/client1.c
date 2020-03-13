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

int main(){
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
	
	return 0;
}
