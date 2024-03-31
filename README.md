# **Operating Systems Project**

## S1 Description

The proposed project combines functionalities for monitoring a directory to manage 
differences between two captures (snapshots) of it. The user will be able to observe and intervene 
in the changes in the monitored directory.

## Directory Monitoring

The user can specify the directory to be monitored as an argument in the command 
line, and the program will track changes occurring in it and its subdirectories, parsing recursively each 
entry from the directory. With each run of the program, the snapshot of the directory will be updated, 
storing the metadata of each entry.

## S1 Testing

I tested my implementation on my **Fedora** operating system by providing the path **/home/stikeez_/** 
in the terminal as argument.
