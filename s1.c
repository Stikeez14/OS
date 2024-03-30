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

int main(int argc,char *argv[]){

    DIR *d;
    struct dirent *dir;
    if(argc==2){
        char *path=argv[1];
        d=opendir(path);
        if(d){
            while((dir=readdir(d))!=NULL){
                printf("%s\n",dir->d_name);   //printing the content of the directory
            }
            closedir(d);
        }
        else{
            printf("error: The given path is incorrect!\n"); //if the path to the directory is incorrect =>
                                                             // => error message + exits the program
            exit(EXIT_FAILURE);                     
        } 
    }
    else{
        printf("error: There should be only one argument!\n");  //if in the terminal is not exactly one argument
        exit(EXIT_FAILURE);                                     // (the path to the directory) => error mesage + exits the program
    }
}