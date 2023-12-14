/***************************************************************************
* fs.c
    function implementation of fs.h
***************************************************************************/

#include "fs.h"

int start_sys(void) {
    fs_head = (unsigned char *)malloc(DISK_SIZE);
    memset(fs_head, 0, DISK_SIZE);
    FILE *fp = NULL;

    // not first time start
    if ((fp = fopen(SYS_PATH, "r")) != NULL) {
        fread(fs_head, DISK_SIZE, 1, fp);
        fclose(fp);
    }
    // first time using, needs format
    else {
        printf("System initializing...\n");
        Format();
        printf("Initialized successfully\n");
    }

    // initialize openfile_list
    // openfile_list[0] is root
    fcb_cpy(&openfile_list[0].open_fcb, ((fcb *)(fs_head + 5 * BLOCK_SIZE)));
    strcpy(openfile_list[0].dir_name, ROOT);
    openfile_list[0].pos = 0;
    openfile_list[0].dirty_bit = 0;
    openfile_list[0].valid = 1;
    cur_dir_idx = 0;
    // other files
    fcb *empty =  (fcb *)malloc(sizeof(fcb));
    set_fcb(empty, "\0", "\0", 0, 0, 0, 0);
    for (int i = 1; i < MAX_OPENFILE; ++i) {
        fcb_cpy(&openfile_list[i].open_fcb, empty);
        openfile_list[i].pos = 0;
        openfile_list[i].dirty_bit = 0;
        openfile_list[i].valid = 0;
    }

    // initialize global variables
    strcpy(cur_dir_name, openfile_list[cur_dir_idx].dir_name);
    start = ((boot_block *)fs_head)->start_block;
    free(empty);

    return 0;
}

int Format(void) {
    // initialize boot block
    unsigned char *ptr = fs_head;
    boot_block *init_block = (boot_block *)ptr;
    strcpy(init_block->information,
            "Disk Size = 1MB, Block Size = 256B\n");
    init_block->root = 5;
    init_block->start_block = (unsigned char *)(init_block + BLOCK_SIZE * 7);

    // initialize FAT0/1
    ptr += BLOCK_SIZE;
    set_free(0, 0, 2);

    // allocate block0 - block4 for boot block and fat 0/1
    set_free(get_free(1), 1, 0);
    set_free(get_free(2), 2, 0);
    set_free(get_free(2), 2, 0);

    // set fcb for root directory
    ptr += BLOCK_SIZE * 4;
    fcb *root = (fcb *)ptr;
    int first = get_free(ROOT_BLOCK_NUM);
    set_free(first, ROOT_BLOCK_NUM, 0);
    set_fcb(root, ".", "di", 0, first, BLOCK_SIZE * 2, 1);
    root++;
    set_fcb(root, "..", "di", 0, first, BLOCK_SIZE * 2, 1);
    root++;
    for (int i = 2; i < BLOCK_SIZE * 2 / sizeof(fcb); ++i, ++root) {
        root->valid = 0;
    }

    // write back current system state to fsFile
    FILE *fp = fopen(SYS_PATH, "w");
    fwrite(fs_head, DISK_SIZE, 1, fp);
    fclose(fp);

    printf("Format Done.\n");

    return 0;
}

int Chdir(char **args) {

    // check arguments
    char abspath[PATH_LENGTH];
    memset(abspath, 0, PATH_LENGTH);
    get_abspath(abspath, args[1]);
    fcb *dir = find_fcb(abspath);
    if (dir == NULL || dir->attr == 1) {
        fprintf(stderr, "cd: no such folder\n");
        return 0;
    }

    // if the target dir is open, then change to it
    for (int i = 0; i < MAX_OPENFILE; ++i) {
        if (openfile_list[i].valid == 0)
            continue;
        if (!strcmp(dir->filename, openfile_list[i].open_fcb.filename) && dir->first_block == openfile_list[i].open_fcb.first_block) {
            cur_dir_idx = i;
            memset(cur_dir_name, 0, sizeof(cur_dir_name));
            strcpy(cur_dir_name, openfile_list[cur_dir_idx].dir_name);
            return 1;
        }
    }

    // if the target dir is not open, then open it first
    int fd;
    if ((fd = do_open(abspath)) > 0){
        cur_dir_idx = fd;
        memset(cur_dir_name, 0, sizeof(cur_dir_name));
        strcpy(cur_dir_name, openfile_list[cur_dir_idx].dir_name);
        return 1;
    }
    
    return 0;
}

int Pwd(void) {
    printf("%s\n", cur_dir_name);
    return 1;
}

int Mkdir(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "mkdir: missing arguments\n");
        return 0;
    }

    char goal_path[PATH_LENGTH], parent_path[PATH_LENGTH], dirname[NAME_LENGTH];

    for (int i = 1; args[i] != NULL; ++i) {

        get_abspath(goal_path, args[i]);
        char *end = strrchr(goal_path, '/');

        // get parent path and directory name to be made
        if (end == goal_path) { // parent is root
            strcpy(parent_path, "/");
            strcpy(dirname, goal_path + 1);
        }
        else {                  // parent is not root
            strncpy(parent_path, goal_path, end - goal_path);
            parent_path[end - goal_path] = 0;
            strcpy(dirname, end + 1);
        }

        // parent directory doesn't exist
        if (find_fcb(parent_path) == NULL) {
            fprintf(stderr, "create: parent folder does not exist\n");
            continue;
        }

        // already exists the target directory
        if (find_fcb(goal_path) != NULL) {
            fprintf(stderr, "create: already exists\n");
            continue;
        }

        in_mkdir(parent_path, dirname);
    }

    return 1;
}

int in_mkdir(const char *parent_path, const char *dirname) {

    int flag = 0;
    int first = find_fcb(parent_path)->first_block;
    int second = get_free(1);
    fcb *dir = (fcb *)(fs_head + BLOCK_SIZE * first);

    // check if there's free fcb
    for (int i = 0; i < BLOCK_SIZE / sizeof(fcb); ++i, ++dir) {
        if (dir->valid == 0) {
            flag = 1;
            break;
        }
    }
    if (!flag) {
        fprintf(stderr, "mkdir: no free fcb");
        return -1;
    }

    // check if there's free space
    if (second == -1) {
        fprintf(stderr, "mkdir: no more space\n");
        return -1;
    }
    set_free(second, 1, 0);

    // set fcb for the new directory
    set_fcb(dir, dirname, "di", 0, second, BLOCK_SIZE, 1);
    init_folder(first, second);
    return 0;
}

int Rmdir(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "rmdir: missing arguments\n");
        return 0;
    }

    for (int i = 1; args[i] != NULL; ++i) {
        if (!strcmp(args[i], ".") || !strcmp(args[i], "..")) {
            fprintf(stderr, "rmdir: '.' or '..' is read only\n");
            return 0;
        }
        if (!strcmp(args[i], "/")) {
            fprintf(stderr, "rmdir: cannot remove root directory\n");
            return 0;
        }

        fcb *dir = find_fcb(args[i]);
        if (dir == NULL) {
            fprintf(stderr, "rmdir: no such folder\n");
            return 0;
        }
        if (dir->attr == 1) {
            fprintf(stderr, "rmdir: not a directory\n");
            return 1;
        }

        // if the directory is open, close it first
        for (int j = 0; j < MAX_OPENFILE; ++j) {
            if (openfile_list[j].valid == 0)
                continue;
            if (!strcmp(dir->filename, openfile_list[j].open_fcb.filename) && dir->first_block == openfile_list[j].open_fcb.first_block) {
                do_close(j);
                in_rmdir(dir);
                return 1;
            }
        }

        in_rmdir(dir);
    }
    return 1;
}

void in_rmdir(fcb *dir) {
    int first = dir->first_block;
    
    dir->valid = 0;

    // set "." free
    dir = (fcb *)(fs_head + BLOCK_SIZE * first);
    dir->valid = 0;
    // set ".." free
    dir++;
    dir->valid = 0;

    // update FAT
    set_free(first, 1, 1);
}

int Ls(char **args) {
    int first = openfile_list[cur_dir_idx].open_fcb.first_block;
    int i, mode = 'n';
    int flag[3];    // to mark if args[i] is a mode argument

    for (i = 0; args[i] != NULL; ++i)
        flag[i] = 0;
    if (i > 3) {
        fprintf(stderr, "ls: too many arguments\n");
        return 0;
    }

    // check if there's -l mode
    flag[0] = 1;
    for (i = 1; args[i] != NULL; ++i) {
        if (args[i][0] == '-') {
            flag[i] = 1;
            if (!strcmp(args[i], "-l")) {
                mode = 'l';
                break;
            }
            else {
                fprintf(stderr, "ls: wrong operand(-l)\n");
                return 0;
            }
        }
    }

    for (i = 1; args[i] != NULL; ++i) {
        if (flag[i] == 0) {
            fcb *dir = find_fcb(args[i]);
            if (dir != NULL && dir->attr == 0) {
                first = dir->first_block;
            }
            else {
                fprintf(stderr, "ls: no such file or directory\n");
                return 0;
            }
            break;
        }
    }

    in_ls(first, mode);
    
    return 1;
}

void in_ls(int first, char mode) {
    int length = BLOCK_SIZE;
    char fullname[NAME_LENGTH], date[16], time[16];
    fcb *root = (fcb *)(fs_head + BLOCK_SIZE * first);
    boot_block *init_block = (boot_block *)fs_head;

    // root occupies 2 blocks, therefore if the target is root we need to double the length
    if (first == init_block->root) {
        length = ROOT_BLOCK_NUM * BLOCK_SIZE;
    }

    if (mode == 'n') {
        for (int i = 0, count = 1; i < length / sizeof(fcb); ++i, ++count, ++root) {
            if (root->valid == 0)
                continue;
            
            if (root->attr == 0) {
                printf("%s", FOLDER_COLOR);
                printf("%s\t", root->filename);
                printf("%s", DEFAULT_COLOR);
            }
            else {
                get_fullname(fullname, root);
                printf("%s\t", fullname);
            }
        }
    }
    else if (mode == 'l') {
        for (int i = 0; i < length / sizeof(fcb); ++i, ++root) {
            if (root->valid == 0)
                continue;
            trans_date(date, root->date);
            trans_time(time, root->time);
            get_fullname(fullname, root);
            printf("%d\t%6d\t%6ld\t%s\t%s\t", root->attr, root->first_block, root->length, date, time);
            if (root->attr == 0) {
                printf("%s", FOLDER_COLOR);
                printf("%s\n", fullname);
                printf("%s", DEFAULT_COLOR);
            }
            else {
                printf("%s\n", fullname);
            }
        }
    }
    printf("\n");
}

int Create(char **args) {
    char path[PATH_LENGTH], parent_path[PATH_LENGTH], filename[NAME_LENGTH];
    char *end = NULL;

    if (args[1] == NULL) {
        fprintf(stderr, "create: missing argument\n");
        return 0;
    }

    memset(parent_path, 0, PATH_LENGTH);
    memset(filename, 0, NAME_LENGTH);

    for (int i = 1; args[i] != NULL; ++i) {
        get_abspath(path, args[i]);
        end = strrchr(path, '/');
        if (end == path) {
            strcpy(parent_path, "/");
            strcpy(filename, path + 1);
        }
        else {
            strncpy(parent_path, path, end - path);
            strcpy(filename, end + 1);
        }

        if (find_fcb(parent_path) == NULL) {
            fprintf(stderr, "create: parent folder does not exist\n");
            continue;
        }
        if (find_fcb(path) != NULL) {
            fprintf(stderr, "create: folder or file already exists\n");
            continue;
        }

        in_create(parent_path, filename);
    }

    return 1;
}

int in_create(const char *parent_path, const char *filename) {
    char fullname[NAME_LENGTH], fname[16], exname[8];
    char *token = NULL;
    int first = get_free(1), flag = 0;
    fcb *dir = (fcb *)(fs_head + BLOCK_SIZE * find_fcb(parent_path)->first_block);

    // check for free fcb
    for (int i = 0; i < BLOCK_SIZE / sizeof(fcb); ++i, ++dir) {
        if (dir->valid == 0) {
            flag = 1;
            break;
        }
    }
    if (!flag) {
        fprintf(stderr, "create: no free fcb\n");
        return -1;
    }

    // check for free disk space
    if (first == -1) {
        fprintf(stderr, "create: No more space\n");
        return -1;
    }
    set_free(first, 1, 0);

    // split the filename and extension name
    memset(fullname, 0, NAME_LENGTH);
    memset(fname, 0, 8);
    memset(exname, 0, 3);
    strcpy(fullname, filename);
    token = strtok(fullname, ".");
    strncpy(fname, token, 8);
    token = strtok(NULL, ".");
    if (token != NULL) {
        strncpy(exname, token, 3);
    }
    else {
        strncpy(exname, "d", 2);
    }

    set_fcb(dir, fname, exname, 1, first, 0, 1);

    return 0;
}

int Rm(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "rm: missing argument\n");
        return 0;
    }

    for (int i = 1; args[i] != NULL; ++i) {
        fcb *file = find_fcb(args[i]);
        if (file == NULL) {
            fprintf(stderr, "rm: no such file\n");
            return 0;
        }

        if (file->attr == 0) {
            fprintf(stderr, "rm: is a directory\n");
            return 0;
        }

        // check if it is open, if open then close it first before removing
        for (int j = 0; j < MAX_OPENFILE; ++j) {
            if (openfile_list[j].valid == 0)
                continue;
            if (!strcmp(file->filename, openfile_list[j].open_fcb.filename) &&
                file->first_block == openfile_list[j].open_fcb.first_block) {
                do_close(j);
                int tmp_first = file->first_block;
                file->valid = 0;
                set_free(tmp_first, 0, 1);
                return 1;
            }
        }

        int tmp_first = file->first_block;
        file->valid = 0;
        set_free(tmp_first, 0, 1);
    }
    
    return 1;
}

int do_open(char *path) {
    int fd = get_user_open();
    fcb *file = find_fcb(path);
    if (fd == -1) {
        fprintf(stderr, "open: cannot open file, no more useropen entry\n");
        return -1;
    }
    fcb_cpy(&openfile_list[fd].open_fcb, file);
    openfile_list[fd].valid = 1;
    openfile_list[fd].pos = 0;
    openfile_list[fd].dirty_bit = 0;
    memset(openfile_list[fd].dir_name, 0, 80);
    strcpy(openfile_list[fd].dir_name, path);

    return fd;
}

void do_close(int fd) {
    if (fd >= MAX_OPENFILE){
        fprintf(stderr, "openfile_list out of boundary\n");
        return;
    }
    // write back
    if (openfile_list[fd].dirty_bit == 1) {
        fcb_cpy(find_fcb(openfile_list[fd].dir_name), &openfile_list[fd].open_fcb);
    }
    openfile_list[fd].valid = 0;
}

int my_close(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "close: missing argument\n");
        return 1;
    }

    // check for "-a"
    if (args[1][0] == '-') {
        if (!strcmp(args[1], "-a")) {
            for (int i = 0; i < MAX_OPENFILE; ++i) {
                if (i == cur_dir_idx)
                    continue;
                openfile_list[i].valid = 0;
            }
            return 1;
        }
        else {
            fprintf(stderr, "close: wrong argument\n");
            return 0;
        }
    }

    for (int i = 1; args[i] != NULL; ++i) {
        fcb *file = find_fcb(args[i]);
        if (file == NULL) {
            fprintf(stderr, "close: no such file or folder\n");
            return 1;
        }

        // if open, then close it; otherwise ignore
        for (int j = 0; j < MAX_OPENFILE; j++) {
            if (openfile_list[j].valid == 0) {
                continue;
            }

            if (!strcmp(file->filename, openfile_list[j].open_fcb.filename) &&
                file->first_block == openfile_list[j].open_fcb.first_block) {
                do_close(j);
            }
        }
    }

    return 1;
}

int Write(char **args) {
    // default mode is cover
    int mode = 'w', flag = 0, i;
    char path[PATH_LENGTH], data_str[WRITE_SIZE];
    int open_flag = 0;

    // check writing mode
    for (i = 1; args[i] != NULL; ++i) {
        if (args[i][0] == '-') {
            if      (!strcmp(args[i], "-w"))    mode = 'w';
            else if (!strcmp(args[i], "-i"))    mode = 'i';
            else if (!strcmp(args[i], "-a"))    mode = 'a';
            else {
                fprintf(stderr, "write: mode a/w/i, add/write/insert\n");
                return 0;
            }
        }
        else {
            flag += 1 << i;
        }
    }
    // no few argument || too many argument || mode > 1
    if ((flag == 0) || (flag > 4) || (i > 3)) {
        fprintf(stderr, "write <-a>/<-w>/<-i> filename\n");
        return 0;
    }

    // check if the file exists or file name is a folder
    fcb *file = NULL;
    strcpy(path, args[flag >> 1]);
    if ((file = find_fcb(path)) == NULL) {
        fprintf(stderr, "write: no such file\n");
        return 0;
    }
    if (file->attr == 0) {
        fprintf(stderr, "write: is a folder\n");
        return 0;
    }

    // check if the file is open, if not then open it first
    for (i = 0; i < MAX_OPENFILE; ++i){
        if (openfile_list[i].valid == 0)
            continue;
        if (!strcmp(file->filename, openfile_list[i].open_fcb.filename) && file->first_block == openfile_list[i].open_fcb.first_block)
            break;
    }
    if (i == MAX_OPENFILE){
        do_open(path);
        open_flag = 1;
    }

    memset(data_str, 0, WRITE_SIZE);
    for (i = 0; i < MAX_OPENFILE; ++i) {
        if (openfile_list[i].valid == 0)
            continue;
        if (!strcmp(file->filename, openfile_list[i].open_fcb.filename) && file->first_block == openfile_list[i].open_fcb.first_block) {
            if (mode == 'i') {
                printf("Please input location: ");
                scanf("%d", &openfile_list[i].pos);
                getchar();
            }

            int j = 0;
            char c;
            // two consecutive \n denotes end writing
            while (1) {
                for (; (data_str[j] = getchar()) != '\n'; ++j);
                j++;
                if ((c = getchar()) == '\n') break;
                else data_str[j++] = c;
            }

            // insert style, don't need the ending \n
            if (mode == 'i') {
                do_write(i, data_str, j - 1, mode);
            }
            // other 2 styles, ending \n needed
            else {
                do_write(i, data_str, j + 1, mode);
            }

            return 1;
        }
    }

    if (open_flag)
        do_close(i);

    return 1;
}

int do_write(int fd, char *content, size_t len, int wstyle) {
    fat *fat0 = (fat *)(fs_head + BLOCK_SIZE);
    fat *fat1 = (fat *)(fs_head + 3 * BLOCK_SIZE);

    // txt - write buffer
    char txt[WRITE_SIZE] = {0};
    int write_pos = openfile_list[fd].pos;
    openfile_list[fd].pos = 0;
    do_read(fd, openfile_list[fd].open_fcb.length, txt);
    openfile_list[fd].pos = write_pos;
    int i = openfile_list[fd].open_fcb.first_block;
    char input[WRITE_SIZE] = {0};
    strncpy(input, content, len);

    if (wstyle == 'w') {
        memset(txt, 0, WRITE_SIZE);
        strncpy(txt, input, len);
    }
    else if (wstyle == 'i') {
        strcat(input, txt + openfile_list[fd].pos);
        strncpy(txt + openfile_list[fd].pos, input, strlen(input));
    }
    else if (wstyle == 'a') {
        strncpy(txt + openfile_list[fd].open_fcb.length, input, len);
    }
    else if (wstyle == 'd'){
        int ite = openfile_list[fd].pos;
        txt[ite] = '\n';
        txt[ite + 1] = '\0';
    }

    // write the buffer data into file system
    int length = strlen(txt);
    int num = (length - 1) / BLOCK_SIZE + 1;
    int num0 = num;

    while (num) {
        char buf[BLOCK_SIZE] = {0};
        memcpy(buf, &txt[(num0 - num) * BLOCK_SIZE], BLOCK_SIZE);
        unsigned char *p = fs_head + i * BLOCK_SIZE;
        memcpy(p, buf, BLOCK_SIZE);
        num = num - 1;
        if (num > 0) {
            fat *fat_cur = fat0 + i;
            // need a new block
            if (fat_cur->id == END) {
                int nxt = get_free(1);
                if (nxt == -1) {
                    fprintf(stderr, "write: no more space\n");
                    return -1;
                }

                fat_cur->id = nxt;
                fat_cur = fat0 + nxt;
                fat_cur->id = END;
            }
            i = (fat0 + i)->id;
        }
    }

    // free redundant disk blocks
    if (fat0[i].id != END) {
        int j = fat0[i].id;
        fat0[i].id = END;
        i = j;
        while (fat0[i].id != END) {
            int nxt = fat0[i].id;
            fat0[i].id = FREE;
            i = nxt;
        }
        fat0[i].id = FREE;
    }

    // backup store to fat1
    memcpy(fat1, fat0, 2 * BLOCK_SIZE);
    openfile_list[fd].open_fcb.length = length;
    openfile_list[fd].dirty_bit = 1;
    
    return strlen(input);
}

int Read(char **args) {
    int i;
    char path[PATH_LENGTH], str[WRITE_SIZE];

    if (args[1] == NULL){
        fprintf(stderr, "cat: missing argument\n");
        return 0;
    }

    // check if the file exists, if is it a file rather than a folder
    strcpy(path, args[1]);
    fcb *file = NULL;
    if ((file = find_fcb(path)) == NULL) {
        fprintf(stderr, "read: file does not exist\n");
        return 0;
    }
    if (file->attr == 0) {
        fprintf(stderr, "read: is a folder\n");
        return 0;
    }

    memset(str, 0, WRITE_SIZE);
    for (i = 0; i < MAX_OPENFILE; i++) {
        if (openfile_list[i].valid == 0)
           continue;
        if (!strcmp(file->filename, openfile_list[i].open_fcb.filename) && file->first_block == openfile_list[i].open_fcb.first_block) {
            int length;
            openfile_list[i].pos = 0;
            length = UINT16_MAX;

            do_read(i, length, str);
            fputs(str, stdout);
            return 1;
        }
    }

    // if not open, open it first and close after reading
    int fd = do_open(path);
    int length;
    openfile_list[fd].pos = 0;
    length = UINT16_MAX;

    do_read(fd, length, str);
    fputs(str, stdout);
    do_close(fd);

    return 1;
}

int do_read(int fd, int len, char *txt) {
    memset(txt, 0, WRITE_SIZE);

    if (len <= 0)
        return 0;
    
    // get real reading length
    fat *fat0 = (fat *)(fs_head + BLOCK_SIZE);
    int txt_location = 0;
    int length = len;
    int count = openfile_list[fd].pos;
    if ((openfile_list[fd].open_fcb.length - count) < len) {
        length = openfile_list[fd].open_fcb.length - count;
    }

    // read block by block
    int i = openfile_list[fd].open_fcb.first_block;
    int num = count / BLOCK_SIZE;
    
    for (int j = 0; j < num; ++j) {
        i = (fat0 + i)->id;
    }

    while (length) {
        char buf[BLOCK_SIZE];
        int tmp_count = openfile_list[fd].pos;
        int off = tmp_count % BLOCK_SIZE;

        fcb *p = (fcb *)(fs_head + BLOCK_SIZE * i);
        memcpy(buf, p, BLOCK_SIZE);

        if ((off + length) <= BLOCK_SIZE) {
            memcpy(&txt[txt_location], &buf[off], length);
            openfile_list[fd].pos = openfile_list[fd].pos + length;
            txt_location += length;
            length = 0;
        }
        else {
            int tmp = BLOCK_SIZE - off;
            memcpy(&txt[txt_location], &buf[off], tmp);
            openfile_list[fd].pos += tmp;
            txt_location += tmp;
            length -= tmp;
        }

        i = (fat0 + i)->id;
    }

    return txt_location;
}

int exit_sys(void) {
    for (int i = 0; i < MAX_OPENFILE; ++i) {
        if (openfile_list[i].valid == 1)
            do_close(i);
    }

    FILE *fp = fopen(SYS_PATH, "w");
    fwrite(fs_head, DISK_SIZE, 1, fp);
    free(fs_head);
    fclose(fp);

    return 0;
}

int get_free(int count) {
    unsigned char *ptr = fs_head;
    fat *fat0 = (fat *)(ptr + BLOCK_SIZE);
    int flag = 0;
    int fat[BLOCK_NUM];

    for (int i = 0; i < BLOCK_NUM; ++i, ++fat0)
        fat[i] = fat0->id;

    for (int i = 0, j; i < BLOCK_NUM - count; ++i) {
        for (j = i; j < i + count; ++j) {
            if (fat[j] > 0) {
                flag = 1;
                break;
            }
        }
        if (flag) flag = 0, i = j;
        else return i;
    }

    return -1;
}

int set_free(unsigned short first, unsigned short length, int mode) {
    fat *flag = (fat *)(fs_head + BLOCK_SIZE);
    fat *fat0 = (fat *)(fs_head + BLOCK_SIZE);
    fat *fat1 = (fat *)(fs_head + BLOCK_SIZE * 3);
    int i = first, offset;
    fat0 += first;
    fat1 += first;

    if (mode == 1) {
        // clear the file
        while (fat0->id != END) {
            offset = fat0->id - (fat0 - flag) / sizeof(fat);
            fat0->id = FREE;
            fat1->id = FREE;
            fat0 += offset;
            fat1 += offset;
        }
        fat0->id = FREE;
        fat1->id = FREE;
    }
    else if (mode == 2) {
        // FAT format
        for (i = 0; i < BLOCK_NUM; ++i, ++fat0, ++fat1) {
            fat0->id = FREE;
            fat1->id = FREE;
        }
    }
    else {
        for (; i < first + length - 1; ++i, ++fat0, ++fat1) {
            // any non-zero value will work here
            fat0->id = first + 1;
            fat1->id = first + 1;
        }
        fat0->id = END;
        fat1->id = END;
    }

    return 0;
}

int set_fcb(fcb *f, const char *filename, const char *exname, unsigned char attr, unsigned short first,
            unsigned long length, char ffree) {
    time_t *now = (time_t *)malloc(sizeof(time_t));
    struct tm *timeinfo = NULL;
    time(now);
    timeinfo = localtime(now);

    memset(f->filename, 0, 9);
    memset(f->ext_name, 0, 4);
    strncpy(f->filename, filename, 8);
    strncpy(f->ext_name, exname, 3);
    f->attr = attr;
    f->time = get_time(timeinfo);
    f->date = get_date(timeinfo);
    f->first_block = first;
    f->length = length;
    f->valid = ffree;

    free(now);
    return 0;
}

unsigned short get_time(struct tm *timeinfo) {
    int hour, min, sec;
    unsigned short result;

    hour = timeinfo->tm_hour;
    min = timeinfo->tm_min;
    sec = timeinfo->tm_sec;
    result = (hour << 11) + (min << 5) + (sec >> 1);

    return result;
}

unsigned short get_date(struct tm *timeinfo) {
    int year, mon, day;
    unsigned short result;

    year = timeinfo->tm_year;
    mon = timeinfo->tm_mon;
    day = timeinfo->tm_mday;
    result = (year << 9) + (mon << 5) + day;

    return result;
}

fcb *fcb_cpy(fcb *dest, fcb *src) {
    memset(dest->filename, 0, 9);
    memset(dest->ext_name, 0, 4);

    strcpy(dest->filename, src->filename);
    strcpy(dest->ext_name, src->ext_name);
    dest->attr = src->attr;
    dest->time = src->time;
    dest->date = src->date;
    dest->first_block = src->first_block;
    dest->length = src->length;
    dest->valid = src->valid;

    return dest;
}


char *get_abspath(char *abspath, const char *relpath) {
    // if relpath is already the absolute path
    if (!strcmp(relpath, DELIM) || relpath[0] == '/') {
        strcpy(abspath, relpath);
        return 0;
    }

    char str[PATH_LENGTH];
    char *token = NULL, *end = NULL;
    
    memset(abspath, 0, PATH_LENGTH);
    strcpy(abspath, cur_dir_name);
    strcpy(str, relpath);

    token = strtok(str, DELIM);
    do {
        if (!strcmp(token, "."))
            continue;
        if (!strcmp(token, "..")) {
            if (!strcmp(abspath, ROOT))
                continue;
            end = strrchr(abspath, '/');
            if (end == abspath) {   // parent is root
                strcpy(abspath, ROOT);
                continue;
            }
            // jump to parent directory
            memset(end, 0, 1);
            continue;
        }
        // pad a "/" if parent is not root
        if (strcmp(abspath, "/")) {
            strcat(abspath, DELIM);
        }
        strcat(abspath, token);
    } while ((token = strtok(NULL, DELIM)) != NULL);

    return abspath;
}

fcb *find_fcb(const char *path) {
    char abspath[PATH_LENGTH];
    get_abspath(abspath, path);
    char *token = strtok(abspath, DELIM);
    if (token == NULL) {    // root
        return (fcb *)(fs_head + BLOCK_SIZE * 5);
    }
    return find_fcb_r(token, 5);
}

fcb *find_fcb_r(char *token, int first) {
    int i, length = BLOCK_SIZE;
    char fullname[NAME_LENGTH] = "\0";
    fcb *root = (fcb *)(BLOCK_SIZE * first + fs_head);
    fcb *dir = NULL;
    boot_block *init_block = (boot_block *)fs_head;

    // root occupies 2 blocks
    if (first == init_block->root) {
        length = ROOT_BLOCK_NUM * BLOCK_SIZE;
    }

    // check all fcbs under current directory
    for (i = 0, dir = root; i < length / sizeof(fcb); ++i, ++dir) {
        if (dir->valid == 0)
            continue;
        get_fullname(fullname, dir);
        if (!strcmp(token, fullname)) {
            token = strtok(NULL, DELIM);
            if (token == NULL) {
                return dir;
            }
            return find_fcb_r(token, dir->first_block);
        }
    }

    return NULL;
}

int get_user_open() {
    for (int i = 0; i < MAX_OPENFILE; ++i) {
        if (openfile_list[i].valid == 0) {
            return i;
        }
    }
    return -1;
}

void init_folder(int first, int second) {
    fcb *par = (fcb *)(fs_head + BLOCK_SIZE * first);
    fcb *cur = (fcb *)(fs_head + BLOCK_SIZE * second);

    set_fcb(cur, ".", "di", 0, second, BLOCK_SIZE, 1);
    cur++;
    set_fcb(cur, "..", "di", 0, first, par->length, 1);
    cur++;

    for (int i = 2; i < BLOCK_SIZE / sizeof(fcb); ++i, ++cur)
        cur->valid = 0;
}

void get_fullname(char *fullname, fcb *fcb1) {
    memset(fullname, 0, NAME_LENGTH);
    strcat(fullname, fcb1->filename);
    if (fcb1->attr == 1) {
        strcat(fullname, ".");
        strncat(fullname, fcb1->ext_name, 3);
    }
}

char *trans_date(char *sdate, unsigned short date) {
    int year, month, day;
    memset(sdate, 0, 16);

    year = date & 0xffff;                           // 1111 1111 1111 1111
    month = date & 0x01ff;                          // 0000 0001 1111 1111
    day = date & 0x001f;                            // 0000 0000 0001 1111

    sprintf(sdate, "%04d-%02d-%02d", (year >> 9) + 1900, (month >> 5) + 1, day);
    return sdate;
}

char *trans_time(char *stime, unsigned short time) {
    int hour, min, sec;
    memset(stime, 0, 16);

    hour = time & 0xffff;                           // 1111 1111 1111 1111
    min = time & 0x07ff;                            // 0000 0111 1111 1111
    sec = time & 0x001f;                            // 0000 0000 0001 1111

    sprintf(stime, "%02d:%02d:%02d", hour >> 11, min >> 5, sec << 1);
    return stime;
}

int Del(char **args){

    char path[PATH_LENGTH];

    if (args[1] == NULL){
        fprintf(stderr, "del: missing filename");
        return 0;
    }

    fcb *file = NULL;
    strcpy(path, args[1]);
    if ((file = find_fcb(path)) == NULL){
        fprintf(stderr, "del: no such file\n");
        return 0;
    }
    if (file->attr == 0){
        fprintf(stderr, "del: is a folder\n");
        return 0;
    }

    for (int i = 0; i < MAX_OPENFILE; ++i){
        if (openfile_list[i].valid == 0)
            continue;
        if (!strcmp(file->filename, openfile_list[i].open_fcb.filename) && file->first_block == openfile_list[i].open_fcb.first_block){
            printf("please input location: ");
            scanf("%d", &openfile_list[i].pos);
            getchar();
            char data_str[WRITE_SIZE] = {0};
            do_write(i, data_str, 1, 'd');
            return 1;
        }
    }

    int fd = do_open(path);
    printf("please input location: ");
    scanf("%d", &openfile_list[fd].pos);
    getchar();
    char data_str[WRITE_SIZE] = {0};
    do_write(fd, data_str, 1, 'd');
    do_close(fd);

    return 1;
}