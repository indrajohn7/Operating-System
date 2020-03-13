#include<bits/stdc++.h>
#include  <stdio.h>
#include  <stdlib.h>
#include  <sys/types.h>
#include  <sys/ipc.h>
#include  <sys/shm.h>
#include <errno.h>

void  ClientProcess(int []);

void  main(int  argc, char *argv[])
{
     int    ShmID;
     int    *ShmPTR;
     pid_t  pid;
     int    status;

     if (argc != 5) {
          printf("Use: %s #1 #2 #3 #4\n", argv[0]);
          exit(1);
     }
	 int key = 99999;	
	 int is_present = 0;
     ShmID = shmget(key, 4*sizeof(int), IPC_CREAT | IPC_EXCL |0666);
     if (ShmID < 0) {
          printf("*** shared memory already existing in  (server) :: ERRCODE: %d\n", ShmID);
		  is_present = 1;
     	  ShmID = shmget(key, 4*sizeof(int), IPC_CREAT | 0666);
     }
     printf("Server has received a shared memory of four integers...\n");

     ShmPTR = (int *) shmat(ShmID, NULL, 0);
     if ( ShmPTR == (int*)-1) {
          printf("*** shmat error (server) ***\n");
          exit(1);
     }
     printf("Server has attached the shared memory...\n");
	 if(!is_present) {
     	ShmPTR[0] = atoi(argv[1]);
     	ShmPTR[1] = atoi(argv[2]);
  	   	ShmPTR[2] = atoi(argv[3]);
   	  	ShmPTR[3] = atoi(argv[4]);
	 }
     printf("Server has filled %d %d %d %d in shared memory...\n",
            ShmPTR[0], ShmPTR[1], ShmPTR[2], ShmPTR[3]);

     printf("Server is about to fork a child process...\n");
     pid = fork();
     if (pid < 0) {
          printf("*** fork error (server) ***\n");
          exit(1);
     }
     else if (pid == 0) {
          ClientProcess(ShmPTR);
          exit(0);
     }

     wait(&status);
     printf("Server has detected the completion of its child...\n");
     shmdt((void *) ShmPTR);
     printf("Server has detached its shared memory...\n");
//     shmctl(ShmID, IPC_RMID, NULL);
//     printf("Server has removed its shared memory...\n");
     printf("Server exits...\n");
     exit(0);
}

void  ClientProcess(int  SharedMem[])
{
     printf("   Client process started\n");

     printf("   Client found %d %d %d %d in shared memory\n",
                SharedMem[0], SharedMem[1], SharedMem[2], SharedMem[3]);
	 for(int i = 0; i < 4; i++)
	 	SharedMem[i] += 1;
     printf("   Client is about to exit\n");
}

