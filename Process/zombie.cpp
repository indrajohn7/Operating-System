// A C program to demonstrate Zombie Process. 
// Child becomes Zombie as parent is sleeping
// when child process exits.
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include<bits/stdc++.h>

using namespace std;


int main()
{
    // Fork returns process id
    // in parent process
    pid_t child_pid = fork();
 
    // Parent process 
    if (child_pid > 0)
        sleep(50);
 
    // Child process
    else{       
        cout<<"Exiting from the child process:\n";
		exit(0);
	}
 
    return 0;
}

