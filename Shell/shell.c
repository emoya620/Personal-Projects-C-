// Below are some resources that I would like to cite (I tried to use the man pages as much as possible):
// 1) I used this article to get a better understanding of the 'strtok_r' function: https://www.geeksforgeeks.org/strtok-strtok_r-functions-c-examples/
// 2) This article helped me better understand the 'getline' function: https://c-for-dummies.com/blog/?p=1112
// 3) This article was very useful when it came to better understanding files and I/O: https://user-web.icecube.wisc.edu/~dglo/c_class/stdio.html
// 4) This article helped me understand the 'chdir' function: https://www.geeksforgeeks.org/chdir-in-c-language-with-examples/
// 5) I used this article to better understand the 'memset' function: https://www.geeksforgeeks.org/memset-c-example/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

void handler(){
}

int main(){
    // Constants
    const char* invCmd = "invalid command";
    const char* invPrg = "invalid program";
    const char* invFile = "invalid file";
    const char* invDir = "invalid directory";
    const char* invJob = "invalid job";
    const char* suspJob = "there are suspended jobs";

    // Ignore SIGINT, SIGQUIT, and SIGTSTP signals
    signal(SIGINT, handler);
    signal(SIGQUIT, handler);
    signal(SIGTSTP, handler);

    // Variables for handling suspended jobs
    char* suspendedJobs[100][1000];
    int pidArray[100];
    int numSuspJobs = 0;
    int pidStatus;

    while (1){
        //Find the current working directory and print the prompt accordingly
        char* dir = (char*) malloc(sizeof(char) * 1000);
        dir = getcwd(dir, 1000);
        int length = strlen(dir);

        char* curFolder = malloc(sizeof(char) * length + 1);
        int index = length - 1;
        while (dir[index] != '/'){
            index--;
        }
        index++;
            
        int i = 0;
        while (dir[index] != '\0'){
            curFolder[i] = dir[index];
            index++;
            i++;
        }

        // Print the prompt
        if (strlen(dir) == 1){
            printf("[nyush /]$ ");
        }
        else{
            printf("[nyush %s]$ ", curFolder);
        }
        fflush(stdout);

        // This takes the user's command as an input using the getline() function.
        char* command;
        size_t cmdSize = 1000;

        command = (char*) malloc(sizeof(char) * cmdSize);
        int x = getline(&command, &cmdSize, stdin);

        // If stdin has been closed, then terminate the program.
        if (x == EOF){
            printf("\n");
            exit(0);
        }

        // Parsing the command line
        char* args[50];
        int argIndex = 0;
        char* curString;
        char* strPtr = NULL;

        // Use the strtok_r command to parse the command line and store each command in the args array.
        curString = strtok_r(command, " ", &strPtr);
        while (curString != NULL){
            args[argIndex] = curString;
            curString = strtok_r(NULL, " ", &strPtr);
            argIndex++;
        }
        args[argIndex] = NULL;
        // This line removes the "\n" character that is at the end of the last argument parsed from the command line
        args[argIndex-1][strlen(args[argIndex-1])-1] = 0;

        // Handle certain command errors
        if (strcmp(args[argIndex - 1], "<") == 0 || strcmp(args[argIndex - 1], ">") == 0 || strcmp(args[argIndex - 1], "|") == 0 || strcmp(args[0], "|") == 0){
            fprintf(stderr, "Error: %s\n", invCmd);
            continue;
        }

        // Execute the built-in command 'exit'
        if (strcmp(args[0], "exit") == 0){
            if (args[1] != NULL){
                fprintf(stderr, "Error: %s\n", invCmd);
            }
            else if (numSuspJobs > 0){
                fprintf(stderr, "Error: %s\n", suspJob);
            }
            else{
                exit(0);
            }
            continue;
        }

        // Execute the built-in command 'cd'
        if (strcmp(args[0], "cd") == 0){
            if (args[1] == NULL || args[2] != NULL){
                fprintf(stderr, "Error: %s\n", invCmd);
            }
            else if (args[1][0] == '/'){
                if (chdir(args[1]) != 0){
                    fprintf(stderr, "Error: %s\n", invDir);
                }
            }
            else if (args[1][0] == '.' && args[1][1] == '.'){
                chdir("../");
            }
            else {
                strcat(dir, "/");
                if (chdir(strcat(dir, args[1])) != 0){
                    fprintf(stderr, "Error: %s\n", invDir);
                }
            }
            continue;
        }

        // Execute the built in command 'jobs'
        if (strcmp(args[0], "jobs") == 0){
            if (args[1] != NULL){
                fprintf(stderr, "Error: %s\n", invCmd);
                continue;
            }
            
            if (numSuspJobs == 0){
                continue;
            }
            else{
                for (int i = 0; i < numSuspJobs; i++){
                    printf("[%d] ", i + 1);
                    for (int x = 0; suspendedJobs[i][x] != NULL; x++){
                        printf("%s", suspendedJobs[i][x]);

                        if (suspendedJobs[i][x + 1] != NULL){
                            printf(" ");
                        }
                    }
                    printf("\n");
                }
            }
            continue;
        }

        // Execute the built in command 'fg'
        if (strcmp(args[0], "fg") == 0){
            if (args[1] == NULL || args[2] != NULL){
                fprintf(stderr, "Error: %s\n", invCmd);
                continue;
            }
            
            int jobIndex = atoi(args[1]);
            if (jobIndex < 1 || jobIndex > numSuspJobs){
                fprintf(stderr, "Error: %s\n", invJob);
            }
            else{
                jobIndex--;
                kill(pidArray[jobIndex], SIGCONT);
                waitpid(pidArray[jobIndex], &pidStatus, WUNTRACED);

                // Update pidArray
                if (WIFSTOPPED(pidStatus)){
                    int temp = jobIndex;
                    int tempPid = pidArray[temp];
                    while (pidArray[temp + 1] != 0){
                        pidArray[temp] = pidArray[temp + 1];
                        temp++;
                    }
                    pidArray[temp] = tempPid;
                }
                else{
                    int temp = jobIndex;
                    while (pidArray[temp] != 0){
                        pidArray[temp] = pidArray[temp + 1];
                        temp++;
                    }
                }
                
                // Update suspendedJobs array
                if (numSuspJobs == 1){
                    if (WIFSTOPPED(pidStatus)){
                        continue;
                    }
                    else{
                        memset(suspendedJobs[0], 0, sizeof(suspendedJobs[0]));
                    }
                }
                else{
                    char* tempArgArray[1000];
                    int x = 0;
                    if (WIFSTOPPED(pidStatus)){
                        while (suspendedJobs[jobIndex][x] != NULL){
                            tempArgArray[x] = suspendedJobs[jobIndex][x];
                            x++;
                        }
                    }

                    while (jobIndex < numSuspJobs){
                        memset(suspendedJobs[jobIndex], 0, sizeof(suspendedJobs[jobIndex]));
                        
                        int j = 0;
                        while (suspendedJobs[jobIndex + 1][j] != NULL){
                            suspendedJobs[jobIndex][j] = suspendedJobs[jobIndex + 1][j];
                            j++;
                        }
                        jobIndex++;
                    }

                    if (WIFSTOPPED(pidStatus)){
                        x = 0;
                        while (tempArgArray[x] != NULL){
                            suspendedJobs[jobIndex-1][x] = tempArgArray[x];
                            x++;
                        }
                        continue;
                    }
                    else{
                        memset(suspendedJobs[jobIndex-1], 0, sizeof(suspendedJobs[jobIndex-1]));
                    }
                }
                numSuspJobs--;
            }
            continue;
        }

        // Count the number of pipes in the command
        index = 0;
        int numPipes = 0;
        while (args[index] != NULL){
            if (strcmp(args[index], "|") == 0){
                numPipes++;
            }
            index++;
        }

        // Create n pipes
        int pipefd_size = numPipes * 2;
        int pipefd[pipefd_size];
        if (numPipes > 0){
            for (int i = 0; i < numPipes; i++) {
                if (pipe(pipefd + (i * 2)) == -1){
                    fprintf(stderr, "An error occurred with the system call pipe()");
                    exit(1);
                }
            }
        }

        // Handle a command with n pipes
        index = 0;
        int isPiped = 0;
        int firstIndex = 0;
        int pid1 = -2;
        char* firstArgs[50];
        int curCommand = 0;
        if (numPipes > 0){
            while (curCommand < numPipes + 1){
                if (args[index] != NULL && strcmp(args[index], "|") != 0){
                    firstArgs[firstIndex] = args[index];
                    firstIndex++;
                    firstArgs[firstIndex] = NULL;
                }

                if (args[index] == NULL){
                    index--;
                }

                if (strcmp(args[index], "|") == 0 || args[index+1] == NULL){
                    isPiped = 1;
                    pid1 = -2;
                    int status;
                    int outputIndex;
                    int inputIndex;

                    // Fork a child and redirect its I/O
                    pid1 = fork();

                    if (pid1 == -1){
                        fprintf(stderr, "An error occurred with the system call fork()");
                        exit(1);
                    }

                    if (pid1 == 0){
                        outputIndex = (curCommand * 2) + 1;
                        inputIndex = (curCommand - 1) * 2;

                        if (curCommand != numPipes){
                            dup2(pipefd[outputIndex], 1);
                        }
                        
                        if (curCommand != 0){
                            dup2(pipefd[inputIndex], 0);
                        }

                        for (int i = 0; i < pipefd_size; i++){
                            close(pipefd[i]);
                        }

                        break;
                    }
                    
                    if (pid1 > 0){
                        outputIndex = (curCommand * 2) - 1;
                        inputIndex = (curCommand - 1) * 2;

                        if (curCommand == 0){
                            waitpid(pid1, &status, 0);
                        }
                        else if (curCommand > 0){
                            close(pipefd[outputIndex]);
                            waitpid(pid1, &status, 0);
                            close(pipefd[inputIndex]);
                        }

                        if (curCommand == numPipes){
                            break;
                        }
                    }
                    else{
                        fprintf(stderr, "There was an error with a fork() system call");
                        exit(1);
                    }
                }
                
                if (isPiped == 1){
                    firstIndex = 0;
                    curCommand++;
                }
                isPiped = 0;
                index++;
            }
        }

        // If there was pipes handles, then return parent to the beginning of the while loop
        if (pid1 > 0 && isPiped == 1){
            continue;
        }

        // Determine if I/O needs to be redirected
        index = 0;
        int arrIndex = 0;
        char* prevArgs[50];
        int outIndex = -1;
        int appIndex = -1;
        int inpIndex = -1;
        int isRedir = 0;
        if (pid1 == 0){
            while (firstArgs[index] != NULL){
                if (strcmp(firstArgs[index], ">") == 0){
                    outIndex = index;
                    isRedir = 1;
                }
                else if (strcmp(firstArgs[index], ">>") == 0){
                    appIndex = index;
                    isRedir = 1;
                }
                else if (strcmp(firstArgs[index], "<") == 0){
                    inpIndex = index;
                    isRedir = 1;
                }
                else if (isRedir == 0) {
                    prevArgs[arrIndex] = firstArgs[index];
                    arrIndex++;
                }
                index++;
            }
            prevArgs[arrIndex] = NULL;
        }
        else{
            while (args[index] != NULL){
                if (strcmp(args[index], ">") == 0){
                    outIndex = index;
                    isRedir = 1;
                }
                else if (strcmp(args[index], ">>") == 0){
                    appIndex = index;
                    isRedir = 1;
                }
                else if (strcmp(args[index], "<") == 0){
                    inpIndex = index;
                    isRedir = 1;
                }
                else if (isRedir == 0) {
                    prevArgs[arrIndex] = args[index];
                    arrIndex++;
                }
                index++;
            }
            prevArgs[arrIndex] = NULL;
        }

        // Fork the parent process in order to execute external programs
        int pid = -2;
        if (isPiped == 0){
            pid = fork();
        }

        if (pid == -1){
            printf("An error has occurred\n");
        }

        // Handle execution with I/O redirection
        if ((pid == 0 || isPiped == 1) && outIndex != -1){
            if (pid1 == 0){
                int fd = open(firstArgs[outIndex+1], O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR);
                dup2(fd, 1);
                close(fd);
            }
            else{
                int fd = open(args[outIndex+1], O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR);
                dup2(fd, 1);
                close(fd);
            }

            if (inpIndex != -1){
                int fd = open(args[inpIndex+1], O_RDONLY);
                dup2(fd, 0);
                close(fd);
            }

            char location[10] = "/usr/bin/";
            if (pid == 0 && execv(strcat(location, args[0]), prevArgs) == -1){
                fprintf(stderr, "Error: %s\n", invPrg);
            }
            else if (pid1 == 0 && execv(strcat(location, firstArgs[0]), prevArgs) == -1){
                fprintf(stderr, "Error: %s\n", invPrg);
            }
            exit(1);
        }
        else if ((pid == 0 || isPiped == 1) && appIndex != -1){
            if (pid1 == 0){
                int fd = open(firstArgs[appIndex+1], O_WRONLY|O_APPEND);
                dup2(fd, 1);
                close(fd);
            }
            else{
                int fd = open(args[appIndex+1], O_WRONLY|O_APPEND);
                dup2(fd, 1);
                close(fd);
            }

            if (inpIndex != -1){
                int fd = open(args[inpIndex+1], O_RDONLY);
                dup2(fd, 0);
                close(fd);
            }

            char location[10] = "/usr/bin/";
            if (pid == 0 && execv(strcat(location, args[0]), prevArgs) == -1){
                fprintf(stderr, "Error: %s\n", invPrg);
            }
            else if (pid1 == 0 && execv(strcat(location, firstArgs[0]), prevArgs) == -1){
                fprintf(stderr, "Error: %s\n", invPrg);
            }
            exit(1);
        }
        else if ((pid == 0 || isPiped == 1) && inpIndex != -1){
            strcat(dir, "/");
            strcat(dir, args[inpIndex+1]);
            int fileExists = access(dir, 0);
            if (fileExists == -1){
                fprintf(stderr, "Error: %s\n", invFile);
                exit(1);
            }

            if (pid1 == 0){
                int fd = open(firstArgs[inpIndex+1], O_RDONLY);
                dup2(fd, 0);
                close(fd);
            }
            else {
                int fd = open(args[inpIndex+1], O_RDONLY);
                dup2(fd, 0);
                close(fd);
            }

            if (outIndex != -1){
                int fd = open(args[outIndex+1], O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR);
                dup2(fd, 1);
                close(fd);
            }

            if (appIndex != -1){
                int fd = open(args[appIndex+1], O_WRONLY|O_APPEND);
                dup2(fd, 1);
                close(fd);
            }

            char location[10] = "/usr/bin/";
            if (pid == 0 && execv(strcat(location, args[0]), prevArgs) == -1){
                fprintf(stderr, "Error: %s\n", invPrg);
            }
            else if (pid1 == 0 && execv(strcat(location, firstArgs[0]), prevArgs) == -1){
                fprintf(stderr, "Error: %s\n", invPrg);
            }
            exit(1);
        }

        // Execute external programs
        if ((pid == 0 || isPiped == 1) && args[0][0] == '/'){
            if (pid == 0 && execv(args[0], args) == -1){
                fprintf(stderr, "Error: %s\n", invPrg);
            }
            else if (pid1 == 0 && execv(firstArgs[0], firstArgs) == -1){
                fprintf(stderr, "Error: %s\n", invPrg);
            }
            exit(1);
        }
        else if ((pid == 0 || isPiped == 1) && strchr(args[0], '/') == NULL){            
            char location[10] = "/usr/bin/";
            if (pid == 0 && execv(strcat(location, args[0]), args) == -1){
                fprintf(stderr, "Error: %s\n", invPrg);
            }
            else if (pid1 == 0 && execv(strcat(location, firstArgs[0]), firstArgs) == -1){
                fprintf(stderr, "Error: %s\n", invPrg);
            }
            exit(1);
        }
        else if ((pid == 0 || isPiped == 1) && strchr(args[0], '/') != NULL){
            char cmdRun[3] = "./";
            if (pid == 0 && execv(strcat(cmdRun, args[0]), args) == -1){
                fprintf(stderr, "Error: %s\n", invPrg);
            }
            else if (pid1 == 0 && execv(strcat(cmdRun, firstArgs[0]), firstArgs) == -1){
                fprintf(stderr, "Error: %s\n", invPrg);
            }
            exit(1);
        }
        
        int pidStatus = 0;
        if (pid > 0){
            waitpid(pid, &pidStatus, WUNTRACED);
           
            if (WIFSTOPPED(pidStatus)){
                int i = 0;
                while (args[i] != NULL){
                    suspendedJobs[numSuspJobs][i] = args[i];
                    i++;
                }
                suspendedJobs[numSuspJobs][i] = NULL;

                pidArray[numSuspJobs] = pid;
                numSuspJobs++;
            }

            continue;
        }
        
        // Free the previously allocated memory
        free(dir);
        free(curFolder);
        free(command);

        // Exit the child process if it somehow made it this far
        if (pid == 0){
            exit(0);
        }
    }
    return 0;
}
