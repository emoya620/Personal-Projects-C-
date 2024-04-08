// Below are some resources that I would like to cite:
// 1) I used the following resource to gat a better understanding of how to use the mmap() function: https://linuxhint.com/using_mmap_function_linux/
// 2) I used the following resource to get a better understanding of POSIX threads: https://www.geeksforgeeks.org/posix-threads-in-os/

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#define CHUNK_SIZE 4096

// Define the necessary global variables
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t taskCond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t encodingMutex =  PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t encodedCond = PTHREAD_COND_INITIALIZER;
char** taskQueue;
char** encodedChunkChars;
int numCharsArray[280000];
unsigned char** encodedChunkCounts;
int numTasks = 0;
int totalChunks;
int chunkNum = 0;
int totalNumChunks = 0;
int lastChunkSize = 0;
int numChunksCompleted = 0;

// Place a chunk into the taskQueue and update the number of tasks
void sendChunk(char* chunk){
    pthread_mutex_lock(&mutex);
    taskQueue[numTasks] = chunk;
    numTasks++;
    pthread_cond_signal(&taskCond);
    pthread_mutex_unlock(&mutex);
}

// Thread pool where threads will wait for 4kb chunks and then encode those chunks
void* threadPool(){
    while (1){

        // Take a task (chunk) from the task queue and update it. Also, increment the chunk number in order to maintain the proper order
        pthread_mutex_lock(&mutex);
        while (numTasks == 0){
            pthread_cond_wait(&taskCond, &mutex);
        }

        int curChunkNum = chunkNum;
        char* chunk = taskQueue[0];
        for (int i = 0; i < numTasks - 1; i++){
            taskQueue[i] = taskQueue[i+1];
        }
        numTasks--;
        chunkNum++;
        pthread_mutex_unlock(&mutex);

        // Define the necessary variables for encoding the chunk
        int charIndex = 0;
        int countIndex = 0;

        // Iterate through the chunk and encode it by updating the encodedChunkChars and encodedChunkCounts arrays
        int index = 0;
        int numChars = 0;
        unsigned int count = 0;
        char curChar = chunk[index];
        
        // If the current chunk is the last chunk from the file
        if (curChunkNum + 1 >= totalNumChunks){
            if (lastChunkSize == 0){
                lastChunkSize = CHUNK_SIZE;
            }

            while (index < lastChunkSize){
                if (curChar == chunk[index]){
                    count++;
                }
                else{
                    encodedChunkChars[curChunkNum][charIndex] = curChar;
                    charIndex++;
                    encodedChunkCounts[curChunkNum][countIndex] = count;
                    countIndex++;

                    count = 1;
                    curChar = chunk[index];
                    numChars++;
                }

                if (index + 1 == lastChunkSize){
                    encodedChunkChars[curChunkNum][charIndex] = curChar;
                    charIndex++;
                    encodedChunkCounts[curChunkNum][countIndex] = count;
                    countIndex++;
                    numChars++;
                }
                index++;
            }
        }
        // If the chunk from the file is not the last one
        else{
            while (index < CHUNK_SIZE){
                if (curChar == chunk[index]){
                    count++;
                }
                else{
                    encodedChunkChars[curChunkNum][charIndex] = curChar;
                    charIndex++;
                    encodedChunkCounts[curChunkNum][countIndex] = count;
                    countIndex++;

                    count = 1;
                    curChar = chunk[index];
                    numChars++;
                }

                if (index + 1 == CHUNK_SIZE){
                    encodedChunkChars[curChunkNum][charIndex] = curChar;
                    charIndex++;
                    encodedChunkCounts[curChunkNum][countIndex] = count;
                    countIndex++;
                    numChars++;
                }
                index++;
            }
        }
        // Store how many unique chars were encoded in a given chunk (needed for writing later)
        numCharsArray[curChunkNum] = numChars;

        // Update the number of chunks encoded, and if all chunks have been encoded, signal for the main thread to start writing
        pthread_mutex_lock(&encodingMutex);
        numChunksCompleted++;
        if (numChunksCompleted == totalChunks){
            pthread_cond_signal(&encodedCond);
        }
        pthread_mutex_unlock(&encodingMutex);
    }
}

// Main function that determines if the program should be multithreaded or should run sequentially
int main(int argc, char* argv[]) {

    // Allocate memory for the storage arrays
    taskQueue = (char**) malloc(270000 * sizeof(char*));
    encodedChunkChars = (char**) malloc(270000 * sizeof(char*));
    encodedChunkCounts = (unsigned char**) malloc(270000 * sizeof(unsigned char*));

    int chunksSent = 0;
    // If "-j jobs" is included in the command, create n jobs (based on "-j jobs"), else run sequentially 
    if (strcmp(argv[1], "-j") == 0){
        int numThreads = atoi(argv[2]);
        pthread_t threads[numThreads];

        // Create n threads
        for (int i = 0; i < numThreads; i++){
            pthread_create(&threads[i], NULL, &threadPool, NULL);
        }

        // Open a file and map it to memory
        int index = 3;
        while (argv[index] != NULL){
            int fd = open(argv[index], O_RDONLY);
            struct stat sb;
            fstat(fd, &sb);
            char* fileStr = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

            // Split the fileStr into 4kb chunks, which get sent into the task queue
            totalChunks = ceil((double) sb.st_size / CHUNK_SIZE);
            totalNumChunks += totalChunks;
            lastChunkSize = sb.st_size % CHUNK_SIZE;

            encodedChunkChars[chunksSent] = (char*) malloc(CHUNK_SIZE * sizeof(char));
            encodedChunkCounts[chunksSent] = (unsigned char*) malloc(CHUNK_SIZE * sizeof(unsigned char*));

            sendChunk(fileStr);
            chunksSent++;
            for (int i = 1; i < totalChunks; i++){
                encodedChunkChars[chunksSent] = (char*) malloc(CHUNK_SIZE * sizeof(char));
                encodedChunkCounts[chunksSent] = (unsigned char*) malloc(CHUNK_SIZE * sizeof(unsigned char*));

                char* chunk = fileStr + (i * CHUNK_SIZE);
                sendChunk(chunk);
                chunksSent++;
            }

            // Wait for all of the chunks to be encoded in the thread pool
            pthread_mutex_lock(&encodingMutex);
            while(numChunksCompleted != totalChunks){
                pthread_cond_wait(&encodedCond, &encodingMutex);
            }
            pthread_mutex_unlock(&encodingMutex);

            numChunksCompleted = 0;
            index++;
            close(fd);
        }

        // Write the encoded chunks to stdout
        int chunkIndex = 0;
        char finalChar;
        unsigned char finalCount;
        while (chunkIndex < totalNumChunks){
            int i = 0;
            // If the finalChar is equal to the first char of the next chunk, update the final count to the combination of the finalChar and firstChar
            if (chunkIndex > 0 && finalChar == encodedChunkChars[chunkIndex][0]){
                finalCount += encodedChunkCounts[chunkIndex][0];
                i++;
            }

            // Check to see if it is okay to write the finalChar and finalCount to stdout
            if ((numCharsArray[chunkIndex] > 1 || (numCharsArray[chunkIndex] == 1 && finalChar != encodedChunkChars[chunkIndex][0])) && chunkIndex > 0){
                write(1, &finalChar, 1);
                write(1, &finalCount, 1);
            }

            // Loop through the chars and counts in encoded arrays and write them to stdout, and store the finalChar and finalCount
            while (i < numCharsArray[chunkIndex]){
                if (i + 1 == numCharsArray[chunkIndex]){
                    finalChar = encodedChunkChars[chunkIndex][i];
                    finalCount = encodedChunkCounts[chunkIndex][i];
                    break;
                }

                write(1, &encodedChunkChars[chunkIndex][i], 1);
                write(1, &encodedChunkCounts[chunkIndex][i], 1);
                i++;
            }

            // If its the last chunk, then write finalChar and finalCount to stdout
            if (chunkIndex + 1 == totalNumChunks){
                write(1, &finalChar, 1);
                write(1, &finalCount, 1);
            }

            chunkIndex++;
        }
    }
    else {
        // Handle the scenario when "-j jobs" is not included. I will likely remove this once I get the thread pool working properly
        int numFilesOpened = 0;
        int numChars = 0;
        char chars[10000];
        char finalChar = -1;
        unsigned char counts[10000];
        int charIndex = 0;
        int countIndex = 0;
        while (numFilesOpened < argc - 1){
            // Open the files and read their contents to fileStr
            int fd = open(argv[numFilesOpened + 1], O_RDONLY);
            struct stat sb;
            fstat(fd, &sb);
            char* fileStr = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

            if (fileStr == MAP_FAILED){
                perror("Error");
                return 1;
            }

            // Declare variables necessary for encoding, and encode the fileStr
            int index = 0;
            unsigned int count = 0;
            char curChar = fileStr[index];
            while (index < sb.st_size){
                if (index == 0 && curChar == finalChar){
                    while (index < sb.st_size && fileStr[index] == finalChar){
                        counts[countIndex - 1]++;
                        index++;
                    }
                    curChar = fileStr[index];
                    continue;
                }

                if (curChar == fileStr[index]){
                    count++;
                }
                else{
                    chars[charIndex] = curChar;
                    charIndex++;
                    counts[countIndex] = count;
                    countIndex++;

                    count = 1;
                    curChar = fileStr[index];
                    numChars++;
                }

                if (index + 1 == sb.st_size){
                    chars[charIndex] = curChar;
                    charIndex++;
                    counts[countIndex] = count;
                    countIndex++;
                    finalChar = chars[charIndex - 1];
                    numChars++;
                }

                index++;
            }
            numFilesOpened++;
            munmap(NULL, sb.st_size);
            close(fd);
        }
        
        // Write the encoded fileStr to stdout
        for (int i = 0; i < numChars; i++){
            write(1, &chars[i], 1);
            write(1, &counts[i], 1);
        }
    }

    return 0;
}