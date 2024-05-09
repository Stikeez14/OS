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

int count_processes=0; //counts the no. child procesess for each monitored directory
int count_grandchild_procesess=0; //counts the no. grandchild processes for each child process 
int count_corrupted=0; //counts the no. of files with potential danger


/*
    FUNCTION PROTOTYPES
*/
/*
    Parses through the directory (given as argument for monitoring) and his sub_directories recursively.
    Writes in the snapshot file the full path and other file information (allocated dinamically) for all entries.
*/
void Read_Directories(const char *path, int snapshot_fd, const char *dir_name, char *isolated_path);


/*
    Creates a snapshot file in the output directory. The name of the snapshot file contains the name of the monitored
    directory and a timestamp. This function calls read_directories (for parsing through the directory) and GetPreviousSnapshotThenCompare
    (for comparing the current snapshot with the previous one).
*/
void Create_Snapshot(char *path, char *output_path, char *isolated_path);


/*
    Parses through the files from the output directory that contains the name of the monitored dir and calls the
    compare_snapshots function if two snapshot are found. Otherwise, no comparation is made. 
    After comparation, if a difference is found, the previous snapshot is overriden.
*/
void GetPreviousSnapshotThenCompare(const char *output_path, const char *dir_name, const char *snapshot_file_name);


/*
    Compares the current snapshot (created when the program is executed) with the previous snapshot found in the output dir. 
    Returns 1 if a difference is found, 0 if the snapshot are identical and -1 in case of errors.
*/
int Compare_Snapshots(const char *prev_snapshot_file_name, const char *current_snapshot_file_name, const char *dir_name);


/*
    Checks if a directory entry given as parameter has all the access permissions missing and creates a new grandchild 
    process in that case for running the script and analyzing syntactically the dir entry.
*/
void CheckPermissionsAndAnalyze(const char *dir_entry, struct stat permissions, const char *dir_name, char *isolated_path, int snapshot_fd);


/*
    Performs the syntactic analysis for the files that have all the permissions missing by running the scripy 'verifiy_for_malicious'.
    The result of the analysis is written to the pipe (for transmitting information to the child process)
*/
int Analyze_File(const char *dir_entry, const char *dir_name, int pipe_fd);


/*
    Depending on the result transmitted through the pipe, this function takes the decision of moving the corrupted file
    to the isolated directory or not.
*/
void Result_Of_Analysis(int pipe_fd[2], const char *dir_entry, char *isolated_path, pid_t pid, const char *dir_name);


/*
    FUNCTION IMPLEMENTATIONS
*/
/*
    READ DIRECTORIES FUNCTION

    |  path == input directory path                                            |
    |  snapshot_fd == file descriptor                                          |
    |  dir_name == input directory name (without path) used only for printing  |
    |  isolated_path == isolated directory path                                |
*/
void Read_Directories(const char *path, int snapshot_fd, const char *dir_name, char *isolated_path){

    DIR *d = opendir(path);
    struct dirent *dir_entry;
    
    char *entries_path=NULL; //storing the path and name of each directory entry
    char *file_info=NULL;  //storing information for each directory entry

    char *current_dir_name=basename((char*)path); //from libgen, used for getting the name of the input directory

    if(!d){
        fprintf(stderr, "*read_directories* error: Failed to open the directory  \"%s\"\n", current_dir_name);
        return;
    }

    while((dir_entry = readdir(d)) != NULL){

        //not printing in the snapshot file the entries "." & ".." 
        if(strcmp(dir_entry->d_name, ".") == 0 || strcmp(dir_entry->d_name, "..") == 0)  continue;  
 
        entries_path=realloc(entries_path, strlen(path) + strlen(dir_entry->d_name) +2); //+2 is for '/' and null terminator
        if(entries_path == NULL){          
            fprintf(stderr, "*read_directories* error: Failed to allocate memory for path  \"%s\"\n", path);
            //if the allocation of memory fails, after the printed error
            //the loop will break => the directory will not be monitored anymore 
            free(entries_path);       
            break;
        }
        sprintf(entries_path, "%s/%s", path, dir_entry->d_name); //constructing the path of each entry

        struct stat st;                        //get file information with lstat      
        if(lstat(entries_path, &st) == -1){    //& print error message in case of failing     
            fprintf(stderr, "*read_directories* error: Failed to get information for path  \"%s\"\n", dir_entry->d_name);  
            free(entries_path);                                      
            break;   
        }
        else CheckPermissionsAndAnalyze(entries_path,st, dir_name, isolated_path, snapshot_fd);
              
        //gets the actual size of each line & used for reallocaating memory 
        size_t data_length = snprintf(NULL, 0, "Path: %s\nSize: %ld bytes\nAccess Rights: %c%c%c %c%c%c %c%c%c\nHard Links: %ld\n", entries_path, st.st_size, (st.st_mode & S_IRUSR) ? 'r' : '-', (st.st_mode & S_IWUSR) ? 'w' : '-', (st.st_mode & S_IXUSR) ? 'x' : '-', (st.st_mode & S_IRGRP) ? 'r' : '-', (st.st_mode & S_IWGRP) ? 'w' : '-', (st.st_mode & S_IXGRP) ? 'x' : '-' , (st.st_mode & S_IROTH) ? 'r' : '-', (st.st_mode & S_IWOTH) ? 'w' : '-', (st.st_mode & S_IXOTH) ? 'x' : '-', st.st_nlink);

        //reallocating memory for file_info then write in it entries_path & some additional data
        file_info=realloc(file_info, (data_length+1)); // +1 is for the null terminator
        if(file_info == NULL){
            fprintf(stderr, "*read_directories* error: Failed to allocate memory for entry  \"%s\"\n", dir_entry->d_name);
            free(file_info);
            break;
        }

        sprintf(file_info, "Path: %s\nSize: %ld bytes\nAccess Rights: %c%c%c %c%c%c %c%c%c\nHard Links: %ld\n", entries_path, st.st_size, (st.st_mode & S_IRUSR) ? 'r' : '-', (st.st_mode & S_IWUSR)? 'w' : '-', (st.st_mode & S_IXUSR) ? 'x' : '-', (st.st_mode & S_IRGRP) ? 'r' : '-', (st.st_mode & S_IWGRP) ? 'w' : '-', (st.st_mode & S_IXGRP) ? 'x' : '-' ,(st.st_mode & S_IROTH) ? 'r' : '-', (st.st_mode & S_IWOTH) ? 'w' : '-', (st.st_mode & S_IXOTH) ? 'x' : '-', st.st_nlink);

        //writing to the snapshot file the file_info & "\n" after each line
        write(snapshot_fd, file_info, strlen(file_info));                                               
        write(snapshot_fd, "\n", 1);   

        //if an entry is a directory => recursively call again the function with the new path  
        if(S_ISDIR(st.st_mode)) Read_Directories(entries_path, snapshot_fd, dir_name, isolated_path);
    }

    free(entries_path);
    free(file_info);
    closedir(d);     
}


/*
    CREATE SNAPSHOT FUNCTION

    |  path == input directory path              |
    |  output_path == output directory path      |
    |  isolated_path == isolated directory path  |       
*/
void Create_Snapshot(char *path, char *output_path, char *isolated_path){

    char *dir_name=basename((char *)path);  //gets the name of the input directory

    DIR *dir_check=opendir(path); //checking if the directory given as argument for monitoring   
                                  //exist. If the path is incorrect the error message will be printed to stderr 
                                  //and it will be skipped
    if(dir_check == NULL){
        fprintf(stderr, "*create_snapshots* error: The provided directory for monitoring does not exist  \"%s\"\n", dir_name);
        return;
    }
    closedir(dir_check);

    dir_check=opendir(output_path); //checking if the output directory given as argument exists. 
                                    //creates the output directory in case of non-existance
    if(dir_check == NULL) mkdir(output_path, 0777);
    closedir(dir_check); 

    dir_check=opendir(isolated_path); //checking if the isolated directory given as argument exists. 
                                      //creates the isolated directory in case of non-existance
    if(dir_check == NULL) mkdir(isolated_path, 0777);
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
        fprintf(stderr, "*create_snapshots* error: Failed to open the snapshot file for  \"%s\"\n", dir_name);
        return;
    }

    clock_t start=clock();  //getting the cpu time used for the read_directories function
    Read_Directories(path, snapshot_fd, dir_name, isolated_path);
    clock_t end=clock();

    double time=(double)(end-start)/CLOCKS_PER_SEC;  
   
    if(snapshot_fd != -1) fprintf(stdout, "(Creating) Snapshot created successfully for  \"%s\"  in %g (s)\n", dir_name, time);
    
    close(snapshot_fd);
    GetPreviousSnapshotThenCompare(output_path,dir_name, snapshot_file_name);
}


/*
    GET PREVIOUS SNAPSHOT THEN COMPARE FUNCTION

    |  output_path == output directory path                                                    |
    |  dir_name == input directory name (without path) used only for printing                  |
    |  snapshot_file_name == name of the current snapshot (output path + dir_name + timestamp) |
*/
void GetPreviousSnapshotThenCompare(const char *output_path, const char *dir_name,const char *snapshot_file_name){

    DIR *d=opendir(output_path);
    struct dirent *dir_entry;
                                                                                      
    if(!d){
        fprintf(stderr, "*get_prev_snapshot* error: Failed to open the output directory  \"%s\"\n", dir_name);
        return;
    }

    int prev_snapshot_no=0;
    int current_snapshot_no=1;
    char prev_snapshot_file_name[FILENAME_MAX];

    //parsing through all the snapshot files from the output that starts with
    //directory name that is monitored
    while((dir_entry = readdir(d)) != NULL){

        if(!(strstr(dir_entry->d_name, dir_name) == dir_entry->d_name && strstr(dir_entry->d_name, "_Snapshot_") != NULL)) continue;

        if(prev_snapshot_no < current_snapshot_no){
            strcpy(prev_snapshot_file_name, output_path);    //--> constructing the name of the previous
            strcat(prev_snapshot_file_name, "/");            //    snapshot file
            strcat(prev_snapshot_file_name, dir_entry->d_name);
        }
        prev_snapshot_no++;   
    }
    
    //if in the folder is not a previous snapshot => no comparison will be made
    if(prev_snapshot_no < 2){
        fprintf(stdout, "(Comparing) No snapshots were previously created for  \"%s\"\n", dir_name);
        return;
    }
        
    int IsDifferent=Compare_Snapshots(prev_snapshot_file_name, snapshot_file_name, dir_name);

    if(!IsDifferent){   //in case no difference is found
        fprintf(stdout, "(Comparing) No differences found between the current and the previous snapshot for  \"%s\"\n", dir_name);
        unlink(prev_snapshot_file_name); //deleting the previous snapshot file name
    }
    else{
        fprintf(stdout, "(Comparing) Difference found between the current and the previous snapshot => Overriding the previous snapshot for  \"%s\"\n", dir_name);
        unlink(prev_snapshot_file_name);  //deleting the previous snapshot file name
        rename(snapshot_file_name, prev_snapshot_file_name);  //renaming the current snapshot
    } 
    
    closedir(d);
}


/*
    COMPARE SNAPSHOTS FUNCTION

    |  dir_name == input directory name (without path) used only for printing                          |
    |  current_snapshot_file_name == name of the current snapshot (output path + dir_name + timestamp) |
    |  prev_snapshot_file_name == name of the previous snapshot (output path + dir_name + timestamp)   |
*/
int Compare_Snapshots(const char *prev_snapshot_file_name, const char *current_snapshot_file_name, const char *dir_name){

    int snapshot_fd_current=open(current_snapshot_file_name, O_RDONLY, S_IRUSR); //opening the current snapshot file in reading mode
    if(snapshot_fd_current == -1){
        fprintf(stderr, "*compare_snapshots* error: Failed to open the current snapshot file for  \"%s\"\n", dir_name);
        return -1;
    }

    int snapshot_fd_prev=open(prev_snapshot_file_name, O_RDONLY, S_IRUSR); //opening the previous snapshot file in reading 
                                                                            //and writing mode
    if(snapshot_fd_prev == -1){
        fprintf(stderr, "*compare_snapshots* error: Failed to open the previous snapshot file for  \"%s\"\n", dir_name);
        return -1;
    }

    char current_line[MAX_LINE]; //buffers for storing the lines of the  
    char prev_line[MAX_LINE];    //snapshots that are compared

    int IsDifferent=0; //flag
    
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

    close(snapshot_fd_current);
    close(snapshot_fd_prev);

    return IsDifferent;
}


/*
    CHECK PERMISSIONS AND ANALYZE FUNCTION

    |  dir_entry == input directory entry                                                              |
    |  permissions == stores file info retrieved with lstat (checks entries specific access rights)    |           
    |  dir_name == input directory name (without path) used only for printing                          |
    |  isolated_path == isolated directory path                                                        |  
*/
void CheckPermissionsAndAnalyze(const char *dir_entry, struct stat permissions, const char *dir_name, char *isolated_path, int snapshot_fd){

    pid_t pid;

    int pipe_fd[2];
    if(pipe(pipe_fd)==-1){
        write(STDERR_FILENO, "*check_permissions* error: pipe() failed!\n", strlen("*check_permissions* error: pipe() failed!\n"));
        return;
    }

    //checking if all the permissions are missing
    if(!(permissions.st_mode & S_IXUSR) && !(permissions.st_mode & S_IRUSR) && !(permissions.st_mode & S_IWUSR) && !(permissions.st_mode & S_IRGRP) && 
    !(permissions.st_mode & S_IWGRP) && !(permissions.st_mode & S_IXGRP) && !(permissions.st_mode & S_IROTH) && !(permissions.st_mode & S_IWOTH) && 
    !(permissions.st_mode & S_IXOTH)){     //if all of them are missing => syntactic analysis will be perfomed

        fprintf(stdout,"(Checking Permissions) \"%s\" from \"%s\" has no access rights => Performing Syntactic Anaysis!\n", basename((char *)dir_entry), dir_name);
        
        int file_status;
        pid=fork();
        count_grandchild_procesess++;

        if(pid==0){

            close(snapshot_fd); //closing because otherwise fork() will start the process of creating snapshots
            close(pipe_fd[0]);  //close read end of the pipe

            file_status=Analyze_File(dir_entry,dir_name, pipe_fd[1]);

            close(pipe_fd[1]);  //close write end of the pipe
            exit(file_status);

        }
        else if(pid < 0){
            write(STDERR_FILENO, "*check_permissions* error: fork() for child failed!\n", strlen("*check_permissions* error: fork() for child failed!\n"));
            return;
        }   
        else Result_Of_Analysis(pipe_fd, dir_entry, isolated_path, pid, dir_name);
        
        fprintf(stdout, "Grandchild Process %d.%d terminated with PID %d and exit code %d for file  \"%s\"  from  \"%s\"\n", count_processes, count_grandchild_procesess, getpid(), pid, basename((char *)dir_entry), dir_name);
        write(STDOUT_FILENO, "\n", 1);
    }
}


/*
    ANALYZE FILE FUNCTION

    |  dir_entry == input directory entry                                     |
    |  dir_name == input directory name (without path) used only for printing |
    |  pipe_fd == pipe for inter-process communication                        |
*/
int Analyze_File(const char *dir_entry, const char *dir_name, int pipe_fd){

    int give_access=chmod(dir_entry, S_IRUSR); //giving read access to the "malicious file"

    char argument[100];  //making the command to run the script
    snprintf(argument, sizeof(argument), "./verify_for_malicious.sh \"%s\"", dir_entry);
    
    int file_status = system(argument); //executing the script
    write(pipe_fd, &file_status, sizeof(file_status)); //writing to the pipe the result
    close(pipe_fd);

    give_access = chmod(dir_entry, 0); //changing again the acces rights to 0
    close(give_access);
        
    return file_status;
}


/*
    RESULT OF ANALYSIS FUNCTION

    |  pipe_fd == pipe for inter-process communication                        |  
    |  dir_entry == input directory entry                                     |
    |  output_path == output directory path                                   |
    |  pid == process id (used only for printing in this function)            |
    |  dir_name == input directory name (without path) used only for printing |  
*/
void Result_Of_Analysis(int pipe_fd[2], const char *dir_entry, char *isolated_path, pid_t pid, const char *dir_name){

    close(pipe_fd[1]); //close write end of the pipe
    int file_status;

    read(pipe_fd[0], &file_status, sizeof(file_status)); //read result from the pipe
    close(pipe_fd[0]); //close read end of the pipe

    if(file_status != 0){ //if status is 0, the file is safe, otherwise it will be moved to the isolated directory with rename function

        strcat(isolated_path, basename((char *)dir_entry));
        rename(dir_entry, isolated_path); 

        count_corrupted++;
        fprintf(stdout, "(Syntactic Analysis) \"%s\" from \"%s\" is malicious or corrupted => Moving it to the isolated directory!\n", basename((char *)dir_entry), dir_name);
    } 
    else fprintf(stdout, "(Syntactic Analysis) \"%s\" from \"%s\" is SAFE!\n", basename((char *)dir_entry), dir_name);

}


int main(int argc, char *argv[]){

    write(STDOUT_FILENO,"\n",1);

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
            write(STDERR_FILENO, "error: The argument \"-s\" was detected more than once in the terminal! => Exiting program!\n", strlen("error: The argument \"-s\" was detected more than once in the terminal! => Exiting program!\n"));
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
                Create_Snapshot(path, output_path, isolated_path);   
                fprintf(stdout,"Child Process %d terminated with PID %d and %d files with potential danger for  \"%s\"\n", count_processes, getpid(), count_corrupted, basename((char *)path));
                return EXIT_SUCCESS;
            }
            else if(pid < 0){
                write(STDERR_FILENO,"*main* error: fork() for child failed!\n", strlen("*main* error: fork() for child failed!\n"));
                return EXIT_FAILURE;
            }
        }
    }

    for(int i=1;i<argc;i++){ 
        if((strcmp(argv[i],"-o")==0 && i+1<argc) || (strcmp(argv[i],"-s")==0 && i+1<argc)) i++;
        else{
            write(STDOUT_FILENO,"\n",1);
            wait(NULL);
        }
    }

    write(STDOUT_FILENO,"\n",1);
    return EXIT_SUCCESS;
}
