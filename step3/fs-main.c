/***************************************************************************
* main.c
    user shell of the file system
***************************************************************************/
#include "fs.h"

#define SEPARATORS " \n\t"

int main(){

    start_sys();

    char line_buffer[1024];

    printf("%s $ ", cur_dir_name);

    while (fgets(line_buffer, 1024, stdin)){

        char *args[64], **arg;

        // parse the request
        arg = args;
        *arg++ = strtok(line_buffer, SEPARATORS);
        while ((*arg++ = strtok(NULL, SEPARATORS)))
            ;

        if (!strcmp(args[0], "f")){
            Format();
        }
        else if (!strcmp(args[0], "pwd")){
            Pwd();
        }
        else if (!strcmp(args[0], "cd")){
            Chdir(args);
        }
        else if (!strcmp(args[0], "mkdir")){
            Mkdir(args);
        }
        else if (!strcmp(args[0], "rmdir")){
            Rmdir(args);
        }
        else if (!strcmp(args[0], "ls")){
            Ls(args);
        }
        else if (!strcmp(args[0], "mk")){
            Create(args);
        }
        else if (!strcmp(args[0], "rm")){
            Rm(args);
        }
        else if (!strcmp(args[0], "write")){
            Write(args);
        }
        else if (!strcmp(args[0], "cat")){
            Read(args);
        }
        else if (!strcmp(args[0], "del")){
            Del(args);
        }
        else if (!strcmp(args[0], "exit")){
            exit_sys();
            break;
        }
        else if (!strcmp(args[0], "close")){
            my_close(args);
        }
        else{
            printf("request not defined\n");
        }

        //printf("%s $ ", cur_dir_name);
        printf("%s $ ", cur_dir_name);
    }

    return 0;
}