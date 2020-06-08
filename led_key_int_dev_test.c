#include <linux/types.h>
#include <linux/kernel.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    int fd;
    int len;
    int status = 0;

    if(argc < 2 || argc > 3){
        printf("invalid argument.\r\nexp: ./led_dev_test on | off | key\r\n");
        return -1;
    }

    fd = open("/dev/led_dev",O_RDWR);

    if(fd == -1){
        printf("can not open leddev. fd= %d \r\n",fd);
        return -1;
    }

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
        unsigned char keyval;       
        printf("waiting for key2 input...\r\n");
        while(1){
            read(fd, &keyval, sizeof(keyval));
            if(keyval != 1){
                printf("key2 pressed.\r\n");
                break;
            }
        }
    }
	else{
        printf("invalid argument.\r\nexp: ./led_dev_test on | off | key\r\n");
	}
    
    close(fd);

    return 0;
}
