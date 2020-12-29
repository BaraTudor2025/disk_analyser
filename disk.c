#include "disk.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>

#define CHECK(...) if((__VA_ARGS__) == -1) { perror(#__VA_ARGS__); exit(-1); }

typedef struct process_id_s {
    int local_id; // id-ul prin care ne referim la task din comanda ./da
    int proc_id; // id-ul din sistem
    char path[128];
} process_id_t;

static const char* PROC_LIST_FILENAME = ".proc_list";
static int s_fd; // fd pentru .proc_list
static int s_proc_num; // number of processes
static process_id_t s_proc_ids[100]; // lista de procese, maxim 100

typedef struct process_info_s {
    process_id_t ids;
    int priority; // 1 la 3
    int progress; //  0 la 100
    int status; // preparing | in progress | done
    //char status[20]; // preparing | in progress | done
    int files;
    int dirs;
} process_info_t;

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

/*
 *
 *
 *
 */

// TODO cu flock
void read_proc_info(){
}

void write_proc_info(){
}

void proc_add(const char* path, int priority){
    // TODO verifica daca path-ul este valid

    read_proc_list();

    // mai intai vedem daca exista un task pentru acest path
    for(int i = 0; i < s_proc_num; i++){
        if(strcmp(s_proc_ids[i].path, path) == 0){
            // exista task
            printf("Directory '%s' is already included in analysis with ID '%d'\n", path, s_proc_ids[i].local_id);
            return;
        }
    }

    pid_t pid = fork();
    if(pid == 0) { // daemon
        int fd;
        char filename[10] = ".task";
        sprintf(filename+5, "%d", s_proc_num);
        CHECK(fd = open(filename, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR));

        process_info_t proc_info;
        memset(&proc_info, 0, sizeof(process_info_t));
        proc_info.ids.local_id = s_proc_num;
        proc_info.ids.proc_id = getpid();
        strcpy(proc_info.ids.path, path);
        proc_info.priority = priority;
        proc_info.progress = 0;
        proc_info.status = 0;
        proc_info.files = 0;
        proc_info.dirs = 0;
        // TODO apel functie de analiza
        //funcita_alaliza(fd, filename,  proc_info);
    }
    else { // parent/main
        s_proc_ids[s_proc_num].local_id = s_proc_num;
        s_proc_ids[s_proc_num].proc_id = pid;
        strcpy(s_proc_ids[s_proc_num].path, path);
        s_proc_num++;
        write_proc_list();
    }
}

void proc_suspend(int id){
    read_proc_list();
    for(int i = 0; i < s_proc_num; i++){
        printf("local:%d, proc:%d, path:%s\n", s_proc_ids[i].local_id, s_proc_ids[i].proc_id, s_proc_ids[i].path);
    }
}

void proc_resume(int id){
}

void proc_remove(int id){
}

void proc_info(int id){
}

void proc_print(int id){
}

int search_folder(const char* global_path, const char* local_path, const char* dirname, long long prev_folder_size);
void proc_list(){
    //check_file("../disk_analyser");
    search_folder("../disk_analyser","../disk_analyser", "",0);
}

//struct dirent dent;

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




