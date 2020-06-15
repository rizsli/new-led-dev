
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <poll.h>
#include <signal.h>
#include <pthread.h>

static char blk_flg;

static void sig_func(int sig)
{
blk_flg = 0;
}

void *stat_thread(void *arg){
	int i=0;
	while(1){
		sleep(1);
		printf("you have been blinking for %d secs, press key2 to quit.\r\n",(++i));
	}
}

int main(int argc, char **argv)
{
    int fd;
    int len;
    int status = 0;
	unsigned char keyval; 
	struct pollfd fds[1];
	int retval;
	int timeout_ms = 5000;
	int flags;
	pthread_t tid1; 

    if(argc < 2 || argc > 3){
        printf("invalid argument.\r\nexp: ./led_dev_test on | off | key\r\n");
        return -1;
    }
	
	signal(SIGIO, sig_func);

    fd = open("/dev/led_dev",O_RDWR);

    if(fd == -1){
        printf("can not open leddev. fd= %d \r\n",fd);
        return -1;
    }

	fds[0].fd = fd;
	fds[0].events = POLLIN;

	
	fcntl(fd, F_SETOWN, getpid());
	flags = fcntl(fd, F_GETFL);
	fcntl(fd, F_SETFL, flags | FASYNC);

    if(strcmp(argv[1],"on") == 0){
        status = 1;
        write(fd, &status, 1);
    }
    else if(strcmp(argv[1],"off") == 0)
    {
        status = 0;
        write(fd, &status, 1);
    }
    else if(strcmp(argv[1],"key") == 0)
    {      
        printf("waiting for key2 input...\r\n");
        while(1){
			retval = poll(fds, 1, timeout_ms);
			if ((retval == 1) && (fds[0].revents & POLLIN))
			{
				read(fd, &keyval, sizeof(keyval));
				printf("key2 pressed.\r\n");
				break;
			}
			else
			{
				printf("key2 timeout\n");
				break;
			}
			
        }
    }
	else if(strcmp(argv[1],"blink") == 0)
    {   
    	retval = pthread_create(&tid1, NULL, stat_thread, NULL);
		if(retval != 0){
		perror("pthread create error");
		return -1;
		}
		
        printf("blinking...\r\n");
		blk_flg = 1;
        while(blk_flg){
        status = 1;
        write(fd, &status, 1);
		usleep(200000);
		
        status = 0;
        write(fd, &status, 1);
		usleep(200000);
        }
		printf("blink end.\r\n");
    }
	else{
        printf("invalid argument.\r\nexp: ./led_dev_test on | off\r\n");
	}
    
    close(fd);

    return 0;
}

