#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

void read_directories(const char *path, FILE *fp){
    DIR *d = opendir(path);
    struct dirent *dir_file;
    if(d){
        while((dir_file = readdir(d)) != NULL){
            if(strcmp(dir_file->d_name, ".") == 0 || strcmp(dir_file->d_name, "..") == 0){
                continue; //all the directories have the entries "." & ".." 
                          //i'm not printing them in the Snapshot file. 
            }
            char new_path[1024]; //concatenating the path and the name of each entry into 
                                 // 'new_path'
            snprintf(new_path, sizeof(new_path), "%s/%s", path, dir_file->d_name);

            struct stat st;                            
            if (lstat(new_path, &st) == -1) { //get file information with lstat
                perror("error");                    //& print error message in case of failing
                continue;
            }
            fprintf(fp, "%s\n", new_path); // printing every snapshot_entry in the Snapshot file
            if(S_ISDIR(st.st_mode)){     //if an entry is a directory
                                         //recursively call again the function with the new path     
                read_directories(new_path, fp);
            }
        }
        closedir(d);
    }
    else{
        printf("error: The given path is incorrect!\n");
        exit(EXIT_FAILURE);
    } 
}

int main(int argc, char *argv[]){
    
    FILE *fp = fopen("Snapshot.txt", "w");

    if(fp == NULL){
        printf("error: Cannot open the snapshot file!\n");
        exit(EXIT_FAILURE);        
    }
    if(argc == 2){
        char *path = argv[1];
        read_directories(path, fp);
    }
    else{
        printf("error: There should be only one argument!\n");
        exit(EXIT_FAILURE);
    }

    fclose(fp);
    return 0;
}
