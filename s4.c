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
#include <sys/wait.h>

#define MAX_LINE 128

/*
    FUNCTION PROTOTYPES
*/

//traverse the directory given as argument and his sub_directories recursively
//and writes in the snapshot file the path and the name of each entry 
void read_directories(const char *path, int snapshot_fd);

//creates a snapshot file in the output directory for each 
//directory given as argument in the terminal for monitoring.
void create_snapshot(char *path, char *output_path);

//searches in the output directory all the snapshots that starts with the same directory name and calls 
//the compare_snapshots function if two snapshots are found. If there is only one, no comparation is made
void get_previous_snapshot(const char *output_path, const char *dir_name, const char *snapshot_file_name);

//compares the snapshot that is created when the program runs, with the snapshot that was previously created
//then overrides the previous snapshot with the information of the current one
void compare_snapshots(const char *prev_snapshot_file_name, const char *snapshot_file_name, const char *dir_name);


/*
    READ DIRECTORIES FUNCTION IMPLEMENTATION
*/
void read_directories(const char *path, int snapshot_fd){

    DIR *d = opendir(path);
    struct dirent *dir_entry;
    
    char *entries_path=NULL; //storing the path and name of each directory entry
    char *file_info=NULL;  //storing information for each directory entry

    char *dir_name=basename((char*)path); //from libgen, used for getting the name of the input directory

    if(d){
        while((dir_entry = readdir(d)) != NULL){

            //not printing in the snapshot file the entries "." & ".." 
            if(strcmp(dir_entry->d_name, ".") == 0 || strcmp(dir_entry->d_name, "..") == 0)  continue;  
 
            entries_path=realloc(entries_path, strlen(path) + strlen(dir_entry->d_name) +2); //+2 is for '/' and null terminator
            if(entries_path == NULL){          
                fprintf(stderr, "*read_directories* error: Failed to allocate memory for path: %s\n", path);
                //if the allocation of memory fails, after the printed error
                //the loop will break => the directory will not be monitored anymore 
                free(entries_path);       
                break;
            }
            sprintf(entries_path, "%s/%s", path, dir_entry->d_name); //constructing the path of each entry

            struct stat st;                        //get file information with lstat      
            if(lstat(entries_path, &st) == -1){    //& print error message in case of failing     
                fprintf(stderr, "*read_directories* error: Failed to get information for path: %s\n", dir_entry->d_name);  
                free(entries_path);                                      
                break;   
            }
            
            //gets the actual size of each line & used for reallocaating memory 
            size_t data_length = snprintf(NULL, 0, "Path: %s\nSize: %ld bytes\nMode: %o\nHard Links: %ld\nUser ID: %d\nGroup ID: %d\n",  entries_path, st.st_size, st.st_mode, st.st_nlink, st.st_uid, st.st_gid);

            //reallocating memory for file_info then write in it entries_path & some additional data
            file_info=realloc(file_info, (data_length+1)); // +1 is for the null terminator
            if(file_info == NULL){
                fprintf(stderr, "*read_directories* error: Failed to allocate memory for entry: %s\n", dir_entry->d_name);
                free(file_info);
                break;
            }

            sprintf(file_info, "Path: %s\nSize: %ld bytes\nMode: %o\nHard links: %ld\nUser ID: %d\nGroup ID: %d\n", entries_path, st.st_size, st.st_mode, st.st_nlink, st.st_uid, st.st_gid);

            //writing to the snapshot file the file_info & "\n" after each line
            write(snapshot_fd, file_info, strlen(file_info));                                               
            write(snapshot_fd, "\n", 1);   

            //if an entry is a directory => recursively call again the function with the new path  
            if(S_ISDIR(st.st_mode)){       
                read_directories(entries_path, snapshot_fd);
            }
        }
        free(entries_path);
        free(file_info);
        closedir(d);
    }
    else{
        fprintf(stderr, "*read_directories* error: Failed to open the directory: %s\n", dir_name);
        return;
    } 
}


/*
    CREATE SNAPSHOT FUNCTION IMPLEMENTATION
*/
void create_snapshot(char *path, char *output_path){

    char *dir_name=basename((char *)path);  //gets the name of the input directory
    char *output_dir_name=basename((char *)output_path); //gets the name of the output directory

    DIR *dir_check=opendir(path); //checking if the directory given as argument for monitoring   
                                  //exist. If the path is incorrect the error message will be printed to stderr 
                                  //and it will be skipped

    if(dir_check == NULL){
        fprintf(stderr, "*create_snapshots* error: The provided directory for monitoring does not exist:  %s\n", dir_name);
        return;
    }
    closedir(dir_check);

    dir_check=opendir(output_path); //checking if the output directory given as argument exists. If the path is incorrect
                                    //the snapshots does not have a place to be stored so the program exits.
    if(dir_check == NULL){
        fprintf(stderr, "*create_snapshots* error: The provided output directory does not exist:  %s\n", output_dir_name);
        exit(EXIT_FAILURE);
    }
    closedir(dir_check); 

    //buffer for storing the name of the snapshot file
    char snapshot_file_name[FILENAME_MAX];
  
    time_t now;                                         
    struct tm *timestamp;  
    char timestamp_str[32];

    time(&now); // --> gets the current time
    timestamp=localtime(&now);
    strftime(timestamp_str,sizeof(timestamp_str),"%Y.%m.%d_%H:%M:%S", timestamp);

    //constructing the snapshot file name with: output_path --> directory name --> snapshot number --> and timestamp
    snprintf(snapshot_file_name, sizeof(snapshot_file_name), "%s/%s_Snapshot_%s.txt", output_path, dir_name, timestamp_str);
  
    int snapshot_fd=open(snapshot_file_name, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR); 
    if(snapshot_fd == -1){
        fprintf(stderr, "*create_snapshots* error: Failed to open the snapshot file for:  %s\n", dir_name);
        return;
    }

    clock_t start=clock();  //getting the cpu time used for the read_directories function
    read_directories(path, snapshot_fd);
    clock_t end=clock();

    double time=(double)(end-start)/CLOCKS_PER_SEC;  
    char time_str[50];                                    //converting the time to string
    snprintf(time_str, sizeof(time_str), "Snapshot created in %g (s) for: ", time);   

    if(snapshot_fd != -1){ 
        fprintf(stderr, "Snapshot created successfully for: %s in %g (s)\n", dir_name, time);
    }

    close(snapshot_fd);
    get_previous_snapshot(output_path,dir_name, snapshot_file_name);
}


/*
    GET PREVIOUS SNAPSHOT FUNCTION IMPLEMENTATION
*/
void get_previous_snapshot(const char *output_path, const char *dir_name,const char *snapshot_file_name){

    DIR *d=opendir(output_path);
    struct dirent *dir_entry;
                                                                                      
    if(d){
        int prev_snapshot_no=0;
        int current_snapshot_no=1;
        char prev_snapshot_file_name[FILENAME_MAX];

        //parsing through all the snapshot files from the output that starts with
        //directory name that is monitored
        while((dir_entry = readdir(d)) != NULL){
            if(strstr(dir_entry->d_name, dir_name) == dir_entry->d_name && strstr(dir_entry->d_name, "_Snapshot_") != NULL){ 
                if(prev_snapshot_no < current_snapshot_no){
                    strcpy(prev_snapshot_file_name, output_path);    //--> constructing the name of the previous
                    strcat(prev_snapshot_file_name, "/");            //    snapshot file
                    strcat(prev_snapshot_file_name, dir_entry->d_name);
                }
                prev_snapshot_no++;   
            }
        }
    
        //if in the folder is not a previous snapshot => no comparison will be made
        if(prev_snapshot_no == 1) fprintf(stderr, "No snapshots were previously created for:  %s\n", dir_name);
        else compare_snapshots(prev_snapshot_file_name, snapshot_file_name, dir_name);
    }
    else fprintf(stderr, "*get_prev_snapshot* error: Failed to open the output directory:  %s\n", dir_name);
    closedir(d);
}


/*
    COMPARE SNAPSHOTS FUNCTION IMPLEMENTATION
*/
void compare_snapshots(const char *prev_snapshot_file_name, const char *current_snapshot_file_name, const char *dir_name){

    int snapshot_fd_current=open(current_snapshot_file_name, O_RDONLY, S_IRUSR); //opening the current snapshot file in reading mode
    if(snapshot_fd_current == -1){
        fprintf(stderr, "*compare_snapshots* error: Failed to open the current snapshot file for: %s\n", dir_name);
        return;
    }

    int snapshot_fd_prev=open(prev_snapshot_file_name, O_RDONLY, S_IRUSR); //opening the previous snapshot file in reading 
                                                                            //and writing mode
    if(snapshot_fd_prev == -1){
        fprintf(stderr, "*compare_snapshots* error: Failed to open the previous snapshot file for: %s\n", dir_name);
        return;
    }

    char current_line[MAX_LINE];
    char prev_line[MAX_LINE];

    int IsDifferent=0;
    
    ssize_t current_read, prev_read;

    do{
        current_read=read(snapshot_fd_current, current_line, sizeof(current_line)-1);
        prev_read=read(snapshot_fd_prev, prev_line, sizeof(prev_line)-1);

        current_line[current_read]='\0';
        prev_line[prev_read]='\0';

        if((current_read!=prev_read) || strcmp(current_line, prev_line)!=0){
            IsDifferent=1;
            break;
        }
    }
    while(current_read > 0 && prev_read > 0);

    if(!IsDifferent){   //in case no difference is found
        fprintf(stderr, "No differences found between the current and the previous snapshot for: %s\n", dir_name);
        unlink(prev_snapshot_file_name); //deleting the previous snapshot file name
        
    }
    else{
        fprintf(stderr, "Difference found between the current and the previous snapshot => overriding the previous snapshot for: %s\n", dir_name);
        unlink(prev_snapshot_file_name);  //deleting the previous snapshot file name
        rename(current_snapshot_file_name, prev_snapshot_file_name);  //renaming the current snapshot
    }

    close(snapshot_fd_current);
    close(snapshot_fd_prev);
}


int main(int argc, char *argv[]){

    write(STDERR_FILENO,"\n",1);

    if(argc<6){   // minimum 6 arguments because now I need "-o" and the output dir, "-s" and the isolated dir,
                  // the ./a.out and the rest of the paths to directories that will be monitored
        write(STDERR_FILENO, "error: Not enough arguments! => Exiting program!\n", strlen("error: Not enough arguments! => Exiting program!\n"));
        exit(EXIT_FAILURE);
    }

    char *output_path=NULL;  
    char *isolated_path=NULL;
    int o_count=0 ,s_count=0;

    //parsing through all the arguments for error handling
    for(int i=0;i<argc;i++){           
        if(strcmp(argv[i],"-o")==0){  
            o_count++;      
            output_path=argv[i+1];
        }
        else if(strcmp(argv[i],"-s")==0){
            s_count++;
            isolated_path=argv[i+1];
        }

        //checking if "-o" and "-s" arguments are consecutive
        if((strcmp(argv[i],"-o")==0 && i+1<argc && strcmp(argv[i+1],"-s")==0) || (strcmp(argv[i],"-s")==0 && i+1<argc && strcmp(argv[i+1],"-o")==0)){
            write(STDERR_FILENO, "error: Both \"-o\" and \"-s\" arguments cannot be provided consecutively! => Exiting program!\n", strlen("error: Both \"-o\" and \"-s\" arguments cannot be provided consecutively! => Exiting program!\n"));
            exit(EXIT_FAILURE);
        }

        //checking if argument "-o" or "-s" is not written in terminal
        if(o_count<1 && i+1==argc){  
            write(STDERR_FILENO,"error: The argument \"-o\" was not detected in the terminal! => Exiting program!\n", strlen("error: The argument \"-o\" was not detected in the terminal! => Exiting program!\n"));
            exit(EXIT_FAILURE);
        }
        if(s_count<1 && i+1==argc){  
            write(STDERR_FILENO,"error: The argument \"-s\" was not detected in the terminal! => Exiting program!\n", strlen("error: The argument \"-s\" was not detected in the terminal! => Exiting program!\n"));
            exit(EXIT_FAILURE);
        }

        //checking if argument "-o" or "-s" is more than once
        if(o_count>1){  
            write(STDERR_FILENO,"error: The argument \"-o\" was detected more than once in the terminal! => Exiting program!\n", strlen("error: The argument \"-o\" was detected more than once in the terminal! => Exiting program!\n"));
            exit(EXIT_FAILURE);
        }
        if(s_count>1){  
            write(STDERR_FILENO,"error: The argument \"-s\" was detected more than once in the terminal! => Exiting program!\n", strlen("error: The argument \"-s\" was detected more than once in the terminal! => Exiting program!\n"));
            exit(EXIT_FAILURE);
        }

        //checking if argument "-o" or "-s" is the last argument
        if(strcmp(argv[i],"-o")==0 && i+1==argc){ 
            write(STDERR_FILENO,"error: \"-o\" cannot be the last argument! => Exiting program!\n", strlen("error: \"-o\" cannot be the last argument! => Exiting program!\n"));
            exit(EXIT_FAILURE);
        }
        if(strcmp(argv[i],"-s")==0 && i+1==argc){ 
            write(STDERR_FILENO,"error: \"-s\" cannot be the last argument! => Exiting program!\n", strlen("error: \"-s\" cannot be the last argument! => Exiting program!\n"));
            exit(EXIT_FAILURE);
        }
    }

    pid_t pid;
    int count_processes=0;

    //parsing again through all the argument
    for(int i=1;i<argc;i++){  

        //basically if the argument is "-o" then the next one is the output directory so we skip them
        //the same for "-s" and the isolate directory
        if((strcmp(argv[i],"-o")==0 && i+1<argc) || (strcmp(argv[i],"-s")==0 && i+1<argc)) i++; 
       
        else{ //the rest of the arguments are directories that are monitored
            pid=fork();
            count_processes++;

            if(pid == 0){
                char *path = argv[i];              
                create_snapshot(path,output_path);   
                fprintf(stderr,"Child process %d terminated with PID %d and exit code %d for: %s \n", count_processes, getpid(), pid, basename((char *)path));
                return EXIT_SUCCESS;
            }
            else if(pid < 0){
                write(STDERR_FILENO,"error: fork() failed!\n",strlen("error: fork() failed!\n"));
                return EXIT_FAILURE;
            }
        }
    }

    for(int i=1;i<argc;i++){ 
        if((strcmp(argv[i],"-o")==0 && i+1<argc) || (strcmp(argv[i],"-s")==0 && i+1<argc)) i++;
        else{
            //write(STDERR_FILENO,"\n",1);
            wait(NULL);
        }
    }

    write(STDERR_FILENO,"\n",1);
    return EXIT_SUCCESS;
}
