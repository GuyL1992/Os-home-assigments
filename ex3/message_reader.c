#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h> 
#include <unistd.h> 
#include <errno.h>
#include <sys/ioctl.h>
#include "message_slot.h"

enum ERROR_TYPE {ARGC, FILE_E, IOCTL, READ, WRITE_CONSOLE};


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
            fprintf(stderr, "Invalid argument");
            close(fd);
            exit(1);
            break;
        
        case(READ):
            fprintf(stderr, "Invalid argument");
            close(fd);
            exit(1);
            break;
        
        case(WRITE_CONSOLE):
            fprintf(stderr, "A problem has occured with CONSOLE WRITE process");
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
    int fd;
    int res;
    char message[128];

    if (argc != 3)
        error_exit(ARGC,fd);

    path = argv[1];
    channel_id = atoi(argv[2]);

    if ((fd = open(path, O_RDWR)) < 0)
        error_exit(FILE_E,fd);

    if ((res = ioctl(fd, MSG_SLOT_CHANNEL, channel_id)) < 0)
        error_exit(IOCTL,fd);

    if ((res = read(fd, message, 128)) < 0)
        error_exit(READ,fd);
    
    else {
        if (write(STDOUT_FILENO, message, res) < 0)
            error_exit(WRITE_CONSOLE,fd);
    }
    
    close(fd);

    exit(0);
}

