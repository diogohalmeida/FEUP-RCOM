#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include "ll.h"

typedef struct{
    int fdFile;
    int fdPort;
    int fileSize;
    char* fileName; 

} ApplicationLayer;

int readFileInformation(char* fileName);


int sendControlPacket(unsigned char controlByte);

int sendDataPacket();

int sendFile(char* fileName, int fdPort);

int readControlPacket();

int processDataPackets(unsigned char* packet);

int receiveFile(int fdPort);

