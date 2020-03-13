#include<stdio.h>
#include<stdlib.h>
#include<signal.h>
#include<mqueue.h>
#include<fcntl.h>


int main()
{
	int fields[2];
	memset(fields,0,sizeof(fields));
	pipe(fields);
	char buff[10];
	write(fields[1],buff,1);
	read(fields[0],buff,1);
	printf("VALUE:%d\n",fields[0]);
	return 0;
}
