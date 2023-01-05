#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/limits.h>
#include <errno.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <threads.h>
#include  <fcntl.h>
#include <stdatomic.h>

// ################### DEFINNE STRUCTS ##########################

struct mission {
    char* directory;
    struct mission* next;
} typedef mission;

struct queue {
    mission* head;
    mission* tail;
    int* mission_count;
    int* thread_count;
    int* thread_num;
    char* search_term;
    int matches_count;
} typedef queue;

struct node {
    int id;
    struct node* next;
};

struct queue_node { 
    struct node* head;
    struct node* tail;
};

// ################### DEFINNE GLOBAL LOCKS AND CV ##############


mtx_t queue_lock;
mtx_t sync_lock;
mtx_t sleep_lock;
cnd_t GO;
cnd_t notEmpty;
cnd_t laavoda;
cnd_t* conditions;

atomic_int thread_initiated = 0;
atomic_int requested_threads_num;
atomic_int sleeping_threads = 0;
char* search_term;
atomic_int matches_count;
queue* mission_queue;
struct queue_node* queue_node;
atomic_int error_thread = 0;
mission* root_mission;
char* root_diretory;

// ################### DEFINNE QUEUE FUNCTION ###################

void enqueue_node(long id){
    struct node* my_node = malloc(sizeof(struct node));

    if (my_node == NULL){
        fprintf(stderr,"memory allocation failed\n");
        error_thread = 1;
    }

    sleeping_threads++;
    my_node->id = id;
    my_node->next = NULL;

    if (queue_node->tail == NULL){
        queue_node->tail = my_node;
        queue_node->head = my_node;
        return;
    }
        queue_node->tail->next = my_node;
        queue_node->tail = my_node;
}

void dequeue_node(int finish){

    struct node* extracted_node = queue_node->head;

    if (extracted_node == NULL)
        return;
    
    if (queue_node->head->next == NULL){
        queue_node->head = NULL;
        queue_node->tail = NULL;
    }
    else {
        queue_node->head = queue_node->head->next;
    }
    extracted_node->next = NULL;
    cnd_signal(&conditions[extracted_node->id]);

    if (!finish)
        sleeping_threads--;
}

void enqueue(queue* queue, mission* mission){
    
    if (queue->tail == NULL){
        queue->tail = mission;
        queue->head = mission;
        return;
    }

    queue->tail->next = mission;
    queue->tail = mission;
}   

mission* dequeue(queue* queue){

    mission* extracted_mission = queue->head;

    if (extracted_mission == NULL)
        return NULL;
    
    if (queue->head->next == NULL){
        queue->head = NULL;
        queue->tail = NULL;
    }
    else {
        queue->head = queue->head->next;
    }

    return extracted_mission;
}

void memory_error_hanlder(){ 
    fprintf(stderr,"memory allocation failed\n");
    exit(1);
}
void simple_search(mission* mission, char* search_term) {

    DIR *dir;
    struct dirent* entry;
    struct stat dirent;
    char* full_path;
    struct mission* new_mission;
    
    
    //https://www.ibm.com/docs/en/zos/2.4.0?topic=functions-readdir-read-entry-from-directory
    if ((dir = opendir(mission->directory)) == NULL){
        printf("Can NOT open directory %s\n", mission->directory);
        return;
    }
    
    while ((entry = readdir(dir)) != NULL) {

        if (strcmp(entry->d_name,".") == 0 || strcmp(entry->d_name,"..") == 0)
            continue;
        
        full_path = calloc(PATH_MAX,sizeof(char));

        if (full_path == NULL) {
            error_thread = 1;
            continue;
        }

        sprintf(full_path,"%s/%s",mission->directory,entry->d_name);

        if(stat(full_path,&dirent) < 0){
            error_thread = 1;
            continue;
        }

        if(S_ISDIR(dirent.st_mode)){
            
            // https://www.geeksforgeeks.org/access-command-in-linux-with-examples/
            // https://linux.die.net/man/2/access 

            if (access(full_path, R_OK) < 0 || access(full_path, X_OK) < 0){
                printf("Directory %s: Permission denied.\n",full_path);
                continue;
            }
                mtx_lock(&queue_lock);
                new_mission = malloc(sizeof(struct mission));

                if (new_mission == NULL){
                    error_thread = 1;
                    continue;
                }

                new_mission->directory = full_path;
                enqueue(mission_queue, new_mission);
                dequeue_node(0);
                mtx_unlock(&queue_lock);
                continue;
            }

            //https://stackoverflow.com/questions/12784766/check-substring-exists-in-a-string-in-c
            if (strstr(entry->d_name, search_term) != NULL){
                printf("%s\n", full_path);
                matches_count++;
            }      
    }
    return;
}

//https://www.includehelp.com/c-programs/check-a-given-filename-is-a-directory-or-not.aspx
int isDir(const char* fileName) {
    struct stat path;
    if (stat(fileName, &path) < 0){
        fprintf(stderr, "Can not stat root directory");
        exit(1);
    }
    return S_ISDIR(path.st_mode);
}

int thread_search(void* t){

    mission* my_mission;
    long id = (long)(t);
    mtx_lock(&sync_lock);
    thread_initiated++;
    
    /* signal the main thread that all threads are ready*/
    if (thread_initiated == requested_threads_num)
        cnd_signal(&GO);

    /* wait for signal from the main thread*/
    cnd_wait(&laavoda, &sync_lock);
    mtx_unlock(&sync_lock);

    while(1) {

        mtx_lock(&queue_lock);

        while(mission_queue->head == NULL) { 

            /* if the there are no mission check if you are the only one who is awake*/
            if(sleeping_threads + 1 == thread_initiated){
                dequeue_node(1); // each thread will wake the next sleeping one
                mtx_unlock(&queue_lock);
                thrd_exit(0);

            }
            /*
            there might be another work in the future - go to sleep on it's condition
            */
            enqueue_node(id);
            cnd_wait(&conditions[id], &queue_lock);
        }

        /*if the queue is not empty - take out new "mission" and start searching for it's diretory*/

        my_mission = dequeue(mission_queue);
        mtx_unlock(&queue_lock);
        if (my_mission != NULL)
            simple_search(my_mission, search_term);
    }

}

int validate_input(int argc, char** argv){

    if (argc != 4){
        fprintf(stderr,"Invalid number of arguments\n");
        return 0;
    }

     if (!isDir(argv[1])){
        fprintf(stderr,"Invalid root directory\n");
        return 0;
    }

    if (access(argv[1], R_OK | X_OK) < 0){ 
        fprintf(stderr,"Root directory is unsearchable\n");
        return 0;
    }
    
    return 1;
}

void destory_resources(){
    mtx_destroy(&queue_lock);
    cnd_destroy(&GO);
    mtx_destroy(&sync_lock);
    cnd_destroy(&laavoda);
}

void prepare(){

    conditions = calloc(requested_threads_num,sizeof(cnd_t));
    if (conditions == NULL)
        memory_error_hanlder();

    queue_node = malloc(sizeof(queue_node));
    if (queue_node == NULL)
        memory_error_hanlder();
    
    queue_node->head = NULL;
    queue_node->tail = NULL;

    cnd_init(&GO);
    mtx_init(&queue_lock, mtx_plain);
    mtx_init(&sync_lock, mtx_plain);
    cnd_init(&laavoda);

    for (int i = 0; i < requested_threads_num; i++){
        cnd_init(&conditions[i]);
    }

    mission_queue = malloc(sizeof(struct queue));
    if (mission_queue == NULL)
        memory_error_hanlder();

    mission_queue->tail = NULL;
    mission_queue->head =NULL;

    root_mission = malloc(sizeof(struct mission));
    root_mission->directory = strdup(root_diretory);
}

void threads_get_ready_and_go(thrd_t* threads){
    mtx_lock(&sync_lock);

    for (long i = 0; i <requested_threads_num; i++) {
        if (thrd_create(&threads[i], thread_search, (void*)i)){
            fprintf(stderr,"Error, %d", errno);
            exit(1);
        }
    }

    cnd_wait(&GO,&sync_lock); // main thread is waiting for the rest to be created
    cnd_broadcast(&laavoda); // signal all other thread to start working
    mtx_unlock(&sync_lock);
}

int main (int argc, char** argv){

    if (!validate_input(argc, argv))
        exit(1);

    root_diretory = argv[1];
    search_term = argv[2];
    requested_threads_num = atoi(argv[3]);
    thrd_t threads[requested_threads_num];

    prepare();
    
    enqueue(mission_queue, root_mission);
    
    
    threads_get_ready_and_go(threads);

    for (int i = 0; i < requested_threads_num; i++) {
        thrd_join(threads[i], NULL);
    }
    
    destory_resources();
    printf("Done searching, found %d files\n",matches_count);
    exit(error_thread);
}