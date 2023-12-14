/***************************************************************************
* fs.h
    fs header file
***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

#define BLOCK_SIZE      256
#define BLOCK_NUM       4096
#define DISK_SIZE       1048576
#define WRITE_SIZE      20 * BLOCK_SIZE

#define SYS_PATH        "./fsfile"
#define ROOT            "/"
#define DELIM           "/"

#define END             0xffff
#define FREE            0x0000

#define ROOT_BLOCK_NUM  2
#define MAX_OPENFILE    10
#define NAME_LENGTH     32
#define PATH_LENGTH     128

#define FOLDER_COLOR    "\e[32m"
#define DEFAULT_COLOR   "\e[0m"


typedef struct BOOT_BLOCK {
    char information[200];
    unsigned short root;
    unsigned char *start_block;
} boot_block;

typedef struct FCB {
    char filename[9];
    char ext_name[4];
    char valid;
    unsigned char attr;             // 0 - dir, 1 - file
    unsigned short time;
    unsigned short date;
    unsigned short first_block;
    unsigned long length;
} fcb;

typedef struct FAT {
    unsigned short id;
} fat;

typedef struct USER_OPEN {
    fcb open_fcb;                   // copy fcb
    char dir_name[80];
    int pos;
    char dirty_bit;
    char valid;
} user_open;


unsigned char *fs_head;             // memory space head of the file system
user_open openfile_list[MAX_OPENFILE];
int cur_dir_idx;                    // current directory index in openfile_list
char cur_dir_name[80];              // current directory name
unsigned char *start;               // data block start position


int start_sys(void);

int Format(void);

int Chdir(char **args);

int Pwd(void);

int Mkdir(char **args);

int in_mkdir(const char *parent_path, const char *dirname);

int Rmdir(char **args);

void in_rmdir(fcb *dir);

int Ls(char **args);

void in_ls(int first, char mode);

int Create(char **args);

int in_create(const char *parpath, const char *filename);

int Rm(char **args);

int do_open(char *path);
void do_close(int fd);

// -a denotes close all open files except fot current directory
int my_close(char **args);

// -w cover write, -i insert write, -a add write
int Write(char **args);

int do_write(int fd, char *content, size_t len, int wstyle);

int Del(char **args);

int Read(char **args);

int do_read(int fd, int len, char *text);

int exit_sys();

int get_free(int count);

int set_free(unsigned short first, unsigned short length, int mode);

int set_fcb(fcb *f, const char *filename, const char *exname, unsigned char attr, unsigned short first,
            unsigned long length,
            char ffree);

unsigned short get_time(struct tm *timeinfo);

unsigned short get_date(struct tm *timeinfo);

fcb * fcb_cpy(fcb *dest, fcb *src);

char * get_abspath(char *abspath, const char *relpath);

// find a free position in openfile_list
int get_user_open();

// recursively find fcb, return -1 if fail
fcb *find_fcb(const char *path);
fcb *find_fcb_r(char *token, int root);

void init_folder(int first, int second);

void get_fullname(char *fullname, fcb *fcb1);

char *trans_date(char *sdate, unsigned short date);

char *trans_time(char *stime, unsigned short time);
