//S1 Description: The proposed project combines functionalities for monitoring a directory to manage 
//differences between two captures (snapshots) of it. The user will be able to observe and intervene 
//in the changes in the monitored directory.

//Directory Monitoring: The user can specify the directory to be monitored as an argument in the command 
//line, and the program will track changes occurring in it and its subdirectories, parsing recursively each 
//entry from the directory. With each run of the program, the snapshot of the directory will be updated, 
//storing the metadata of each entry.

#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>


//FUNCTION TO READ THE DIRECTORY PUT AS ARGUMENT IN TERMINAL & RECURSIVELY TRAVERSE EVERY 
//SUB_DIRECTORY FROM IT. IT SAVES IN A SNAPSHOT.TXT THE PATH AND NAME OF EVERY FILE
void read_directories(const char *path, int snapshot_fd){
    DIR *d = opendir(path);
    struct dirent *dir_file;
    char *new_path=NULL;
    if(d){
        while((dir_file = readdir(d)) != NULL){
            if(strcmp(dir_file->d_name, ".") == 0 || strcmp(dir_file->d_name, "..") == 0){
                continue; //all the directories have the entries "." & ".." 
                          //i'm not printing them in the Snapshot file. 
            }
            new_path=realloc(new_path,strlen(path)+strlen(dir_file->d_name) +2); //+2 is for '/' and null terminator
            if(new_path==NULL){                                             
                write(STDERR_FILENO, "error: Failed to allocate memory for path\n", strlen("error: Failed to allocate memory for path\n"));
                exit(EXIT_FAILURE);
            }

            sprintf(new_path,"%s/%s", path, dir_file->d_name); //constructing the path

            struct stat st;                            
            if (lstat(new_path, &st) == -1) {                                                                                        //get file information with lstat
                write(STDERR_FILENO, "error: Failed to get information for\n", strlen("error: Failed to get information for\n"));    //& print error message in case of failing
                free(new_path);                  
                continue;
            }

            write(snapshot_fd, new_path, strlen(new_path)); //writing to the snapshot file
            write(snapshot_fd, "\n", 1);

            if(S_ISDIR(st.st_mode)){       //if an entry is a directory
                                           //recursively call again the function with the new path     
                read_directories(new_path, snapshot_fd);
            }
        }
        free(new_path);
        closedir(d);
    }
    else{
        write(STDERR_FILENO, "error: Failed to open the directory\n", strlen("error: Failed to open the directory\n"));
        exit(EXIT_FAILURE);
    } 
}

//FUNCTION THAT CREATES A SNAPSHOT OF THE SPECIFIED DIRECTORY
//AND COUNTS THE EXISTING ONES FOR CONSTRUNCTING THE NAME OF EACH SNAPSHOT 
void create_snapshot(const char *path){

    DIR *dir_check=opendir(path); //if the path is incorrect no snapshot file is created 
    if(dir_check==NULL) {
        write(STDERR_FILENO, "error: Specified directory does not exist\n", strlen("error: Specified directory does not exist\n"));
        exit(EXIT_FAILURE);
    }
    closedir(dir_check);

    char snapshot_file_name[FILENAME_MAX];

    time_t now;
    struct tm *timestamp;  
    char timestamp_str[32];

    time(&now);     //getting the current time
    timestamp=localtime(&now);
    strftime(timestamp_str,sizeof(timestamp_str),"%Y.%m.%d_%H:%M:%S", timestamp);

    //constructing the name
    snprintf(snapshot_file_name,sizeof(snapshot_file_name),"Snapshot_%s.txt",timestamp_str);

    int snapshot_fd=open(snapshot_file_name, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR); 
    if (snapshot_fd ==-1) {
        write(STDERR_FILENO, "error: Failed to open snapshot file\n", strlen("error: Failed to open snapshot file\n"));
        exit(EXIT_FAILURE);
    }

    read_directories(path,snapshot_fd);
    close(snapshot_fd);
}

int main(int argc, char *argv[]){
    
    if(argc == 2){
        char *path = argv[1];
        create_snapshot(path);
        write(STDERR_FILENO, "Snapshot created successfully!\n", strlen("Snapshot created successfully!\n"));
    }
    else{
        write(STDERR_FILENO, "error: There should be only one argument!\n", strlen("error: There should be only one argument!\n"));
        exit(EXIT_FAILURE);
    }
    return 0;
}