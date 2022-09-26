#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "process_ioctl.h"

int fd;

int leave = 0; //string compare bye k liye

int idx;

void * send(void *inp) 
{
	char *message;
	message = (char *)malloc(64);

	while(!leave)
	{
		char rem[64];
		int length = -1;

		memset(message, 0, 64); // reset message because so previous store message again print na ho

		message[0] = idx;

		printf("> ");
		fgets(rem, 63, stdin); // 63 he jayga in rem  consolde may tpy = stdlin
		
		if(strcmp(rem, "Bye!\n") == 0)
		{
			leave = 1;
			break;
		}

		memcpy(message + 1, rem, 63); // copy msg to rem

		int ret = write(fd, message, 64);// 64msg write into  fd
		if(ret == -1)
		{
			printf("P%d write failed\n", idx);
		}
	}

	free(message);
	return NULL;
}


int main(int argc, char const *argv[]) 
{
	//opening character device and storing it to fd and giving permission Read and wright
	fd = open("/dev/mihir", O_RDWR);
	if( fd == -1) // failed
	{
		printf("P%d open failed\n", getpid());
		exit(1);
	}
// connect to chat room
	if(ioctl(fd, CONNECT_USER, &idx) == -1)//error aa raha hai
	{
		printf("P%d join failed\n", getpid());
		close(fd);
		exit(2);
	}
	else
	{
		//current user ka hai bus
		printf("P%d joined\n", idx+1);
		pthread_t t_id;

		pthread_create(&t_id, NULL, send, NULL);
// read kar raha
		char *message;
		message = (char *)malloc(64);

		while(!leave) 
		{
			memset(message, 0, 64);//reset

			int rflag = read(fd, message, 64);//fd to messasge may daal rahe

			if(rflag == -1) 
			{
				printf("P%d read failed\n", idx+1);
			}
			else if(rflag > 0)
			{
				printf("%s\n", message);
			}
		}

		free(message);
		pthread_join(t_id, NULL);
	}
	printf("P%d leaving  the chat\n", idx+1);
	close(fd);
	exit(0);
}