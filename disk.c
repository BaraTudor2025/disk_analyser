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
<<<<<<< HEAD
    char path[128];     // path de analizat (global)
    char filename[20];  // file asociat cu task-ul in care se scrie process_data_t
=======
    char path[128];     // path de analizat
    char filename[32];  // file asociat cu task-ul in care se scrie process_data_t
>>>>>>> cbba71eaf4ebb1f8b6abd6e2aaa2e1b0c425bf86
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
<<<<<<< HEAD
    int files; // nr de fisiere
    int dirs; // nr de foldere
    int message_len;
=======
    int files;
    int dirs;
    int line_num; // numarul de linii pe care il contine mesajul afisat prin --print <id>
>>>>>>> cbba71eaf4ebb1f8b6abd6e2aaa2e1b0c425bf86
} process_data_t;

int search_folder(
    const char* global_path, 
    const char* local_path, 
    const char* dirname, 
    long long prev_folder_size, 
    struct process_data_s* process_data,
    int is_root_folder
);

// populeaza s_proc_ids
void read_proc_info_list(){
    mkdir("~/.da_cache_d", S_IRUSR | S_IWUSR); // nu-l face de doua ori, daca exista atunci ret==-1
    CHECK(s_fd = open(PROC_LIST_FILENAME, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR));
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
    printf("Nu exista task-ul cu ID '%d'\n", id);
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
        close(fd);
        kill(info->proc_id, SIGCONT);
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
    read_proc_info_list();
    process_info_t* info = find_process_info(id);
    process_data_t data;
    int fd = open(info->filename, O_RDONLY);
    CHECK(read(fd, &data, sizeof(data)));

    switch(data.status)
    {
    case STATUS_PENDING:
    case STATUS_PROGRESS:
        kill(info->proc_id, SIGTERM);
    case STATUS_DONE:
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
    }
    printf("\n");
}

void proc_info(int id){
	read_proc_info_list();
	process_info_t* info = find_process_info(id);

	process_data_t data;
    int fd = read_proc_data_lock(info->filename, &data);

    if (data.status == 0 )
    {
    	printf("Statusul analizei este pending\n");
    }
    if (data.status == 1 )
    {
    	printf("Statusul analizei  este progress\n");
    }
    if (data.status == 2 )
    {
    	printf("Statusul analizei este done\n");
    }
    close_and_unlock(fd);
}

void proc_print(int id){
<<<<<<< HEAD
    struct process_data_s data;
    search_folder("test","test","",0,&data,1);
}

void proc_list(){

    read_proc_list();
=======
    read_proc_info_list();
    process_info_t* info = find_process_info(id);

    process_data_t data;
    int fd = read_proc_data_lock(info->filename, &data);
    if(data.status == STATUS_DONE){
        char buff[1024];
        size_t len = 1024;
        FILE* fp = fdopen(fd, "r");
        for(int i = 0; i < data.line_num; i++){
            //CHECK(getdelim(&buff, &len, '\n', fp));
            //fgets(buff, sizeof(buff));
            if(getline(&buff, &len, fp) > 0)
                printf("%s", buff);
        }
    }
    close_and_unlock(fd);
}

void proc_list(){
    read_proc_info_list();
>>>>>>> cbba71eaf4ebb1f8b6abd6e2aaa2e1b0c425bf86
    for(int i=0; i<s_proc_num; i++){
        process_info_t* info = &s_proc_list[i];
        process_data_t data;
    	int fd = read_proc_data_lock(info->filename, &data);
        printf("id=%d path=%s progress=%d percent %d files %d dirs\n", info->proc_id, info->path, data.progress, data.files, data.dirs);
        close_and_unlock (fd);
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
int search_folder(const char* global_path, 
    const char* local_path, 
    const char* dirname, 
    long long prev_folder_size, 
    struct process_data_s* process_data,
    int is_root_folder
){

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
            // if(!strcmp(global_path,local_path)){
            if(is_root_folder){
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
            search_folder(global_path, next_folder,dent->d_name,dir_stat.st_size,process_data,0);
        }
    }
    closedir(srcdir);
}

void check_file(const char* path){
    struct stat stats;
    if(!stat(path, &stats))
        printf("The size of the folder is: %ld \n", stats.st_size);
}


