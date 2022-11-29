#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>


enum Process {EXECOMMAND, EXEBACKGROUND, PIPING, REDIRECTING};
enum Level {PARENT, CHILD};
struct sigaction sa;


enum Process defineProcess(int count, char**arglist, int *separateLocation){
    int i = 0;

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
        signal(SIGINT, SIG_IGN);
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
    if (waitpid(pid, NULL, 0) < 0 && (errno == EINTR || errno != ECHILD)){ // ignore only EINTR and ECHILD errors
        fprintf(stderr,"Error with waitpid process: %s\n", strerror(errno));
        return 0; // zero or 1??? 
    }
    return 1;
}

static void prevent_zombies_handler() {
    //ERAN'S TRICK
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sa.sa_handler = child_handler;
    sigaction(SIGCHLD, &sa, NULL);
}


int piping(char* commandA, char** commandArgsA, char* commandB, char** commandArgsB){
    int fd[2];
    int pid_a;
    int pid_b;

    if (pipe(fd) < 0){
        fprintf(stderr,"Error: %s\n", strerror(errno));
    }

    pid_a = fork();

    if (pid_a < 0){
        fprintf(stderr,"Error: %s\n", strerror(errno));
        exit(1);
    }
    
    else if (pid_a == 0){
        initialize_signals(CHILD); //don't ignore SIGINT
        dup2(fd[1],1);
        close(fd[0]);
        close(fd[1]);
        if (execvp(commandA,commandArgsA) < 0){
            fprintf(stderr,"Error: %s\n", strerror(errno));
            exit(1);
        }
    }
    else { 
        pid_b = fork();

        if (pid_b  < 0){
            fprintf(stderr,"Error: %s\n", strerror(errno));
            exit(1);
        }
        
        else if (pid_b == 0){
            initialize_signals(CHILD); // don't ignore sigint
            dup2(fd[0], 0);
            close(fd[1]);
            close(fd[0]);
            if (execvp(commandB,commandArgsB) < 0){
                fprintf(stderr,"Error: %s\n", strerror(errno));
                exit(1);
            }        
        else {
            close(fd[0]);
            close(fd[1]);
            spec_waitpid(pid_a);
            spec_waitpid(pid_b);
        }

    }}
    return 1;
}

int redirecting(char* command, char** commandArgs, char* fileName){

    int pid;
    int fd = open(fileName, O_WRONLY|O_CREAT|O_TRUNC, S_IRWXU); // https://man7.org/linux/man-pages/man2/open.2.html, https://man7.org/linux/man-pages/man2/open.2.html

    pid = fork();
    if (pid  == 0){
        initialize_signals(CHILD);
        dup2(fd, 1); 
        close(fd);
        if (execvp(command,commandArgs) < 0){
                fprintf(stderr,"Error: %s\n", strerror(errno));
                exit(1);
        }}       
        else {
            close(fd);
            spec_waitpid(pid);
        }
    return 1;
}

int backGroundProcess(char* command, char** commandArgs){
    int pid;
    pid = fork();

    if (pid < 0){
        fprintf(stderr,"Error: %s\n", strerror(errno));
        return 1; // terminate the shell (?)
    }

    else if (pid == 0) {
        initialize_signals(PARENT); // should not terminate UPON SIGINT like parent behavior
        if (execvp(command,commandArgs) < 0){
            fprintf(stderr,"Error: %s\n", strerror(errno));
            exit(1);
        }}
    prevent_zombies_handler();
    return 1;
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
                exit(-1);
            }
            exit(1);
            break;
        
        case -1: // check for -1 or could be something else 
            fprintf(stderr,"Error with FORK : %s\n", strerror(errno));
            exit(-1);
            return 0;
            break;
        
        default: // parent case
            spec_waitpid(pid);
                
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

    if (action == PIPING){
        commandA = arglist[0];
        commandB = arglist[separateLocation + 1];
        commandArgsA = arglist;
        commandArgsB= &(arglist[separateLocation + 1]);
        arglist[separateLocation] = NULL;
        return piping(commandA, commandArgsA, commandB, commandArgsB);
    }

    else if (action == REDIRECTING){
        commandA = arglist[0];
        arglist[separateLocation] = NULL;
        commandArgsA = arglist;
        fileName = arglist[separateLocation + 1];
        return redirecting(commandA, commandArgsA, fileName);
    }

    else if (action == EXEBACKGROUND){
        commandA = arglist[0];
        arglist[count - 1] = NULL;
        return backGroundProcess(commandA, arglist);
    }

    else { 
         return standartProcess(arglist[0], arglist);
    }

    return 1;
}



int prepare(void){
    printf("%d\n", getpid());
    initialize_signals(PARENT);
    return 0;
}

int finalize(void){
    return 0;
}



