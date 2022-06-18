The following file is the file I sent when sending the homework. Probably won't fix because im lazy lol

# IPC server and client for matrix multiplication
Author: Keremenci

Usage: Simply run both files and enter information when prompted.

The client prints out the result.

## Notes
I used mmap instead of shmget etc. for shared memory. msgsnd and msgrcv is used for control and shared memory is used for data transfer as requested. 

## Issues
- There is a memory leak caused by the mailbox that i didn't have the time to fix. 
- Also, the server does not handle SIGINT to gracefully exit on Ctrl-c input. I wish i had time to implement it but...
