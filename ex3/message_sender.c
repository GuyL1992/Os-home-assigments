#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h> 
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "message_slot.h"

enum ERROR_TYPE {ARGC, FILE_E, IOCTL, WRITE};


static void error_exit(enum ERROR_TYPE e, int fd) {

    switch (e){
        case(ARGC):
            fprintf(stderr,"Invalid number of arguments");
            exit(1);
            break;
        
        case(FILE_E):
            fprintf(stderr, "FIle can not be opened");
            exit(1);
            break;
        
        case(IOCTL):
            printf("A problem has occured with IOCTL process\n");
            fprintf(stderr, "A problem has occured with IOCTL process");
            close(fd);
            exit(1);
            break;
        
        case(WRITE):
            fprintf(stderr, "Message too long\n");
            close(fd);
            exit(1);
            break;
        
        default:
            fprintf(stderr, "A problem has occured");
        
        
        
        exit(1);

    } 

}


int main (int argc, char* argv[]) {


    char* path;
    int channel_id;
    char* message;
    int fd = -1;
    int res;

    if (argc != 4)
        error_exit(ARGC,fd);

    path = argv[1];
    channel_id = atoi(argv[2]);
    message = argv[3];

    if ((fd = open(path, O_WRONLY)) < 0)
        error_exit(FILE_E,fd);
    
    printf("1\n");


    if ((res = ioctl(fd, MSG_SLOT_CHANNEL, channel_id)) < 0)
        error_exit(IOCTL,fd);
    
    printf("%d\n", res);
    
    printf("2\n");

    if ((res = write(fd, message, strlen(message))) < 0)
        error_exit(WRITE,fd);
    
    printf("3\n");

    
    close(fd);

    

exit(0);
}