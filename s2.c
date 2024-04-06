//S1 Description: The proposed project combines functionalities for monitoring a directory to manage 
//differences between two captures (snapshots) of it. The user will be able to observe and intervene 
//in the changes in the monitored directory.
//Directory Monitoring: The user can specify the directory to be monitored as an argument in the command 
//line, and the program will track changes occurring in it and its subdirectories, parsing recursively each 
//entry from the directory. With each run of the program, the snapshot of the directory will be updated, 
//storing the metadata of each entry.

//S2: Description: The functionality of the program will be updated to allow it to receive an unspecified number of 
//arguments (directories) in the command line. The logic for capturing metadata will now apply to all received 
//arguments, meaning the program will update snapshots for all directories specified by the user.
//For each entry of each directory provided as an argument, the user will be able to compare the previous snapshot 
//of the specified directory with the current one. If there are differences between the two snapshots, the old
//snapshot will be updated with the new information from the current snapshot.
//The functionality of the code will be expanded so that the program receives an additional argument, representing 
//the output directory where all snapshots of entries from the specified directories in the command line will be stored. 
//This output directory will be specified using the `-o` option. For example, the command to run the program will be: 
//  `./program_exe -o output input1 input2 ...`.

#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <linux/limits.h>
#include <libgen.h>


//FUNCTION TO READ THE DIRECTORY PUT AS ARGUMENT IN TERMINAL & RECURSIVELY TRAVERSE EVERY 
//SUB_DIRECTORY FROM IT. IT SAVES IN A SNAPSHOT.TXT THE PATH AND NAME OF EVERY FILE
void read_directories(const char *path, int snapshot_fd){

    DIR *d = opendir(path);
    struct dirent *dir_file;
    
    char *new_path=NULL; 

    char *dir_name=basename((char*)path); //from libgen, used for getting the name of entries from each path

    if(d){
        while((dir_file = readdir(d)) != NULL){
            if(strcmp(dir_file->d_name, ".") == 0 || strcmp(dir_file->d_name, "..") == 0){
                continue; //all the directories have the entries "." & ".." 
                          //i'm not printing them in the Snapshot file. 
            }
            new_path=realloc(new_path,strlen(path)+strlen(dir_file->d_name) +2); //+2 is for '/' and null terminator
            if(new_path==NULL){                                             
                write(STDERR_FILENO, "error: Failed to allocate memory for path ", strlen("error: Failed to allocate memory for path "));
                write(STDERR_FILENO, path, strlen(path));   //if the allocation of memory fails, after the printed error
                                                            //the loop will break => the directory will not be monitored anymore 
                write(STDERR_FILENO,"\n",1);
                break;
            }

            sprintf(new_path,"%s/%s", path, dir_file->d_name); //constructing the path
            //NEED TO COME BACK LATER AND VERIFY THIS
            struct stat st;                            
            if(lstat(new_path, &st) == -1) {                                                                                    //get file information with lstat
                write(STDERR_FILENO, "error: Failed to get information for ", strlen("error: Failed to get information for "));  //& print error message in case of failing
                write(STDERR_FILENO, dir_file->d_name, strlen(dir_file->d_name)); //writing the entry causing the error
                write(STDERR_FILENO,"\n",strlen("\n"));
                free(new_path);                  //THIS REQUIRE ANOTHER LOOK AT IT
                continue;
            }

            write(snapshot_fd, new_path, strlen(new_path)); //writing the path and name of each file to 
                                                            //the snapshot file
            write(snapshot_fd, "\n", 1);   //writing newline after each line

            if(S_ISDIR(st.st_mode)){       //if an entry is a directory
                                           //recursively call again the function with the new path     
                read_directories(new_path, snapshot_fd);
            }
        }
        free(new_path);
        closedir(d);
    }
    else{
        write(STDERR_FILENO, "error: Failed to open the directory \"", strlen("error: Failed to open the directory \""));
        write(STDERR_FILENO,dir_name,strlen(dir_name));
        write(STDERR_FILENO,"\"\n",strlen("\"\n"));
        return;
    } 
}

//FUNCTION THAT CREATES A SNAPSHOT OF THE SPECIFIED DIRECTORY
//AND COUNTS THE EXISTING ONES FOR CONSTRUNCTING THE NAME OF EACH SNAPSHOT 
void create_snapshot(char *path, char *output_path){

    char *dir_name=basename((char *)path);
    char *output_dir_name=basename((char *)output_path);

    DIR *dir_check=opendir(path); //checking if the directories given as arguments for monitoring   
                                  //exist. If the path is incorrect the error message will be printed to stderr 
                                  //and it will be skipped

    if(dir_check==NULL){
        write(STDERR_FILENO, "error: The directory \"", strlen("error: The directory \""));
        write(STDERR_FILENO,dir_name,strlen(dir_name));
        write(STDERR_FILENO,"\" does not exist!\n",strlen("\" does not exits!\n"));
        return;
    }
    closedir(dir_check);

    dir_check=opendir(output_path); //checking if the output directory given as argument exists. If the path is incorrect
                                    //the snapshots does not have a place to be stored so the program exits.
    if(dir_check==NULL){
        write(STDERR_FILENO, "error: The output directory \"", strlen("error: The output directory \""));
        write(STDERR_FILENO,output_dir_name,strlen(output_dir_name));
        write(STDERR_FILENO,"\" does not exist => Snapshots cannot be saved! => Exiting Program!\n",strlen("\" does not exist => Snapshots cannot be saved! => Exiting Program!\n"));
        exit(EXIT_FAILURE);
    }
    closedir(dir_check); 

    char snapshot_file_name[FILENAME_MAX];
   
    time_t now;
    struct tm *timestamp;  
    char timestamp_str[32];

    time(&now); // --> gets the current time
    timestamp=localtime(&now);
    strftime(timestamp_str,sizeof(timestamp_str),"%Y.%m.%d_%H:%M:%S", timestamp);

    //constructing the snapshot file name with directory name and timestamp
    //it's also creating the snapshot file in the given path as argument
    snprintf(snapshot_file_name, sizeof(snapshot_file_name), "%s/%s_Snapshot_%s.txt",output_path,dir_name,timestamp_str);
  
    int snapshot_fd = open(snapshot_file_name, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR); 
    if(snapshot_fd == -1){
        write(STDERR_FILENO, "error: Failed to open the snapshot file for \"", strlen("error: Failed to open the snapshot file for \""));
        write(STDERR_FILENO,dir_name,strlen(dir_name));
        write(STDERR_FILENO,"\"\n",strlen("\"\n"));
        return;
    }

    read_directories(path, snapshot_fd);

    if(snapshot_fd != -1){ 
        write(STDERR_FILENO, "Snapshot created successfully for \"", strlen("Snapshot created successfully for \""));
        write(STDERR_FILENO,dir_name,strlen(dir_name));
        write(STDERR_FILENO,"\"\n",strlen("\"\n"));
    }

    // (MAYBE AN EXECUTION TIME TO SEE HOW MUCH IT TAKES OR AN PROGRESSION BAR IN THE FUTURE)

    close(snapshot_fd);
}

int main(int argc, char *argv[]){
    
    if(argc < 4){ // minimum 4 arguments because now I need "-o" and the output directory,
                  // the ./a.out and the rest of the paths to directories that will be monitored
        write(STDERR_FILENO, "error: Not enough arguments! => Exiting program!\n", strlen("error: Not enough arguments! => Exiting program!\n"));
        exit(EXIT_FAILURE);
    }
    else{

        char *output_path=NULL;
        int o_count=0;

        for(int i=0;i<argc;i++){          //parsing through all the arguments 
            if(strcmp(argv[i],"-o")==0){  //for error handling
                o_count++;
                output_path=argv[i+1];
            }
            if(o_count<1 && i+1==argc){
                     write(STDERR_FILENO,"error: The argument \"-o\" was not detected in the terminal! => Exiting program!\n",strlen("error: The argument \"-o\" was not detected in the terminal! => Exiting program!\n"));
                     exit(EXIT_FAILURE);
            }
            if(o_count>1){
                    write(STDERR_FILENO,"error: The argument \"-o\" was detected more than once in the terminal! => Exiting program!\n",strlen("error: The argument \"-o\" was detected more than once in the terminal! => Exiting program!\n"));
                    exit(EXIT_FAILURE);
            }
            if(strcmp(argv[i],"-o")==0 && i+1==argc){
                write(STDERR_FILENO,"error: \"-o\" cannot be the last argument! => Exiting program!\n",strlen("error: \"-o\" cannot be the last argument! => Exiting program!\n"));
                exit(EXIT_FAILURE);
            }
        }

        for(int i=1;i<argc;i++){  //parsing again through all the argument
                if(strcmp(argv[i],"-o")==0 && i+1<argc){ //basically if the argument is "-o" then the next one
                                                         //is the output directory so we skip it
                    i++;
                }
                else{
                    char *path = argv[i];                //the rest of the arguments are directories
                    create_snapshot(path,output_path);   //that are monotored
                }
        }
    }

    return EXIT_SUCCESS;
}