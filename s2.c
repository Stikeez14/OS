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

/*
    FUNCTION PROTOTYPES
*/

//traverse the directory given as argument and his sub_directories recursively
//and writes in the snapshot file the path and the name of each entry 
void read_directories(const char *path, int snapshot_fd);

//creates a snapshot file in the output directory for each 
//directory given as argument in the terminal for monitoring.
void create_snapshot(char *path, char *output_path);

//counts the existing snapshots created for each monitored directory
//from the output directory. If no snapshot is found for one of the directories, it will return 0
int count_snapshots(const char *output_path,const char *dir_name);

//searches in the output directory all the snapshots that starts with the same directory name and calls 
//the compare_snapshots function if two snapshots are found. If there is only one, no comparation is made
void get_previous_snapshot(const char *output_path, const char *dir_name,const char *snapshot_file_name, int snp_no);

//used mostly for error handling and printing messages
void write_message(const char *message,const char* dir_name);

//compares the snapshot that is created when the program runs, with the snapshot that was previously created
//then overrides the previous snapshot with the information of the current
void compare_snapshots(const char *prev_snapshot_file_name,const char *snapshot_file_name,const char *dir_name);


/*
    READ DIRECTORIES FUNCTION IMPLEMENTATION
*/
void read_directories(const char *path, int snapshot_fd){

    DIR *d = opendir(path);
    struct dirent *dir_entry;
    
    char *new_path=NULL; 

    char *dir_name=basename((char*)path); //from libgen, used for getting the name of the input directory

    if(d){
        while((dir_entry = readdir(d)) != NULL){
            if(strcmp(dir_entry->d_name, ".") == 0 || strcmp(dir_entry->d_name, "..") == 0){
                continue; //all the directories have the entries "." & ".." 
                          //i'm not printing them in the Snapshot file. 
            }
            new_path=realloc(new_path,strlen(path)+strlen(dir_entry->d_name) +2); //+2 is for '/' and null terminator
            if(new_path==NULL){         
                write_message("*read_directories* error: Failed to allocate memory for path ",path);   
                        //if the allocation of memory fails, after the printed error
                        //the loop will break => the directory will not be monitored anymore        
                break;
            }

            sprintf(new_path,"%s/%s", path, dir_entry->d_name); //constructing the path
            //NEED TO COME BACK LATER AND GIVE MORE INFO  <-- !!

            struct stat st;                     //get file information with lstat      
            if(lstat(new_path, &st) == -1) {    //& print error message in case of failing     
                write_message("*read_directories* error: Failed to get information for ",dir_entry->d_name);                             
                free(new_path);              
                continue;    //THIS REQUIRE ANOTHER LOOK AT IT
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
        write_message("*read_directories* error: Failed to open the directory ",dir_name);
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

    if(dir_check==NULL){
        write_message("*create_snapshots* error: Failed to open the input directory ",dir_name);
        return;
    }
    closedir(dir_check);

    dir_check=opendir(output_path); //checking if the output directory given as argument exists. If the path is incorrect
                                    //the snapshots does not have a place to be stored so the program exits.
    if(dir_check==NULL){
        write_message("*create_snapshots* error: Failed to open the output directory ",output_dir_name);
        exit(EXIT_FAILURE);
    }
    closedir(dir_check); 

    char snapshot_file_name[FILENAME_MAX];
    int snapshots_no=count_snapshots(output_path,dir_name); // --> calls the function and gets the no. of existing snapshots 
                                                            // (starts counting from 0)
    time_t now;                                         
    struct tm *timestamp;  
    char timestamp_str[32];

    time(&now); // --> gets the current time
    timestamp=localtime(&now);
    strftime(timestamp_str,sizeof(timestamp_str),"%Y.%m.%d_%H:%M:%S", timestamp);

    //constructing the snapshot file name with: output_path --> directory name --> snapshot number --> and timestamp
    snprintf(snapshot_file_name, sizeof(snapshot_file_name), "%s/%s_Snapshot(%d)_%s.txt",output_path,dir_name,snapshots_no,timestamp_str);
  
    int snapshot_fd = open(snapshot_file_name, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR); 
    if(snapshot_fd == -1){
        write_message("*create_snapshots* error: Failed to open the snapshot file for ",dir_name);
        return;
    }

    clock_t start_1=clock();  //getting the cpu time used for the read_directories function
    read_directories(path, snapshot_fd);
    clock_t end_1=clock();

    double time_1=(double)(end_1-start_1)/CLOCKS_PER_SEC;  
    char time_str_1[20];                                    //converting the time to string
    sprintf(time_str_1,"--> created in %g (s)\n",time_1);   

    if(snapshot_fd != -1){ 
        write_message("Snapshot created successfully for ",dir_name);
        write(STDERR_FILENO,time_str_1,strlen(time_str_1));
    }

    // (MAYBE AN EXECUTION TIME TO SEE HOW MUCH IT TAKES OR AN PROGRESSION BAR IN THE FUTURE)

    close(snapshot_fd);
    get_previous_snapshot(output_path,dir_name,snapshot_file_name,snapshots_no);
}


/*
    COUNT SNAPSHOTS FUNCTION IMPLEMENTATION
*/
int count_snapshots(const char *output_path,const char *dir_name){
    
    int count=0;

    DIR *d=opendir(output_path);
    struct dirent *dir_entry;

    if(d){
        while((dir_entry=readdir(d))!=NULL){ 

           if(strstr(dir_entry->d_name, dir_name) == dir_entry->d_name &&  //cheks if the dir_name matches the beggining
                strstr(dir_entry->d_name, "_Snapshot(") != NULL) {         //of the snapshot file name & if it contains the substring 
                count++;
            }
        }
        closedir(d);
    }
    else{
        write_message("*count_snapshots* error: Failed to open the directory ",dir_name);
    }
    return count;
}


/*
    GET PREVIOUS SNAPSHOT FUNCTION IMPLEMENTATION
*/
void get_previous_snapshot(const char *output_path, const char *dir_name,const char *snapshot_file_name, int snp_no){

    DIR *d=opendir(output_path);
    struct dirent *dir_entry;
                                           
    if(d){
        int prev_snapshot_no=0;
        char prev_snapshot_file_name[FILENAME_MAX];

        //parsing through all the snapshot files from the output that starts with
        //directory name that is monitored
        while((dir_entry=readdir(d))!=NULL){
            if(strstr(dir_entry->d_name,dir_name)==dir_entry->d_name && strstr(dir_entry->d_name,"_Snapshot(")!=NULL){
                char *snp_no_str=strtok(dir_entry->d_name,"(");    //tokenized the name for obtaining the substring
                snp_no_str=strtok(NULL,"(");                       //containing the snapshot number (ex --> (2) )
                prev_snapshot_no=atoi(snp_no_str);   //convert the string to int

                if(prev_snapshot_no<snp_no){
                    strcpy(prev_snapshot_file_name,output_path);    //--> constructing the name of the previous
                    strcat(prev_snapshot_file_name,"/");            //    snapshot file
                    strcat(prev_snapshot_file_name,dir_entry->d_name);
                    strcat(prev_snapshot_file_name,"(");
                    strcat(prev_snapshot_file_name,snp_no_str);
                } 
            }
        }
    
        if(prev_snapshot_no<1){  //if in the folder is not a previous snapshot => no comparison will be made
            write_message("No snapshots were previously created for ",dir_name);
        }
        else{
            compare_snapshots(prev_snapshot_file_name,snapshot_file_name,dir_name);
        }
    }
    else{
        write_message("*get_prev_snapshot* error: Failed to open the output directory ",dir_name);
    }
    closedir(d);
}


/*
    COMPARE SNAPSHOTS FUNCTION IMPLEMENTATION
*/
void compare_snapshots(const char *prev_snapshot_file_name,const char *snapshot_file_name,const char *dir_name){

    int comparing=1;

    int snapshot_fd_current = open(snapshot_file_name, O_RDONLY, S_IRUSR); //opening the current snapshot file in reading mode
    if(snapshot_fd_current == -1){
        write_message("*compare_snapshots* error: Failed to open the current snapshot file for ",dir_name);
        return;
    }

    int snapshot_fd_prev = open(prev_snapshot_file_name, O_RDWR, S_IRUSR); //opening the previous snapshot file in reading 
                                                                               //and writing mode
    if(snapshot_fd_prev == -1){
        write_message("*compare_snapshots* error: Failed to open the previous snapshot file for ",dir_name);
        return;
    }

    char current_line[256];  //will store the lines into these two bubffers
    char prev_line[256];

    int line=1;
    char line_str[20];

    int IsDifferent=0;

    while(comparing){

        ssize_t current_read=read(snapshot_fd_current, current_line, sizeof(current_line)-1); //reading data from the snapshot 
        ssize_t prev_read=read(snapshot_fd_prev, prev_line, sizeof(prev_line)-1);             //file descriptors into the buffers 

        if(current_read==0 && prev_read==0){ //files reached EOF
            comparing=0;
            break;
        }
        if(current_read==0 || prev_read==0){ //if one of the files has less lines
                                             // => the files are different
        if(current_read==0){
            lseek(snapshot_fd_current, -prev_read, SEEK_CUR);  //positions the file pointer at the beginning of the line
                                                               // in the prev snapshot
                while(read(snapshot_fd_prev, prev_line, sizeof(prev_line)-1)>0){
                    write(snapshot_fd_current, prev_line, strlen(prev_line)); 
                }                                                                   // --> writes the lines of the file that
            } else {                                                                // has more lines in the prev snapshot
                while(read(snapshot_fd_current, current_line, sizeof(current_line)-1)>0){
                    write(snapshot_fd_prev, current_line, strlen(current_line));
                }
            }        
            IsDifferent=1;
        }

        if(strcmp(current_line, prev_line)!=0){   //if the current lines are not equal => files are diff
            sprintf(line_str,"%d",line);          // --> will override the previous snapshot
            lseek(snapshot_fd_prev, -prev_read, SEEK_CUR);  //
            write(snapshot_fd_prev, current_line, strlen(current_line)); //writing in the previous snapshot
            IsDifferent = 1;                                             //the line from the current snapshot
        }
        line++;
    }

    if(!IsDifferent){   //in case no difference is found
        write(STDERR_FILENO,"No differences found between this snapshot and the previous one!\n",
          strlen("No differences found between this snapshot and the previous one!\n"));
    }
    else{
        write_message("Differences found between this snapshot and the previous one at the line ",line_str);
        write_message("Overriding the previous snapshot: ",basename((char*)prev_snapshot_file_name));
    }

    close(snapshot_fd_current);
    close(snapshot_fd_prev);
}


/*
    WRITE MESSAGE FUNCTION IMPLEMENTATION
*/
void write_message(const char *message,const char* dir_name){
    write(STDERR_FILENO,message,strlen(message));
    write(STDERR_FILENO,dir_name,strlen(dir_name));
    write(STDERR_FILENO,"\n",1);
}


int main(int argc, char *argv[]){
    
    if(argc < 4){ // minimum 4 arguments because now I need "-o" and the output directory,
                  // the ./a.out and the rest of the paths to directories that will be monitored
        write(STDERR_FILENO, "error: Not enough arguments! => Exiting program!\n",
          strlen("error: Not enough arguments! => Exiting program!\n"));
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
                write(STDERR_FILENO,"error: The argument \"-o\" was not detected in the terminal! => Exiting program!\n",
                  strlen("error: The argument \"-o\" was not detected in the terminal! => Exiting program!\n"));
                exit(EXIT_FAILURE);
            }
            if(o_count>1){  
                write(STDERR_FILENO,"error: The argument \"-o\" was detected more than once in the terminal! => Exiting program!\n", 
                  strlen("error: The argument \"-o\" was detected more than once in the terminal! => Exiting program!\n"));
                exit(EXIT_FAILURE);
            }
            if(strcmp(argv[i],"-o")==0 && i+1==argc){
                write(STDERR_FILENO,"error: \"-o\" cannot be the last argument! => Exiting program!\n", 
                  strlen("error: \"-o\" cannot be the last argument! => Exiting program!\n"));
                exit(EXIT_FAILURE);
            }
        }

        for(int i=1;i<argc;i++){  //parsing again through all the argument
            if(strcmp(argv[i],"-o")==0 && i+1<argc){ //basically if the argument is "-o" then the next one
                                                     //is the output directory so we skip it
                i++;
            }
            else{
                write(STDERR_FILENO,"\n\n",2);
                char *path = argv[i];                //the rest of the arguments are directories
                create_snapshot(path,output_path);   //that are monotored
            }
        }
    }
    write(STDERR_FILENO,"\n",1);
    return EXIT_SUCCESS;
}