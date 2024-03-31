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

//FUNCTION TO READ THE DIRECTORY PUT AS ARGUMENT IN TERMINAL & RECURSIVELY TRAVERSE EVERY 
//SUB_DIRECTORY FROM IT. IT SAVES IN A SNAPSHOT.TXT THE PATH AND NAME OF EVERY FILE
void read_directories(const char *path, FILE *fp){
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
                fprintf(stderr,"error: Failed to allocate memory for path: %s\n",dir_file->d_name);
                exit(EXIT_FAILURE);
            }
            sprintf(new_path,"%s/%s", path, dir_file->d_name);

            struct stat st;                            
            if (lstat(new_path, &st) == -1) {                                             //get file information with lstat
                fprintf(stderr,"error: Failed to get information for: %s\n",new_path);    //& print error message in case of failing
                free(new_path);                  
                continue;
            }
            fprintf(fp, "%s\n", new_path); // printing every snapshot_entry in the Snapshot file
            if(S_ISDIR(st.st_mode)){       //if an entry is a directory
                                           //recursively call again the function with the new path     
                read_directories(new_path, fp);
            }
        }
        free(new_path);
        closedir(d);
    }
    else{
        fprintf(stderr,"error: The given path is incorrect!\n");
        exit(EXIT_FAILURE);
    } 
}

void create_snapshot(const char *path,FILE *snapshot){
    if(snapshot==NULL){
        fprintf(stderr,"error: Invalid snapshot file!\n");
        exit(EXIT_FAILURE);
    }
    read_directories(path,snapshot);
}

int main(int argc, char *argv[]){
    
    FILE *fp = fopen("Snapshot.txt", "w");

    if(fp == NULL){
        fprintf(stderr,"error: Cannot open the snapshot file!\n");
        exit(EXIT_FAILURE);        
    }
    if(argc == 2){
        char *path = argv[1];
        create_snapshot(path, fp);
    }
    else{
        fprintf(stderr,"error: There should be only one argument!\n");
        exit(EXIT_FAILURE);
    }

    fclose(fp);
    return 0;
}
