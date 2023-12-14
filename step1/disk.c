/***********************************************************************
 * disk.c
 *  physical disk simulation
 **********************************************************************/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>

#define BUFFER_SIZE 4096
#define PROMPT "==>"

int CYLINDERS, SECTORS_PER_CYLINDER, TRACK_DELAY, SECTOR_SIZE = 256;
int current_c = 0;
char DISK_FILENAME[50];
char instruction_buffer[BUFFER_SIZE];
char data_buffer[BUFFER_SIZE];
int end = 0;
FILE *out_log;

int main(int argc, char **argv){

    if (argc != 5){
        printf("TO RUN: ./disk <cylinders> <sector_per_cylinder> <track_to_track_delay> <disk_storage_filename>\n");
        return 0;
    }

    // get the arguments
    CYLINDERS = atoi(argv[1]);
    SECTORS_PER_CYLINDER = atoi(argv[2]);
    TRACK_DELAY = atoi(argv[3]);
    strcpy(DISK_FILENAME, argv[4]);

    // open the file
    int fd = open(DISK_FILENAME, O_RDWR | O_CREAT, 0);
    if (fd < 0){
        printf("ERROR: failed to open file '%s'\n", DISK_FILENAME);
        exit(-1);
    }

    // stretch file size
    long FILE_SIZE = CYLINDERS * SECTORS_PER_CYLINDER * SECTOR_SIZE;
    if (lseek(fd, FILE_SIZE - 1, SEEK_SET) == -1){
        perror("Error calling lseek() to stretch the file\n");
        close(fd);
        exit(-1);
    }
    // ensure stretch success
    if (write(fd, "", 1) != 1){
        perror("Error writing last byte of the file\n");
        close(fd);
        exit(-1);
    }

    // map the file
    char *disk_file = (char *) mmap(NULL, FILE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (disk_file == MAP_FAILED){
        close(fd);
        printf("ERROR: failed to map file\n");
        exit(-1);
    }

    // open disk.log, output destination
    out_log = fopen("disk.log", "w+");
    if (out_log == NULL){
        perror("Error opening 'disk.log'\n");
        close(fd);
        exit(-1);
    }

    /*******************************************
     * read instructions
     * ****************************************/
    printf("%s", PROMPT);

    while (fgets(instruction_buffer, BUFFER_SIZE, stdin)){
        switch (instruction_buffer[0]) {
            case 'I':
            case 'i':
                fprintf(out_log, "#cylinders: %d, #sectors per cylinder: %d\n", CYLINDERS, SECTORS_PER_CYLINDER);
                printf("#cylinders: %d, #sectors per cylinder: %d\n", CYLINDERS, SECTORS_PER_CYLINDER);
                break;

            // read request
            case 'R':
            case 'r': {
                int pos = 2;
                int c = 0, s = 0;
                // get c
                while (instruction_buffer[pos] && instruction_buffer[pos] != ' ')
                    c = c * 10 + (instruction_buffer[pos++] - '0');
                // get s
                pos++;
                while (instruction_buffer[pos] && instruction_buffer[pos] != ' ' && instruction_buffer[pos] != '\n')
                    s = s * 10 + (instruction_buffer[pos++] - '0');
                // delay time
                usleep(TRACK_DELAY * abs(c - current_c));
                current_c = c;
                // read by memcpy()
                if (c < CYLINDERS && s < SECTORS_PER_CYLINDER) {
                    char *src_ptr = &disk_file[SECTOR_SIZE * (c * SECTORS_PER_CYLINDER + s)];
                    memcpy(data_buffer, src_ptr, SECTOR_SIZE);
                    fprintf(out_log, "Yes %s\n", data_buffer);
                    printf("Yes %s\n", data_buffer);
                }
                else{
                    fprintf(out_log, "No\n");
                    printf("No\n");
                }
                break;
            }

            // write request
            case 'W':
            case 'w': {
                int pos = 2;
                int c = 0, s = 0;
                // get c
                while (instruction_buffer[pos] != ' ')
                    c = c * 10 + (instruction_buffer[pos++] - '0');
                // get s
                pos++;
                while (instruction_buffer[pos] != ' ')
                    s = s * 10 + (instruction_buffer[pos++] - '0');
                pos++;
                // time delay
                usleep(TRACK_DELAY * abs(c - current_c));
                current_c = c;
                // write to assigned position
                if (c < CYLINDERS && s < SECTORS_PER_CYLINDER) {
                    char *dst_ptr = &disk_file[SECTOR_SIZE * (c * SECTORS_PER_CYLINDER + s)];
                    memcpy(dst_ptr, instruction_buffer + pos, strlen(instruction_buffer + pos));
                    fprintf(out_log, "Yes\n");
                    printf("Yes\n");
                }
                else{
                    fprintf(out_log, "No\n");
                    printf("No\n");
                }
                break;
            }

            // exit request
            case 'E':
            case 'e':
                fprintf(out_log, "Goodbye!\n");
                printf("Goodbye!\n");
                end = 1;
                break;

            // other request, wrong format
            default:
                printf("Invalid request\n");
                fprintf(out_log, "Invalid request\n");
        }

        if (end)
            break;

        printf("%s", PROMPT);
    }

    fclose(out_log);
    close(fd);

    return 0;
}