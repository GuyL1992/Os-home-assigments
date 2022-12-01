#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>


enum Process {EXECOMMAND, EXEBACKGROUND, PIPING, REDIRECTING}; // https://www.geeksforgeeks.org/enumeration-enum-c/
enum Level {PARENT, CHILD};
struct sigaction sa;


enum Process defineProcess(int count, char**arglist, int *separateLocation){ 
    int i;

    if (strcmp(arglist[count - 1],"&") == 0){ // background process case
        return EXEBACKGROUND;
    }

    for (i = 0; i < count; i++){

        if (strcmp(arglist[i], "|") == 0){ // piping case
            *separateLocation = i;
            return PIPING;
        }
        
        if (strcmp(arglist[i], ">") == 0){ // redirectig case
            *separateLocation = i;
            return REDIRECTING;
        }

    }

return EXECOMMAND; // standart case
}


static void initialize_signals(enum Level level){ // define the required SIGINT behavior 
    if (level == PARENT){
        signal(SIGINT, SIG_IGN); // https://en.cppreference.com/w/c/program/SIG_strategies , 
        return;
    }
        signal(SIGINT, SIG_DFL); // default behavior for child, except background    
}


//ERAN'S TRICK
static void child_handler(int sig) {
    pid_t pid;
    int status;
    while((pid = waitpid(-1, &status, WNOHANG)) > 0);
}

static int spec_waitpid(int pid){
    // Ignore only EINTR and ECHILD
    if (waitpid(pid, NULL, 0) < 0 && (errno != EINTR && errno != ECHILD)) { // https://man7.org/linux/man-pages/man3/errno.3.html, https://linux.die.net/man/2/waitpid
        fprintf(stderr,"Error with waitpid process: %s\n", strerror(errno));
        return 0;
    }

    return 1;
}

static int prevent_zombies_handler() { 
    //ERAN'S TRICK
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART; 
    sa.sa_handler = child_handler;
     if (sigaction(SIGCHLD, &sa, NULL) == -1){ // https://man7.org/linux/man-pages/man2/sigaction.2.html#RETURN_VALUE
        fprintf(stderr,"Error with waitpid process: %s\n", strerror(errno));
        exit(1);
     }

     return 1;
}


int piping(char* commandA, char** commandArgsA, char* commandB, char** commandArgsB){
    int fd[2];
    int pid_a;
    int pid_b;

    if (pipe(fd) < 0){
        fprintf(stderr,"Error with PIPE: %s\n", strerror(errno));
        return 0;
    }

    pid_a = fork();
    

    if (pid_a < 0){
        fprintf(stderr,"Error with Fork: %s\n", strerror(errno));
        return 0;
    }
    /* first command child case */ 

    else if (pid_a == 0){
        initialize_signals(CHILD); //don't ignore SIGINT

        if (dup2(fd[1],1) == -1){
            fprintf(stderr,"Error with dup2: %s\n", strerror(errno)); // https://www.mkssoftware.com/docs/man3/dup2.3.asp
            exit(1);
        }; // https://www.youtube.com/watch?v=5fnVr-zH-SE

        close(fd[0]);
        close(fd[1]);

        if (execvp(commandA,commandArgsA) < 0){
            fprintf(stderr,"Error: %s\n", strerror(errno));
            exit(1);
        }
    }
    /* back tto parent */ 

    else { 
        pid_b = fork();

        if (pid_b  < 0){
            fprintf(stderr,"Error with Fork: %s\n", strerror(errno));
            return 0;
        }
        
        /* second xommand child case */ 

        else if (pid_b == 0){
            initialize_signals(CHILD); // don't ignore sigint

            if (dup2(fd[0], 0) == -1){
                fprintf(stderr,"Error with dup2: %s\n", strerror(errno)); // https://www.mkssoftware.com/docs/man3/dup2.3.asp
                exit(1);
            }

            close(fd[1]);
            close(fd[0]);

            if (execvp(commandB,commandArgsB) < 0){
                fprintf(stderr,"Error: %s\n", strerror(errno));
                exit(1);
            }
        }
        /* back to parent */ 

        else {
            close(fd[0]);
            close(fd[1]);
            return spec_waitpid(pid_a) && spec_waitpid(pid_b);
            
        }
        
    }
    return 1;
}

int redirecting(char* command, char** commandArgs, char* fileName){

    int pid;
    int fd = open(fileName, O_WRONLY|O_CREAT|O_TRUNC, S_IRWXU); // https://man7.org/linux/man-pages/man2/open.2.html, https://man7.org/linux/man-pages/man2/open.2.html

    pid = fork();

    if (pid  < 0){
            fprintf(stderr,"Error with Fork: %s\n", strerror(errno));
            return 0;
        }

    else if (pid  == 0){
        initialize_signals(CHILD);

        if (dup2(fd, 1) == -1){ // https://www.geeksforgeeks.org/dup-dup2-linux-system-call/
            fprintf(stderr,"Error with dup2: %s\n", strerror(errno)); // https://www.mkssoftware.com/docs/man3/dup2.3.asp
            exit(1);
        }

        close(fd);
        if (execvp(command,commandArgs) < 0){
                fprintf(stderr,"Error: %s\n", strerror(errno));
                exit(1);
        }

    }       
    else {
        close(fd);
        return spec_waitpid(pid);
    }
    return 1;
}

int backGroundProcess(char* command, char** commandArgs){
    int pid;
    pid = fork();

    if (pid < 0){
        fprintf(stderr,"Error with Fork: %s\n", strerror(errno));
        return 0;
    }

    else if (pid == 0) {
        initialize_signals(PARENT); // should not terminate UPON SIGINT like parent behavior

        if (execvp(command,commandArgs) < 0){
            fprintf(stderr,"Error: %s\n", strerror(errno));
            exit(1);
        }}

    return prevent_zombies_handler();
}

int standartProcess(char* command, char** commandArgs){
    int pid;

    pid = fork();

    switch (pid)
    {
        case 0: // child case
            initialize_signals(CHILD); // don't ignore SIGINT
            if (execvp(command,commandArgs) < 0){
                fprintf(stderr,"Error: %s\n", strerror(errno));
                exit(1);
            }
            exit(1);
            break;
        
        case -1: // Failure
            fprintf(stderr,"Error with FORK : %s\n", strerror(errno));
            return 0;
        
        default: // parent case
            return spec_waitpid(pid);
                
    }

    return 1;
}

    

int process_arglist(int count, char** arglist){
    enum Process action;
    int separateLocation;
    char* commandA = NULL;
    char* commandB = NULL;
    char** commandArgsA = NULL;
    char** commandArgsB = NULL;
    char* fileName = NULL;

    action = defineProcess(count, arglist, &(separateLocation));
    commandA = arglist[0];

    if (action == PIPING){
        commandB = arglist[separateLocation + 1];
        commandArgsA = arglist;
        commandArgsB= &(arglist[separateLocation + 1]);
        arglist[separateLocation] = NULL;
        return piping(commandA, commandArgsA, commandB, commandArgsB);
    }

    else if (action == REDIRECTING){
        arglist[separateLocation] = NULL;
        commandArgsA = arglist;
        fileName = arglist[separateLocation + 1];
        return redirecting(commandA, commandArgsA, fileName);
    }

    else if (action == EXEBACKGROUND){
        arglist[count - 1] = NULL;
        return backGroundProcess(commandA, arglist);
    }

    else { 
         return standartProcess(commandA, arglist);
    }

    return 1;
}

int prepare(void){
    initialize_signals(PARENT);
    return 0;
}

int finalize(void){
    return 0;
}



