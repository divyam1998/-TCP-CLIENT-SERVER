#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ftw.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <time.h>

#define MAX_DIRS 1000
#define MAX_DIR_LENGTH 256
int dateFlag = 0;
int connectionCount = 0;
int timebasedFiles = 0;
char tarCmd[4096] = "tar -czf /tmp/temp.tar.gz";  //here i am declaring initiak part of the tar command
int size1, size2;  // Global variables to store size range
typedef struct {
    char *path;
    time_t creation_time;
} DirInfo;

DirInfo *directory_list = NULL;
size_t num_directories = 0;
size_t capacity = 0;
char search_filename[256] = {0};

typedef struct {
    char *path;
    off_t size;
    time_t creation_time;
    mode_t permissions;
} FileInfo;

FileInfo *found_file = NULL;

void reset(){
 if(directory_list!=NULL){
 free(directory_list);
 directory_list = NULL;
 }
 num_directories = 0;
 capacity = 0;
 timebasedFiles = 0;

}

static int append_to_tar(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    if (typeflag == FTW_F && sb->st_size >= size1 && sb->st_size <= size2) {
        // Estimate the new length after appending the file path
        size_t new_length = strlen(tarCmd) + strlen(fpath) + 4; // Add space for " ''" and null-terminator
        
        if (new_length < sizeof(tarCmd)) {
            // Append the path safely
            strcat(tarCmd, " '");
            strcat(tarCmd, fpath);
            strcat(tarCmd, "'");
        } else {
            fprintf(stderr, "tarCmd string is too long, cannot append more files.\n");
            return -1; // This will stop nftw from continuing
        }
    }
    return 0; // Continue walking
}

time_t target_date;
static int append_to_tar_time_old(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
   
       if (typeflag == FTW_F && sb->st_mtime <= target_date) {
            size_t new_length = strlen(tarCmd) + strlen(fpath) + 4;
            timebasedFiles++;
            if (new_length < sizeof(tarCmd)) {
                // Safely append the file path to the tar command
                strcat(tarCmd, " '");
                strcat(tarCmd, fpath);
                strcat(tarCmd, "'");
            } else {
                fprintf(stderr, "tarCmd string is too long, cannot append more files.\n");
                return -1; // Stop nftw from continuing
            }
         }
    
    
    return 0; // Continue walking
}

static int append_to_tar_time_new(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
       if (typeflag == FTW_F && sb->st_mtime >= target_date) {
            size_t new_length = strlen(tarCmd) + strlen(fpath) + 4;
            timebasedFiles++;
            if (new_length < sizeof(tarCmd)) {//here we use strcat to concatenate the commands in tarCmd which will lateer be used for processing
                strcat(tarCmd, " '");
                strcat(tarCmd, fpath);
                strcat(tarCmd, "'");
            } else {
                fprintf(stderr, "Tarcmd is large as per the provided size so sorry\n"); //this the case where the size gets bigger 
                return -1;
            }
         }
    return 0; //we continue to traverse the nftw
}

void dataTraversal(const char *dateStr, int client_socket) {
    struct tm tm = {0};
    memset(tarCmd, 0, sizeof(tarCmd));
    sprintf(tarCmd, "tar -czf /tmp/temp.tar.gz");
    if (strptime(dateStr, "%Y-%m-%d", &tm) == NULL) {
        const char *errorMsg = "Invalid date format.\n";
        send(client_socket, errorMsg, strlen(errorMsg), 0);
        return;
    }
    target_date = mktime(&tm);

    //getting the home directory
    const char *homeDir = getenv("HOME");
    //this is the conditions for option 6 and option 7 respectively
    if(dateFlag==0){
         if (nftw(homeDir, append_to_tar_time_old, 20, FTW_PHYS) != 0) {
            perror("sorry there is an some error while nftw");
         }

    }else{
         if (nftw(homeDir, append_to_tar_time_new, 20, FTW_PHYS) != 0) {
             perror("sorry there is an some error while nftw");
        }

    }
    if(timebasedFiles == 0){
        char msg[] = "No file found\n";
        send(client_socket, msg, strlen(msg), 0);
        return;
    }
    printf("command formed : %s\n",tarCmd);
    //now here we run the tar command
    system(tarCmd);
    send_tar_file(client_socket, "/tmp/temp.tar.gz");
}
void send_tar_file(int client_socket, const char* tarFilePath) {
    struct stat tarStat;
    if (stat(tarFilePath, &tarStat) != 0 || tarStat.st_size == 0) {
        perror("Cannot access tar file or file is empty");
        return;
    }
    char msg[] = "Starting file transfer\n";
     send(client_socket, msg, strlen(msg), 0); //giving message to client that i am sending files
    FILE *tarFile = fopen(tarFilePath, "r");
    if (!tarFile) {
        perror("Failed to open tar file for sending");
        return;
    }

    // Send file size first
    long netFileSize = htonl(tarStat.st_size);
    send(client_socket, &netFileSize, sizeof(netFileSize), 0);

    // Then send file content
    char buffer[4096];
    size_t bytesRead;
    while ((bytesRead = fread(buffer, 1, sizeof(buffer), tarFile)) > 0) {
        send(client_socket, buffer, bytesRead, 0);
    }
    fclose(tarFile);
    printf("Tar file sent successfully.\n");
}

void create_tar_file(const char *baseDir, int minSize, int maxSize, int client_socket) {
    memset(tarCmd, 0, sizeof(tarCmd));
    sprintf(tarCmd, "tar -czf /tmp/temp.tar.gz");

    size1 = minSize;
    size2 = maxSize;
    if (nftw(baseDir, append_to_tar, 20, FTW_PHYS) == -1) {
        perror("nftw failed");
        return;
    }

    printf("command formed : %s\n",tarCmd);
    system(tarCmd);
    struct stat tarStat;
    if (stat("/tmp/temp.tar.gz", &tarStat) != 0 || tarStat.st_size == 0) {
        char msg[] = "No file found\n";
        send(client_socket, msg, strlen(msg), 0);
        return;
    }

    send_tar_file(client_socket, "/tmp/temp.tar.gz");
}


void resize_directory_array() {
    if (num_directories >= capacity) {
        size_t new_capacity = (capacity == 0) ? 10 : capacity * 2;
        DirInfo *new_list = realloc(directory_list, new_capacity * sizeof(DirInfo));
        if (new_list == NULL) {
            perror("Failed to realloc memory");
            exit(EXIT_FAILURE);
        }
        directory_list = new_list;
        capacity = new_capacity;
    }
}

static int store_dir_entry(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    if (typeflag == FTW_D && ftwbuf->level == 1) {
        const char *dirName = strrchr(fpath, '/') + 1;
        if (dirName[0] != '.') {
            resize_directory_array();
            directory_list[num_directories].path = strdup(fpath);
            directory_list[num_directories].creation_time = sb->st_ctime;
            if (directory_list[num_directories].path == NULL) {
                perror("Failed to duplicate directory name");
                exit(EXIT_FAILURE);
            }
            num_directories++;
        }
    }
    return 0; // Continue walking
}

int compare_alphabetical(const void *a, const void *b) {
    const DirInfo *dir_a = (const DirInfo*)a;
    const DirInfo *dir_b = (const DirInfo*)b;
    return strcmp(dir_a->path, dir_b->path);
}

int compare_creation_time(const void *a, const void *b) {
    const DirInfo *dir_a = (const DirInfo*)a;
    const DirInfo *dir_b = (const DirInfo*)b;
    if (dir_a->creation_time < dir_b->creation_time) return -1;
    if (dir_a->creation_time > dir_b->creation_time) return 1;
    return 0;
}

void send_sorted_directories(int sd, int sort_type) {
    printf("Entered\n");
    if (sort_type == 0) {
        printf("Sorting directories alphabetically.\n");
        qsort(directory_list, num_directories, sizeof(DirInfo), compare_alphabetical);
    } else if (sort_type == 1) {
        printf("Sorting directories by creation time.\n");
        qsort(directory_list, num_directories, sizeof(DirInfo), compare_creation_time);
    } else {
        printf("Invalid sort type.\n");
        return;
    }

    // Concatenate directory paths into a single string
    char directory_string[MAX_DIRS * MAX_DIR_LENGTH] = {0};
    size_t directory_string_len = 0;
    for (size_t i = 0; i < num_directories; i++) {
        size_t path_len = strlen(directory_list[i].path);
        if (directory_string_len + path_len + 1 < MAX_DIRS * MAX_DIR_LENGTH) {
            strcat(directory_string, directory_list[i].path);
            strcat(directory_string, "\n"); // Add newline after each directory
            directory_string_len += path_len + 1;
        } else {
            printf("Directory string is full, cannot add more directories.\n");
            break;
        }
    }

    // Send the concatenated directory string
    ssize_t bytes_sent = send(sd, directory_string, strlen(directory_string), 0);
    if (bytes_sent < 0) {
        perror("send failed");
    } else if (bytes_sent != (ssize_t)strlen(directory_string)) {
        printf("Incomplete send\n");
        // Handle the case where not all bytes were sent
    }

    printf("Finished sending directory listing.\n");
}


static int find_file(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    const char *filename = strrchr(fpath, '/') ? strrchr(fpath, '/') + 1 : fpath;
    if (typeflag == FTW_F && strcmp(filename, search_filename) == 0) {
        found_file = malloc(sizeof(FileInfo));
        if (found_file == NULL) {
            perror("Memory allocation failed");
            return -1; // Stop walking
        }
        found_file->path = strdup(fpath);
        found_file->size = sb->st_size;
        found_file->creation_time = sb->st_mtime;
        found_file->permissions = sb->st_mode;
        return 1;  // Stop walking since file is found
    }
    return 0;  // Continue walking
}

void crequest(int sd) {
    int n;
    char message[1024] = {0};
    while(1){
        memset(message, 0, sizeof(message));  // Clear the buffer
        n = read(sd, message, 255);
        if (n > 0) {
            message[n] = '\0';
            message[strcspn(message, "\n")] = 0; 
            message[strcspn(message, "\r")] = 0; 
            
            char *case1 = strstr(message, "-a");
            char *case2 = strstr(message, "-t");
            char *case3 = strstr(message,"w24fn");
            char *case4 = strstr(message,"w24fz");
            char *case5 = strstr (message,"w24ft");
            char *case6 = strstr(message,"w24fdb");
            char *case7 = strstr(message,"w24fda");
            char *case8 = strstr (message,"quitc");

            // printf("strstr result: %p\n", case1);
             if (case1) {
                const char *home_dir = getenv("HOME");
                if (home_dir == NULL) {
                    printf("Error: HOME environment variable is not set.\n");
                    // You might want to handle this error condition appropriately
                } else {
                    printf("Entered -a yes command processing\n");
                    // Check if nftw returns successfully
                    int nftw_result = nftw(home_dir, store_dir_entry, 20, FTW_PHYS);
                    if (nftw_result != 0) {
                        printf("Error: nftw failed with error code %d\n", nftw_result);
                        // You might want to handle this error condition appropriately
                    } else {
                        printf("nftw successful.\n");
                        // Check if send_sorted_directories returns successfully
                        send_sorted_directories(sd, 0); // Alphabetical sort
                      
                        printf("Entered\n");
                        fflush(stdout); // Flush the output buffer
                    }
                }
                reset();
            }else if (case2) {
                const char *home_dir = getenv("HOME");
                nftw(home_dir, store_dir_entry, 20, FTW_PHYS);
                send_sorted_directories(sd,1);
                 reset(); // Time sort
            } else if (case3) {
                printf("entered case3");
                strncpy(search_filename, message + 6, sizeof(search_filename) - 1);
                search_filename[strcspn(search_filename, "\n")] = 0; // Strip newline
                const char *home_dir = getenv("HOME");
                if (nftw(home_dir, find_file, 20, FTW_PHYS) == 1 && found_file) {
                    char file_info[1024];
                    sprintf(file_info, "File: %s\nSize: %ld bytes\nDate Modified: %ld\nPermissions: %o\n",
                    found_file->path, found_file->size, found_file->creation_time, found_file->permissions & 0777);
                    write(sd, file_info, strlen(file_info));
                    free(found_file->path);
                    free(found_file);
                    found_file = NULL;
                } else {
                    write(sd, "File not found\n", 16);
                }
                 reset();
            } else if (case4) {
               // char input[] = message;
                char command[10];
                int size1, size2;
                const char *home_dir = getenv("HOME");

                // Use sscanf to extract the command and two sizes
                if (sscanf(message, "%s %d %d", command, &size1, &size2) == 3) {
                    printf("Command: %s\n", command);
                    printf("Size 1: %d\n", size1);
                    printf("Size 2: %d\n", size2);
                    create_tar_file(home_dir,size1,size2,sd);
                } else {
                    printf("Invalid input format\n");
                }
                 reset();
            } else if (case5) {
                // Case 5 logic
                 reset();
            } else if(case6) {
                char command[10];
                char date[10];
                int size1, size2;
                const char *home_dir = getenv("HOME");
                dateFlag = 0;
                // Use sscanf to extract the command and two sizes
                if (sscanf(message, "%s %s", command, date) == 2) {
                    printf("Command: %s\n", command);
                    printf("date: %s\n", date);
                    dataTraversal(date,sd);
                } else {
                    printf("Invalid input format\n");
                }
                reset();
            } else if (case7) {
             char command[10];
              char date[10];
                int size1, size2;
                const char *home_dir = getenv("HOME");
                dateFlag = 1;
                // Use sscanf to extract the command and two sizes
                if (sscanf(message, "%s %s", command, date) == 2) {
                    
                    printf("Command: %s\n", command);
                    printf("date: %s\n", date);
                    dataTraversal(date,sd);
                   
                } else {
                    printf("Invalid input format\n");
                }
                 reset();
            } else if(case8) {
                
                 reset();
            }
        }
    }
}

int main(int argc, char *argv[]) {
    int sd, client, portNumber;
    struct sockaddr_in servAdd;

    if (argc != 2) {
        printf("Call model: %s <Port Number>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    servAdd.sin_family = AF_INET;
    servAdd.sin_addr.s_addr = htonl(INADDR_ANY);
    sscanf(argv[1], "%d", &portNumber);
    servAdd.sin_port = htons((uint16_t)portNumber);

    if (bind(sd, (struct sockaddr *)&servAdd, sizeof(servAdd)) == -1) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(sd, 5) == -1) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    while (1) {
        client = accept(sd, NULL, NULL);
        if (client < 0) {
            perror("Accept failed");
            continue;
        }
        if(connectionCount == 9){
           connectionCount = 0;
        }
        if(connectionCount <= 3){
           connectionCount++;
            printf("Got a client\n");
        }else if(connectionCount <=6){
            char *redirect_message = "CLOSED_1";
            send(client, redirect_message, strlen(redirect_message), 0); //sending this to mirror1
            close(client);
        }else{
            char *redirect_message = "CLOSED_2"; //sending this for mirror2
            send(client, redirect_message, strlen(redirect_message), 0);
            close(client);
        }
       

        if (!fork()) { // creating a new child process
            close(sd); // close the listening socket in the child process
            crequest(client);
            exit(0);
        }
        close(client); // close the client socket in the parent process
    }
}
