#include "disk.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const char* s_help_options =
"Accepta o singura comanda, cu exceptia lui --priority\n"
"Usage: da [OPTION]... [DIR]...\n"
"Analyze the space occupied by the directory at [DIR]\n"
"-a, --add          analyze a new directory path for disk usage\n"
"-p, --priority     set priority for the new analysis (works only with -a argument)\n"
"-S, --suspend <id> suspend task with <id>\n"
"-R, --resume <id>  resume task with <id>\n"
"-r, --remove <id>  remove the analysis with the given <id>\n"
"-i, --info <id>    print status about the analysis with <id> (pending, progress, done)\n"
"-l, --list         list all analysis tasks, with their ID and the corresponding root path\n"
"-p, --print <id>   print analysis report for those tasks that are \"done\"\n";

/* enum command { */
/*     C_ADD, C_SUSPEND, C_RESUME, C_REMOVE, C_INFO, C_LIST, C_PRINT */
/* }; */
/*  */
/* typedef struct command_s { */
/*     enum command command; */
/*     const char* path; */
/*     int id; */
/*     int priority; */
/* }command_t; */

int main(int argc, char* argv[]) {

#define GET_INT_ARG  atoi(argv[argpos+1])

#define IS_COMMAND(short, long) \
    ((str_command=long,0) || strcmp(argv[argpos], short ) == 0 || strcmp(argv[argpos], long) == 0)

#define ASSERT_HAS_ARG(index) \
    if((argc - 1) < index){ \
        printf("comanda %s are nevoie de un argument\n", str_command); \
        exit(-1); \
    }

    int command_arg_pos = 2;

    const int default_priority = 2;
    int argpos = 1;
    const char* str_command = ""; // folosit de macrouri
    if(argc == 1 || IS_COMMAND("-h", "--help")){
        printf("%s", s_help_options);
        exit(0);
    } else if(IS_COMMAND("-a", "--add")) {
        ASSERT_HAS_ARG(command_arg_pos);
        argpos++;
        const char* path = argv[argpos];
        int priority = 2;
        if((argc - 1) >= 3){
            argpos++;
            if(IS_COMMAND("-p", "--priority")){
                ASSERT_HAS_ARG(4);
                priority = GET_INT_ARG;
                if(priority < 1 || priority > 3){
                    printf("prioritate invalida: %d, trebuie sa fie 1, 2 sau 3\n", priority);
                    exit(EXIT_FAILURE);
                }
            } else {
                printf("command %s nu poate fi executata, abort...\n", argv[argpos]);
            }
        }
        proc_add(path, priority);

    } else if(IS_COMMAND("-S", "--suspend")) {
        ASSERT_HAS_ARG(command_arg_pos);
        proc_suspend(GET_INT_ARG);

    } else if(IS_COMMAND("-R", "--resume")) {
        ASSERT_HAS_ARG(command_arg_pos);
        proc_resume(GET_INT_ARG);

    } else if(IS_COMMAND("-r", "--remove")) {
        ASSERT_HAS_ARG(command_arg_pos);
        proc_remove(GET_INT_ARG);

    } else if(IS_COMMAND("-i", "--info")) {
        ASSERT_HAS_ARG(command_arg_pos);
        proc_info(GET_INT_ARG);

    } else if(IS_COMMAND("-p", "--print")) {
        ASSERT_HAS_ARG(command_arg_pos);
        proc_print(GET_INT_ARG);

    } else if(IS_COMMAND("-l", "--list")) {
        proc_list();

    } else if(IS_COMMAND("-p", "--priority")) {
        printf("--priority merge doar cu --add ");
    } else {
        printf("comanda: %s nu exista, --help pentru comenzi\n", argv[argpos]);
    }
}
