// Below are some resources that I would like to cite:
// 1) I used this website to help get a better understanding of the sprintf() function: https://www.tutorialspoint.com/c_standard_library/c_function_sprintf.htm
// 2) I used this stack overflow page to get a better understanding of SHA-1 hashing: https://stackoverflow.com/questions/9284420/how-to-use-sha1-hashing-in-c-programming

// Include the necessary libraries
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <openssl/sha.h>

// Define a constant variable for the length of SHA-1
#define SHA_DIGEST_LENGTH 20

// Structure for boot sector
#pragma pack(push,1)
typedef struct BootEntry {
  unsigned char  BS_jmpBoot[3];     // Assembly instruction to jump to boot code
  unsigned char  BS_OEMName[8];     // OEM Name in ASCII
  unsigned short BPB_BytsPerSec;    // Bytes per sector. Allowed values include 512, 1024, 2048, and 4096
  unsigned char  BPB_SecPerClus;    // Sectors per cluster (data unit). Allowed values are powers of 2, but the cluster size must be 32KB or smaller
  unsigned short BPB_RsvdSecCnt;    // Size in sectors of the reserved area
  unsigned char  BPB_NumFATs;       // Number of FATs
  unsigned short BPB_RootEntCnt;    // Maximum number of files in the root directory for FAT12 and FAT16. This is 0 for FAT32
  unsigned short BPB_TotSec16;      // 16-bit value of number of sectors in file system
  unsigned char  BPB_Media;         // Media type
  unsigned short BPB_FATSz16;       // 16-bit size in sectors of each FAT for FAT12 and FAT16. For FAT32, this field is 0
  unsigned short BPB_SecPerTrk;     // Sectors per track of storage device
  unsigned short BPB_NumHeads;      // Number of heads in storage device
  unsigned int   BPB_HiddSec;       // Number of sectors before the start of partition
  unsigned int   BPB_TotSec32;      // 32-bit value of number of sectors in file system. Either this value or the 16-bit value above must be 0
  unsigned int   BPB_FATSz32;       // 32-bit size in sectors of one FAT
  unsigned short BPB_ExtFlags;      // A flag for FAT
  unsigned short BPB_FSVer;         // The major and minor version number
  unsigned int   BPB_RootClus;      // Cluster where the root directory can be found
  unsigned short BPB_FSInfo;        // Sector where FSINFO structure can be found
  unsigned short BPB_BkBootSec;     // Sector where backup copy of boot sector is located
  unsigned char  BPB_Reserved[12];  // Reserved
  unsigned char  BS_DrvNum;         // BIOS INT13h drive number
  unsigned char  BS_Reserved1;      // Not used
  unsigned char  BS_BootSig;        // Extended boot signature to identify if the next three values are valid
  unsigned int   BS_VolID;          // Volume serial number
  unsigned char  BS_VolLab[11];     // Volume label in ASCII. User defines when creating the file system
  unsigned char  BS_FilSysType[8];  // File system type label in ASCII
} BootEntry;
#pragma pack(pop)

// Structure for directory entry
#pragma pack(push,1)
typedef struct DirEntry {
  unsigned char  DIR_Name[11];      // File name
  unsigned char  DIR_Attr;          // File attributes
  unsigned char  DIR_NTRes;         // Reserved
  unsigned char  DIR_CrtTimeTenth;  // Created time (tenths of second)
  unsigned short DIR_CrtTime;       // Created time (hours, minutes, seconds)
  unsigned short DIR_CrtDate;       // Created day
  unsigned short DIR_LstAccDate;    // Accessed day
  unsigned short DIR_FstClusHI;     // High 2 bytes of the first cluster address
  unsigned short DIR_WrtTime;       // Written time (hours, minutes, seconds
  unsigned short DIR_WrtDate;       // Written day
  unsigned short DIR_FstClusLO;     // Low 2 bytes of the first cluster address
  unsigned int   DIR_FileSize;      // File size in bytes. (0 for directories)
} DirEntry;
#pragma pack(pop)

int main(int argc, char* argv[]){
    if (argc == 3 && strcmp(argv[2], "-i") == 0){
        // Open up disk specified in the command line and map it to memory
        int fd = open(argv[1], O_RDWR);
        struct stat sb;
        fstat(fd, &sb);
        struct BootEntry* diskImage;
        diskImage = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

        // Print out information about the file system specified in command line
        printf("Number of FATs = %d\n", diskImage->BPB_NumFATs);
        printf("Number of bytes per sector = %d\n", diskImage->BPB_BytsPerSec);
        printf("Number of sectors per cluster = %d\n", diskImage->BPB_SecPerClus);
        printf("Number of reserved sectors = %d\n", diskImage->BPB_RsvdSecCnt);
    }
    else if (argc == 3 &&  strcmp(argv[2], "-l") == 0){
        // Open up disk specified in the command line and map it to memory
        int fd = open(argv[1], O_RDWR);
        struct stat sb;
        fstat(fd, &sb);
        char* diskImage = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        struct BootEntry* bootSector = (BootEntry*) diskImage;

        // Declare necessary variables for looping through the directory entries
        struct DirEntry* rootDir;
        int entryNum = 0;
        int numEntries = 0;
        int clusterSize = bootSector->BPB_BytsPerSec * bootSector->BPB_SecPerClus;
        int clusterNum = bootSector->BPB_RootClus;
        int* FAT = (int*) (diskImage + (bootSector->BPB_RsvdSecCnt * bootSector->BPB_BytsPerSec));

        // Loop through all clusters of the root directory
        while (1) {
            // Locate the root directory cluster
            int rootDirSectors = ((bootSector->BPB_RootEntCnt * 32) + (bootSector->BPB_BytsPerSec - 1)) / bootSector->BPB_BytsPerSec;
            int firstDataSector = bootSector->BPB_RsvdSecCnt + (bootSector->BPB_NumFATs * bootSector->BPB_FATSz32) + rootDirSectors;
            int firstSectorOfCluster = ((clusterNum - 2) * bootSector->BPB_SecPerClus) + firstDataSector;

            // Loop through each directory entry in the given fat cluster of the root directory
            rootDir = (DirEntry*) (diskImage + (firstSectorOfCluster * bootSector->BPB_BytsPerSec) + (32 * entryNum));
            while (rootDir->DIR_Name[0] != 0x00 && entryNum * 32 < clusterSize){
                // If the root directory is deleted, then skip it
                if (rootDir->DIR_Name[0] == 0xE5){
                    entryNum++;
                    rootDir = (DirEntry*) (diskImage + (firstSectorOfCluster * bootSector->BPB_BytsPerSec) + (32 * entryNum));
                    continue;
                }

                // Declare the necessary variables to help print out the name of the file
                char* fName = malloc(11);
                memset(fName, '\0', 11);
                memcpy(fName, rootDir->DIR_Name, 11);
                int index = 0;
                while (index < 8){
                    if (fName[index] != 0x20){
                        printf("%c", fName[index]);
                    }
                    index++;
                }

                // Determine which type of file the entry is and print the appropriate message
                if (rootDir->DIR_FileSize > 0 && ((rootDir->DIR_FstClusHI << 16) | rootDir->DIR_FstClusLO) > 0){
                    index = 8;
                    if (fName[index] != 0x20){
                        printf(".");
                    }

                    while (index < 11){
                        if (fName[index] != 0x20){
                            printf("%c", fName[index]);
                        }
                        index++;
                    }
                    printf(" (size = %d, starting cluster = %d)\n", rootDir->DIR_FileSize, (rootDir->DIR_FstClusHI << 16) | rootDir->DIR_FstClusLO);
                }
                else if (((rootDir->DIR_FstClusHI << 16) | rootDir->DIR_FstClusLO) > 0){
                    printf("/ (starting cluster = %d)\n", (rootDir->DIR_FstClusHI << 16) | rootDir->DIR_FstClusLO);
                }
                else{
                    index = 8;
                    if (fName[index] != 0x20){
                        printf(".");
                    }

                    while (index < 11){
                        if (fName[index] != 0x20){
                            printf("%c", fName[index]);
                        }
                        index++;
                    }
                    printf(" (size = %d)\n", rootDir->DIR_FileSize);
                }
                
                entryNum++;
                numEntries++;
                rootDir = (DirEntry*) (diskImage + (firstSectorOfCluster * bootSector->BPB_BytsPerSec) + (32 * entryNum));
            }
            entryNum = 0;

            // Break the while loop if this is the last cluster of the root directory
            if (FAT[clusterNum] >= 0x0ffffff8){
                break;
            }

            clusterNum = FAT[clusterNum];
        }

        // Print the total number of entries in the root directory
        printf("Total number of entries = %d\n", numEntries);
    }
    else if (argc > 2 && strcmp(argv[2], "-r") == 0){
        // Open up disk specified in the command line and map it to memory
        int fileFound = -1;
        int usingSHA = -1;
        char firstChar = argv[3][0];
        int fd = open(argv[1], O_RDWR);
        struct stat sb;
        fstat(fd, &sb);
        char* diskImage = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        struct BootEntry* bootSector = (BootEntry*) diskImage;

        // Declare necessary variables for finding the deleted file
        struct DirEntry* rootDir;
        struct DirEntry* matchingDir = (DirEntry*) diskImage;
        int entryNum = 0;
        int numMatching = 0;
        unsigned char* fileContent;
        int firstSectorEntry;
        unsigned char buffer[SHA_DIGEST_LENGTH];
        unsigned char hash[SHA_DIGEST_LENGTH * 2];
        int clusterSize = bootSector->BPB_BytsPerSec * bootSector->BPB_SecPerClus;
        int clusterNum = bootSector->BPB_RootClus;
        int* FAT = (int*) (diskImage + (bootSector->BPB_RsvdSecCnt * bootSector->BPB_BytsPerSec));
        // int* FAT2 = (int*) (diskImage + (bootSector->BPB_RsvdSecCnt * bootSector->BPB_BytsPerSec) + (bootSector->BPB_FATSz32 * bootSector->BPB_BytsPerSec));

        // Loop through all clusters of the root directory
        while (1) {
            // Locate the root directory cluster
            int rootDirSectors = ((bootSector->BPB_RootEntCnt * 32) + (bootSector->BPB_BytsPerSec - 1)) / bootSector->BPB_BytsPerSec;
            int firstDataSector = bootSector->BPB_RsvdSecCnt + (bootSector->BPB_NumFATs * bootSector->BPB_FATSz32) + rootDirSectors;
            int firstSectorOfCluster = ((clusterNum - 2) * bootSector->BPB_SecPerClus) + firstDataSector;

            // Loop through each directory entry in the given fat cluster of the root directory
            rootDir = (DirEntry*) (diskImage + (firstSectorOfCluster * bootSector->BPB_BytsPerSec) + (32 * entryNum));
            while (rootDir->DIR_Name[0] != 0x00 && entryNum * 32 < clusterSize){
                // If the directory entry is not deleted, then skip it
                if (rootDir->DIR_Name[0] != 0xE5){
                    entryNum++;
                    rootDir = (DirEntry*) (diskImage + (firstSectorOfCluster * bootSector->BPB_BytsPerSec) + (32 * entryNum));
                    continue;
                }

                // Calculate the SHA-1 value of the file if necessary
                if (argc > 5 && strcmp(argv[4], "-s") == 0){
                    usingSHA = 0;
                    if (rootDir->DIR_FileSize > 0){
                        firstSectorEntry = ((rootDir->DIR_FstClusLO - 2) * bootSector->BPB_SecPerClus) + firstDataSector;
                        fileContent = (unsigned char*) (diskImage + (firstSectorEntry * bootSector->BPB_BytsPerSec));
                        SHA1(fileContent, rootDir->DIR_FileSize, buffer);

                        for (int i = 0; i < SHA_DIGEST_LENGTH; i++){
                            sprintf((char*) &(hash[i*2]), "%02x", buffer[i]);
                        }
                    }
                    else{
                        char* empty = "da39a3ee5e6b4b0d3255bfef95601890afd80709";

                        for (int i = 0; i < SHA_DIGEST_LENGTH * 2; i++){
                            sprintf((char*) &(hash[i]), "%c", empty[i]);
                        }
                    }
                }

                // Declare the necessary variables for finding the directory entry's name
                char* fName = malloc(11);
                memset(fName, '\0', 11);
                memcpy(fName, rootDir->DIR_Name, 11);
                char* fileName = malloc(11);
                fileName[0] = firstChar;
                int fileIndex = 1;
                int nameIndex = 1;
                while (fileIndex < 8){
                    if (fName[fileIndex] == 0x20){
                        fileIndex++;
                        continue;
                    }

                    fileName[nameIndex] = fName[fileIndex];
                    nameIndex++;
                    fileIndex++;
                }    

                if (fName[8] != 0x20 || fName[9] != 0x20 || fName[10] != 0x20) {
                    fileName[nameIndex] = '.';
                    nameIndex++;

                    while (fileIndex < 11){
                        if (fName[fileIndex] == 0x20){
                            fileIndex++;
                            continue;
                        }

                        fileName[nameIndex] = fName[fileIndex];
                        nameIndex++;
                        fileIndex++;
                    }
                }

                // If the entry file name matches the given file name, then indicate that a match has been found
                if (strcmp(fileName, argv[3]) == 0 && usingSHA == -1){
                    fileFound = 0;
                    matchingDir = rootDir;
                    numMatching++;
                }

                // If the SHA-1 value of the entry matches the SHA-1 value given, then indicate a file has been found and break from the loop
                if (usingSHA == 0 && strcmp((char*) hash, argv[5]) == 0 && strcmp(fileName, argv[3]) == 0){
                    fileFound = 0;
                    matchingDir = rootDir;
                    numMatching++;
                    break;
                }
                
                entryNum++;
                rootDir = (DirEntry*) (diskImage + (firstSectorOfCluster * bootSector->BPB_BytsPerSec) + (32 * entryNum));
            }
            entryNum = 0;

            // Break the loop if this is the last cluster of the root directory
            if (FAT[clusterNum] >= 0x0ffffff8){
                break;
            }

            clusterNum = FAT[clusterNum];
        }

        // Print whether or not the file was recovered, and print the necessary additional information
        if (fileFound != -1 && numMatching == 1){
            matchingDir->DIR_Name[0] = firstChar;
            int fileSize = matchingDir->DIR_FileSize;

            // Determine the number of clusters the recovered file occupies
            int numClusters;
            if (fileSize % clusterSize == 0){
                numClusters = fileSize / clusterSize;
            }
            else{
                numClusters = (fileSize / clusterSize) + 1;
            }

            // Update the FAT to indicate that the file is fully recovered
            int index = 0;
            int prevCluster = matchingDir->DIR_FstClusLO;
            while (index < numClusters && prevCluster != 0 && prevCluster != 1) {
                if (index + 1 == numClusters){
                    FAT[prevCluster] = EOF;
                    // FAT2[prevCluster] = EOF;
                }
                else{
                    FAT[prevCluster] = prevCluster + 1;
                }
                prevCluster = prevCluster + 1;
                index++;
            }

            if (usingSHA == 0){
                printf("%s: successfully recovered with SHA-1\n", argv[3]);
            }
            else{
                printf("%s: successfully recovered\n", argv[3]);
            }
        }
        else if (fileFound != -1 && numMatching > 1){
            printf("%s: multiple candidates found\n", argv[3]);
        }
        else{
            printf("%s: file not found\n", argv[3]);
        }
    }
    else if (argc > 5 && strcmp(argv[2], "-R") == 0 && strcmp(argv[4], "-s") == 0){
        int fileFound = -1;
        char firstChar = argv[3][0];
        int fd = open(argv[1], O_RDWR);
        struct stat sb;
        fstat(fd, &sb);
        char* diskImage = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        struct BootEntry* bootSector = (BootEntry*) diskImage;

        struct DirEntry* rootDir;
        int entryNum = 0;
        unsigned char* fileContent;
        int firstSectorEntry;
        unsigned char buffer[SHA_DIGEST_LENGTH];
        unsigned char hash[SHA_DIGEST_LENGTH * 2];
        int clusterSize = bootSector->BPB_BytsPerSec * bootSector->BPB_SecPerClus;
        int clusterNum = bootSector->BPB_RootClus;
        int* FAT = (int*) (diskImage + (bootSector->BPB_RsvdSecCnt * bootSector->BPB_BytsPerSec));
        
        while (1) {
            // Locate the root directory cluster
            int rootDirSectors = ((bootSector->BPB_RootEntCnt * 32) + (bootSector->BPB_BytsPerSec - 1)) / bootSector->BPB_BytsPerSec;
            int firstDataSector = bootSector->BPB_RsvdSecCnt + (bootSector->BPB_NumFATs * bootSector->BPB_FATSz32) + rootDirSectors;
            int firstSectorOfCluster = ((clusterNum - 2) * bootSector->BPB_SecPerClus) + firstDataSector;

            rootDir = (DirEntry*) (diskImage + (firstSectorOfCluster * bootSector->BPB_BytsPerSec) + (32 * entryNum));
            while (rootDir->DIR_Name[0] != 0x00 && entryNum * 32 < clusterSize){
                if (rootDir->DIR_Name[0] != 0xE5){
                    entryNum++;
                    rootDir = (DirEntry*) (diskImage + (firstSectorOfCluster * bootSector->BPB_BytsPerSec) + (32 * entryNum));
                    continue;
                }

                if (rootDir->DIR_FileSize > 0){
                    firstSectorEntry = ((rootDir->DIR_FstClusLO - 2) * bootSector->BPB_SecPerClus) + firstDataSector;
                    fileContent = (unsigned char*) (diskImage + (firstSectorEntry * bootSector->BPB_BytsPerSec));
                    SHA1(fileContent, rootDir->DIR_FileSize, buffer);

                    for (int i = 0; i < SHA_DIGEST_LENGTH; i++){
                        sprintf((char*) &(hash[i*2]), "%02x", buffer[i]);
                    }
                }
                else{
                    char* empty = "da39a3ee5e6b4b0d3255bfef95601890afd80709";

                    for (int i = 0; i < SHA_DIGEST_LENGTH * 2; i++){
                        sprintf((char*) &(hash[i]), "%c", empty[i]);
                    }
                }
                

                char* fName = malloc(11);
                memset(fName, '\0', 11);
                memcpy(fName, rootDir->DIR_Name, 11);

                char* fileName = malloc(11);
                fileName[0] = firstChar;
                int fileIndex = 1;
                int nameIndex = 1;
                while (fileIndex < 8){
                    if (fName[fileIndex] == 0x20){
                        fileIndex++;
                        continue;
                    }

                    fileName[nameIndex] = fName[fileIndex];
                    nameIndex++;
                    fileIndex++;
                }    

                if (fName[8] != 0x20 || fName[9] != 0x20 || fName[10] != 0x20) {
                    fileName[nameIndex] = '.';
                    nameIndex++;

                    while (fileIndex < 11){
                        if (fName[fileIndex] == 0x20){
                            fileIndex++;
                            continue;
                        }

                        fileName[nameIndex] = fName[fileIndex];
                        nameIndex++;
                        fileIndex++;
                    }
                }

                if (strcmp(fileName, argv[3]) == 0){
                    fileFound = 0;
                }

                if (strcmp((char*) hash, argv[5]) == 0 && strcmp(fileName, argv[3]) == 0){
                    fileFound = 0;
                    break;
                }
                
                entryNum++;
                rootDir = (DirEntry*) (diskImage + (firstSectorOfCluster * bootSector->BPB_BytsPerSec) + (32 * entryNum));
            }
            entryNum = 0;

            if (FAT[clusterNum] >= 0x0ffffff8){
                break;
            }

            clusterNum = FAT[clusterNum];
        }

        if (fileFound != -1){
            rootDir->DIR_Name[0] = firstChar;
            int fileSize = rootDir->DIR_FileSize;

            int numClusters;
            if (fileSize % clusterSize == 0){
                numClusters = fileSize / clusterSize;
            }
            else{
                numClusters = (fileSize / clusterSize) + 1;
            }

            int index = 0;
            int prevCluster = rootDir->DIR_FstClusLO;
            while (index < numClusters && prevCluster != 0 && prevCluster != 1) {
                if (index + 1 == numClusters){
                    FAT[prevCluster] = EOF;
                }
                else{
                    FAT[prevCluster] = prevCluster + 1;
                }
                prevCluster = prevCluster + 1;
                index++;
            }
            printf("%s: successfully recovered with SHA-1\n", argv[3]);
        }
        else{
            printf("%s: file not found\n", argv[3]);
        }
    }
    else{
        // Print the usage information of nyufile if the command is invalid
        printf("Usage: ./nyufile disk <options>\n");
        printf("  -i                     Print the file system information.\n");
        printf("  -l                     List the root directory.\n");
        printf("  -r filename [-s sha1]  Recover a contiguous file.\n");
        printf("  -R filename -s sha1    Recover a possibly non-contiguous file.\n");
    }
    return 0;
}
