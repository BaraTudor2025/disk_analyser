#include "disk.h"
#include <sys/types.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>

//#define CHECK(...) if((__VA_ARGS__) == -1) { perror(#__VA_ARGS__); exit(-1); }
#define CHECK(...) if((__VA_ARGS__) == -1) report_and_exit(#__VA_ARGS__);

void report_and_exit(const char* msg){
    perror(msg);
    exit(-1);
}

typedef struct process_id_s {
    int proc_id;        // id-ul din sistem
    char path[128];     // path de analizat
    char filename[20];  // file asociat cu task-ul in care se scrie process_info_t
} process_id_t;

static const char* PROC_LIST_FILENAME = ".proc_list";
//static const char* PROC_LIST_FILENAME = "/tmp/da_proc_list";
static int s_fd; // fd pentru .proc_list
static int s_proc_num; // number of processes
static process_id_t s_proc_ids[100]; // lista de procese, maxim 100

enum status {
    STATUS_PENDING,
    STATUS_PROGRESS,
    STATUS_DONE
};

typedef struct process_info_s {
    process_id_t ids;
    int priority; // 1 la 3
    int progress; //  0 la 100
    enum status status; // pending | in progress | done
    int files;
    int dirs;
    int message_len;
} process_info_t;

void check_file(const char*);
int search_folder(const char* global_path, const char* local_path, const char* dirname, long long prev_folder_size);

// populeaza s_proc_ids
void read_proc_list(){
    if(s_fd != 0)
        return;
    CHECK(s_fd = open(PROC_LIST_FILENAME, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR));
    int ret;
    CHECK(ret = read(s_fd, &s_proc_num, sizeof(s_proc_num)));
    if(ret == 0){
        s_proc_num = 0;
    }
    else {
        for(int i = 0; i < s_proc_num; i++){
            CHECK(read(s_fd, &s_proc_ids[i], sizeof (process_id_t)));
        }
    }
}

void write_proc_list(){
    if(s_proc_num == 0)
        return;
    lseek(s_fd, 0, SEEK_SET);
    CHECK(write(s_fd, &s_proc_num, sizeof(s_proc_num)));
    for(int i = 0; i < s_proc_num; i++){
        CHECK(write(s_fd, &s_proc_ids[i], sizeof (process_id_t)));
    }
}

process_id_t* find_process_id(int id){
    for(int i = 0; i < s_proc_num; i++)
        if(s_proc_ids[i].proc_id == id)
            return &s_proc_ids[i];
    printf("Nu exista task-ul cu ID '%d'\n", id);
    exit(-1);
}

void set_task_filename(char* filename, int id){
    strcpy(filename, ".task");
    sprintf(filename+5, "%d", id);
}


// flock folosit pentru sincronizarea dintre procesul task si procesul main care interogheaza
int read_proc_info_lock(char* filename, process_info_t* info){
    int fd;
    CHECK(fd = open(filename, O_RDONLY));
    CHECK(flock(fd, LOCK_SH));
    CHECK(read(fd, info, sizeof(process_info_t)));
    return fd;
}

void close_and_unlock(int fd){
    CHECK(flock(fd, LOCK_UN));
    CHECK(close(fd));
}

void read_proc_info(char* filename, process_info_t* info){
    int fd = read_proc_info_lock(filename, info);
    close_and_unlock(fd);
}

void write_proc_info(char* filename, const process_info_t* info){
    int fd;
    CHECK(fd = open(filename, O_WRONLY));
    CHECK(flock(fd, LOCK_EX));
    CHECK(write(fd, info, sizeof(process_info_t)));
    close_and_unlock(fd);
}

    /* struct flock lock; */
    /* lock.l_type = F_RDLCK; */
    /* //lock.l_type = F_WRLCK; */
    /* lock.l_whence = SEEK_SET; */
    /* lock.l_start = 0; */
    /* lock.l_len = 0; */
    /* lock.pid = getpid(); */
    /* //CHECK(fcntl(fd, F_SETLKW, &lock)); */
    /* CHECK(fcntl(fd, F_SETLKW, &lock)); */
    /*  */
    /* CHECK(read(fd, info, sizeof(process_info_t))); */
    /*  */
    /* lock.l_type = F_UNLCK; */
    /* CHECK(fcntl(fd, F_SETLK, &lock)); */

void proc_add(const char* path, int priority){
    // TODO verifica daca path-ul este valid

    read_proc_list();

    // mai intai vedem daca exista un task pentru acest path
    for(int i = 0; i < s_proc_num; i++){
        // TODO vezi daca este inclus in alt directory, un fel de 'bool is_prefix()'
        if(strcmp(s_proc_ids[i].path, path) == 0){
            // exista task
            printf("Directory '%s' is already included in analysis with ID '%d'\n", path, s_proc_ids[i].proc_id);
            return;
        }
    }

    pid_t pid = fork();
    if(pid == 0) { // daemon
        process_info_t proc_info;
        memset(&proc_info, 0, sizeof(process_info_t));
        proc_info.ids.proc_id = getpid();
        set_task_filename(proc_info.ids.filename, getpid());
        strcpy(proc_info.ids.path, path);
        proc_info.priority = priority;
        proc_info.progress = STATUS_PROGRESS;
        proc_info.status = 0;
        proc_info.files = 0;
        proc_info.dirs = 0;
        setpriority(PRIO_PROCESS, proc_info.ids.proc_id, proc_info.priority);
        int fd;
        CHECK(fd = creat(proc_info.ids.filename, S_IRUSR | S_IWUSR));
        CHECK(close(fd));
        write_proc_info(proc_info.ids.filename, &proc_info);
        // TODO apel functie de analiza
        //funcita_alaliza(fd, filename,  proc_info);
    }
    else { // parent/main
        s_proc_ids[s_proc_num].proc_id = pid;
        strcpy(s_proc_ids[s_proc_num].path, path);
        set_task_filename(s_proc_ids[s_proc_num].filename, pid);
        s_proc_num++;
        write_proc_list();
    }
}

void proc_suspend(int id){
    read_proc_list();
    process_id_t* ids = find_process_id(id);

    process_info_t info;
    int fd = read_proc_info_lock(ids->filename, &info);

    switch(info.status)
    {
    case STATUS_PENDING:
        printf("Task already suspended for '%s'\n", ids->path);
        break;
    case STATUS_PROGRESS:
        CHECK(kill(ids->proc_id, SIGSTOP));
        printf("Suspending task for '%s'\n", ids->path);
        info.status = STATUS_PENDING;
        CHECK(write(fd, &info, sizeof(info)));
        break;
    case STATUS_DONE:
        printf("Task is done no need to suspend for '%s'", ids->path);
        break;
    }
    close_and_unlock(fd);
}

void proc_resume(int id){
    read_proc_list();
    process_id_t* ids = find_process_id(id);
    process_info_t info;
    int fd = open(ids->filename, O_RDONLY);
    CHECK(read(fd, &info, sizeof(info)));

    switch(info.status)
    {
    case STATUS_PENDING:
        info.status = STATUS_PROGRESS;
        CHECK(write(fd, &info, sizeof(info)));
        close(fd);
        kill(ids->proc_id, SIGCONT);
        break;
    case STATUS_PROGRESS:
        printf("\n");
        break;
    case STATUS_DONE:
        printf("\n");
        break;
    }
}

void proc_remove(int id){
    read_proc_list();
    process_id_t* ids = find_process_id(id);

    process_info_t info;
    int fd = open(ids->filename, O_RDONLY);
    CHECK(read(fd, &info, sizeof(info)));

    switch(info.status)
    {
    case STATUS_PENDING:
    case STATUS_PROGRESS:
        kill(ids->proc_id, SIGTERM);
    case STATUS_DONE:
        unlink(ids->filename);
        // TODO remove din array s_proc_ids/list
        //for(int i = 0; )
        write_proc_list();
    }
    printf("\n");
}

void proc_info(int id){
}

void proc_print(int id){
}

void proc_list(){
    //check_file("../disk_analyser");
}

/// Cauta recursiv pornind de la 'global_path'
int search_folder(const char* global_path, const char* local_path, const char* dirname, long long prev_folder_size){

    struct dirent *dent;
    int dir_count = 0;
    DIR* srcdir = opendir(local_path);
    if(!srcdir){
        perror("opendir");
        return -1;
    }
    while((dent = readdir(srcdir)) != 0){
        struct stat dir_stat;

        if(dent->d_name[0] == '.')
            continue;

        // Folosim 'fstatat' in loc de 'stat' deoarece avem de a face cu folder
        if(fstatat(dirfd(srcdir), dent->d_name, &dir_stat,0) < 0){
            perror(dent->d_name);
            continue;
        }
        char relative_path[256];
        strcpy(relative_path, dirname);
        strcat(relative_path,"/");
        strcat(relative_path,dent->d_name);
        if(S_ISDIR(dir_stat.st_mode)){
            if(!strcmp(global_path,local_path)){
                printf("Path    Usage   Size    Amount\n");
                printf("%s 100%% %ld\n",global_path,dir_stat.st_size);
                printf("|\n");
            }
            else{
                long int current_percentage = (dir_stat.st_size/prev_folder_size)*100;
                printf("|-/%s %ld%% %ld \n",relative_path, current_percentage ,dir_stat.st_size);
            }
            dir_count++;
            char next_folder[128];
            strcpy(next_folder,local_path);
            strcat(next_folder,"/");
            strcat(next_folder,dent->d_name);
            search_folder(global_path, next_folder,dent->d_name,dir_stat.st_size);
        }
    }
    closedir(srcdir);
}

void check_file(const char* path){
    struct stat stats;
    if(!stat(path, &stats))
        printf("The size of the folder is: %ld \n", stats.st_size);
}


