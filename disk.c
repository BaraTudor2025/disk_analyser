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
#include <stdarg.h>

#define CHECK(...) if((__VA_ARGS__) < 0) report_and_exit(#__VA_ARGS__);

void report_and_exit(const char* msg){
    perror(msg);
    exit(-1);
}

typedef struct process_info_s {
    int proc_id;        // id-ul din sistem
    char path[128];     // path de analizat
    char filename[32];  // file asociat cu task-ul in care se scrie process_data_t
} process_info_t;


// in 2 locuti mai trebuie updatat daca se schimba dir-ul: read_proc_list si set_task_filenam
static const char* PROC_LIST_FILENAME = "~/.da_cache_d/.proc_list";
//static const char* PROC_LIST_FILENAME = "/tmp/da_proc_list";
static int s_fd; // fd pentru PROC_LIST_FILENAME
static int s_proc_num; // number of processes
static process_info_t s_proc_list[100]; // lista de procese, maxim 100

enum status {
    STATUS_PENDING,
    STATUS_PROGRESS,
    STATUS_DONE
};

typedef struct process_data_s {
    process_info_t info; // path-ul pe care in analizeaza este constant
    int priority; // 1 la 3
    int progress; //  0 la 100
    enum status status; // pending | in progress | done
    int files;
    int dirs;
    int line_num; // numarul de linii pe care il contine mesajul afisat prin --print <id>
} process_data_t;

int search_folder(
    const char* local_path,
    const char* dirname,
    long long prev_folder_size,
    struct process_data_s* process_data,
    int is_root_folder
);

char* str_status(enum status status){
    switch(status){
    case STATUS_DONE:
        return "done";
    case STATUS_PENDING:
        return "is pending";
    case STATUS_PROGRESS:
        return "in progress";
    }
}

char* str_priority(int prio){
    switch(prio){
    case 0:
    case 1:
        return "low";
    case 2:
        return "normal";
    default: // prio >= 3
        return "high";
    }
}

// populeaza s_proc_ids
void read_proc_info_list(){
    char path[PATH_MAX];
    realpath("~/.da_cache_d", path);
    mkdir(path, 700); // nu-l face de doua ori, daca exista atunci ret==-1

    realpath(PROC_LIST_FILENAME, path);
    CHECK(s_fd = open(path, O_RDWR | O_CREAT, 700));
    int read_bytes;
    CHECK(read_bytes = read(s_fd, &s_proc_num, sizeof(s_proc_num)));
    if(read_bytes == 0){
        s_proc_num = 0;
    }
    else {
        for(int i = 0; i < s_proc_num; i++){
            CHECK(read(s_fd, &s_proc_list[i], sizeof (process_info_t)));
        }
    }
}

void write_proc_info_list(){
    if(s_proc_num == 0)
        return;
    lseek(s_fd, 0, SEEK_SET);
    CHECK(write(s_fd, &s_proc_num, sizeof(s_proc_num)));
    for(int i = 0; i < s_proc_num; i++){
        CHECK(write(s_fd, &s_proc_list[i], sizeof (process_info_t)));
    }
}

process_info_t* find_process_info(int id){
    for(int i = 0; i < s_proc_num; i++)
        if(s_proc_list[i].proc_id == id)
            return &s_proc_list[i];
    printf("No existing analysis for task ID '%d'\n", id);
    exit(-1);
}

void set_task_filename(char* filename, int id){
    const char* prefix = "~/.da_cache_d/.task";
    strcpy(filename, prefix);
    sprintf(filename + strlen(prefix), "%d", id);
}

// flock folosit pentru sincronizarea dintre procesul task si procesul main care interogheaza
int read_proc_data_lock(char* filename, process_data_t* data){
    int fd;
    CHECK(fd = open(filename, O_RDONLY));
    CHECK(flock(fd, LOCK_SH));
    CHECK(read(fd, data, sizeof(process_data_t)));
    return fd;
}

void close_and_unlock(int fd){
    CHECK(flock(fd, LOCK_UN));
    CHECK(close(fd));
}

void read_proc_data(char* filename, process_data_t* data){
    int fd = read_proc_data_lock(filename, data);
    close_and_unlock(fd);
}

void write_proc_data(char* filename, const process_data_t* data){
    int fd;
    CHECK(fd = open(filename, O_WRONLY));
    CHECK(flock(fd, LOCK_EX));
    CHECK(write(fd, data, sizeof(process_data_t)));
    close_and_unlock(fd);
}

void proc_add(const char* p, int priority){

    //deoarece malloc aloca memorie nu trebuie dat free, programul da exit rapid
    char *path=realpath(p, NULL);
    if (path == NULL)
    {
    	printf("Nu exista fisierul\n");
    	return;
    }
    read_proc_info_list();

    // mai intai vedem daca exista un task pentru acest path
    for(int i = 0; i < s_proc_num; i++) {
        if(strstr(path, s_proc_list[i].path) != NULL) {
            // exista task
            printf("Directory '%s' is already included in analysis with ID '%d'\n", path, s_proc_list[i].proc_id);
            return;
        }
    }

    pid_t pid = fork();
    if(pid == 0) { // daemon
        process_data_t proc_data;
        memset(&proc_data, 0, sizeof(process_data_t));
        proc_data.info.proc_id = getpid();
        set_task_filename(proc_data.info.filename, getpid());
        strcpy(proc_data.info.path, path);
        proc_data.priority = priority;
        proc_data.progress = STATUS_PROGRESS;
        proc_data.status = 0;
        proc_data.files = 0;
        proc_data.dirs = 0;
        setpriority(PRIO_PROCESS, proc_data.info.proc_id, proc_data.priority);
        int fd;
        CHECK(fd = creat(proc_data.info.filename, S_IRUSR | S_IWUSR));
        CHECK(close(fd));
        printf("Created analysis task with ID '%d', for '%s' and priority %s", proc_data.info.proc_id, proc_data.info.path, str_priority(proc_data.priority));
        write_proc_data(proc_data.info.filename, &proc_data);
        // TODO apel functie de analiza
        //search_folder()
    }

    else { // parent/main
        s_proc_list[s_proc_num].proc_id = pid;
        strcpy(s_proc_list[s_proc_num].path, path);
        set_task_filename(s_proc_list[s_proc_num].filename, pid);
        s_proc_num++;
        write_proc_info_list();
    }
}

void proc_suspend(int id){
    read_proc_info_list();
    process_info_t* info = find_process_info(id);
    process_data_t data;
    int fd = read_proc_data_lock(info->filename, &data);

    switch(data.status)
    {
    case STATUS_PENDING:
        printf("Task already suspended for '%s'\n", info->path);
        break;

    case STATUS_PROGRESS:
        CHECK(kill(info->proc_id, SIGSTOP));
        printf("Suspending task for '%s'\n", info->path);
        data.status = STATUS_PENDING;
        CHECK(write(fd, &data, sizeof(data)));
        break;
    case STATUS_DONE:
        printf("Task is done no need to suspend for '%s'", info->path);
        break;
    }
    close_and_unlock(fd);
}

void proc_resume(int id){
    read_proc_info_list();
    process_info_t* info = find_process_info(id);
    process_data_t data;
    int fd = open(info->filename, O_RDONLY);
    CHECK(read(fd, &data, sizeof(data)));

    switch(data.status)
    {
    case STATUS_PENDING:
        data.status = STATUS_PROGRESS;
        CHECK(write(fd, &data, sizeof(data)));
        CHECK(close(fd));
        CHECK(kill(info->proc_id, SIGCONT));
        printf("Suspended task with ID '%d' for '%s'\n", info->proc_id, info->path);
        break;
    case STATUS_PROGRESS:
        printf("Task for '%s' is already in progress\n", info->path);
        break;
    case STATUS_DONE:
        printf("Task for '%s' is already done\n", info->path);
        break;
    }
}

void proc_remove(int id){
    read_proc_info_list();
    process_info_t* info = find_process_info(id);
    process_data_t data;
    int fd = read_proc_data_lock(info->filename, &data);
    /* int fd = open(info->filename, O_RDONLY); */
    /* CHECK(read(fd, &data, sizeof(data))); */

    switch(data.status)
    {
    case STATUS_PENDING:
    case STATUS_PROGRESS:
        kill(info->proc_id, SIGTERM);
    case STATUS_DONE:
        close_and_unlock(fd);
        unlink(info->filename);
        int index = 0;
        for(int i = 0; i < s_proc_num; i++){
            if(info->proc_id == id){
                index = i;
                break;
            }
        }

        for(int i = index; i < s_proc_num - 1; i++){
            s_proc_list[i].proc_id = s_proc_list[i++].proc_id;
            strcpy(s_proc_list[i].path, s_proc_list[i++].path);
            strcpy(s_proc_list[i].filename, s_proc_list[i++].filename);
        }
        s_proc_num -= 1;
        write_proc_info_list();
        printf("Removed analysis task with ID '%d', status '%s' for '%s'\n", data.info.proc_id, str_status(data.status), data.info.path);
    }
}

void proc_info(int id){
	read_proc_info_list();
	process_info_t* info = find_process_info(id);
	process_data_t data;
    read_proc_data(info->filename, &data);
    printf("Status '%s' for '%s'\n", str_status(data.status), data.info.path);
}

long long count_path_size(const char* local_path){
    struct dirent *dent;
    DIR* srcdir = opendir(local_path);
    if(!srcdir){
        perror("opendir");
        return -1;
    }
    int this_dir_size = 0;
    // char* next_folder = malloc(256);
    while((dent = readdir(srcdir)) != 0){
        // memset(next_folder,256,0);
        //printf("%s\n",dent->d_name);
        struct stat dir_stat;
        if((strcmp(dent->d_name,".") == 0) || (strcmp(dent->d_name, "..") == 0))
            continue;

        // Folosim 'fstatat' in loc de 'stat' deoarece avem de a face cu folder
        if(fstatat(dirfd(srcdir), dent->d_name, &dir_stat,0) < 0){
            perror(dent->d_name);
            continue;
        }

        if(S_ISDIR(dir_stat.st_mode)){
            char next_folder[256];
            strcpy(next_folder,local_path);
            strcat(next_folder,"/");
            strcat(next_folder,dent->d_name);
            this_dir_size += count_path_size(next_folder);
        }
        else{
            this_dir_size += dir_stat.st_size;
        }
    }
    // free(next_folder);
    closedir(srcdir);
    return this_dir_size;
}

// Afiseaza headerul procesului si returneaza dimensiunea folderului principal
long long print_search_folder_header(const char* path){
    struct dirent *dent;
    DIR* srcdir = opendir(path);
    struct stat global_dir_stat; // info despre primul folder
    char* absolute_path= realpath(path,NULL);
    long long folder_size = count_path_size(absolute_path);
    //afisam prima linie: "Path   Usage   Size   Amount"
    if((dent = readdir(srcdir)) != 0){
        printf("Path    Usage   Size    Amount\n");
        printf("%s 100%% %lld\n",absolute_path,folder_size);
        printf("|\n");
    }
    free(absolute_path);
    closedir(srcdir);
    return folder_size;
}

void proc_print(int id){

    /// DE MUTAT IN ADD
    struct process_data_s data;
    long long global_folder_size;
    global_folder_size = print_search_folder_header("flutter"); // afiseaza header-ul cautarii, trebuie inlocuit
    search_folder("flutter","",global_folder_size,&data,1);

    // read_proc_info_list();
    // process_info_t* info = find_process_info(id);

    //process_data_t data;
    // int fd = read_proc_data_lock(info->filename, &data);
    // if(data.status == STATUS_DONE){
    //     char buff[1024];
    //     size_t len = 1024;
    //     FILE* fp = fdopen(fd, "r");
    //     for(int i = 0; i < data.line_num; i++){
    //         //CHECK(getdelim(&buff, &len, '\n', fp));
    //         //fgets(buff, sizeof(buff));
    //         if(getline(&buff, &len, fp) > 0)
    //             printf("%s", buff);
    //     }
    // }
    // close_and_unlock(fd);
}

void proc_list(){
    read_proc_info_list();
    //printf("%");
    for(int i=0; i<s_proc_num; i++){
        process_info_t* info = &s_proc_list[i];
        process_data_t data;
    	read_proc_data(info->filename, &data);
       
        printf( "%-7s %-3s %-4s %-15s %-s %-30s\n", "ID", "PRI", "Done" , "Status", "Details", "Path");
        for(i=0; i<s_proc_num; i++)
        {
        	printf("%-7d %-3s %-d%% %-10s %-3d files %-2d dirs %-30s\n",info->proc_id, str_priority(data.priority), data.progress,str_status(data.status), data.files, data.dirs, info->path);
        }
    }
}

// pt FLORIAN: folosesti functia asta in loc de printf
// IMPORTANT, cand o apelezi sa scrii un rand intreg, adica sa aiba fmt-ul un '\n'; desi din cate vad deja faci asta
void write_proc_data_message(process_data_t* data, char* fmt, ...){
    va_list args;
    va_start(args, fmt);
    int fd = open(data->info.filename, O_WRONLY | O_APPEND);
    CHECK(flock(fd, LOCK_EX));
    FILE* fp = fdopen(fd, "a");
    vfprintf(fp, fmt, args);
    CHECK(flock(fd, LOCK_UN));
    CHECK(fclose(fp));
    //CHECK(close(fd));
    va_end(args);
    data->line_num++;
}


/// Cauta recursiv pornind de la 'global_path'
int search_folder(const char* local_path,
    const char* dirname,
    long long global_folder_size,
    struct process_data_s *process_data,
    int is_root_folder
){

    struct dirent *dent;
    DIR* srcdir = opendir(local_path);
    if(!srcdir){
        perror("opendir");
        return -1;
    }
    int total_file_size = 0;
    char* relative_path = malloc(256);
    char* next_folder = malloc(256);
    while((dent = readdir(srcdir)) != 0){
        memset(relative_path,256,0);
        memset(next_folder,256,0);
        struct stat dir_stat;

        if((strcmp(dent->d_name,".") == 0) || (strcmp(dent->d_name, "..") == 0))
            continue;

        // Folosim 'fstatat' in loc de 'stat' deoarece avem de a face cu folder
        if(fstatat(dirfd(srcdir), dent->d_name, &dir_stat,0) < 0){
            perror(dent->d_name);
            continue;
        }

        strcpy(relative_path, local_path);
        strcat(relative_path,"/");
        strcat(relative_path,dent->d_name);
        if(S_ISDIR(dir_stat.st_mode)){
            strcpy(next_folder,local_path);
            strcat(next_folder,"/");
            strcat(next_folder,dent->d_name);
            int this_folder_size = search_folder(next_folder,dent->d_name,global_folder_size,process_data,0);
            process_data->dirs++;
            float current_percentage = ((float)this_folder_size/(float)global_folder_size)*100;
            //write_proc_data_message();
            // printf("|-/%s %.2f%% %d \n",relative_path,current_percentage ,this_folder_size);
            write_proc_data_message(process_data,"|-/%s %.2f%% %d \n",relative_path,current_percentage ,this_folder_size);
        }
        else{
            total_file_size += dir_stat.st_size;
        //incrementam numarul de fisiere
            // long int current_percentage = (dir_stat.st_size/prev_folder_size)*100;
            // printf("FILE : |-/%s %lld%% %ld \n",relative_path, current_percentage ,dir_stat.st_size);
            process_data->progress += dir_stat.st_size;
            process_data->files++;
        }
    }
    free(relative_path);
    free(next_folder);
    closedir(srcdir);
    return total_file_size;
}

void check_file(const char* path){
    struct stat stats;
    if(!stat(path, &stats))
        printf("The size of the folder is: %ld \n", stats.st_size);
}


