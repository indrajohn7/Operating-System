#include <stdio.h>    //printf(3)
#include <stdlib.h>   //exit(3)
#include <unistd.h>   //fork(3), chdir(3), sysconf(3)
#include <signal.h>   //signal(3)
#include <sys/stat.h> //umask(3)
#include <syslog.h>   //syslog(3), openlog(3), closelog(3)

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

int
main()
{
    int res;
    int ttl=120;
    int delay=5;
    if( (res=daemonize("mydaemon","~/CODE/PLATFORM",NULL,NULL,NULL)) != 0 ) {
        fprintf(stderr,"error: daemonize failed\n");
        exit(EXIT_FAILURE);
    }
    FILE* fp=fopen("syslog.txt","w+");
	while( ttl>0 ) {
        //daemon code here
        syslog(LOG_NOTICE,"daemon ttl %d",ttl);
		fprintf(fp,"logging info...+delay:%d\n",delay);
		fflush(fp);
        sleep(delay);
        ttl-=delay;
    }
    syslog(LOG_NOTICE,"daemon ttl expired");
    closelog();
	fclose(fp);
    return(EXIT_SUCCESS);
}


