#include "aplication.h"


ApplicationLayer app;

int readFileInformation(char* fileName){

    int fd;
    struct stat status;

    if((fd = open(fileName,O_RDONLY)) < 0){
        perror("Error opening file!\n");
        return -1;
    }

    if(stat(fileName,&status) < 0){
        perror("Error reading file information!\n");
        return -1;
    }

    //app.fdFile = fd;
    app.fileName = fileName;
    app.fileSize = status.st_size;

    return 0;
}


int sendControlPacket(unsigned char controlByte){
    int packetIndex = 0;
    int numBytesFileSize = sizeof(app.fileSize);

    char packet[5+numBytesFileSize + strlen(app.fileName)];

    packet[packetIndex] = controlByte;
    packetIndex++;

    packet[packetIndex] = 0x01;  //flag nome do ficheiro
    packetIndex++;
    
    packet[packetIndex] = strlen(app.fileName);  // lenght do nome do ficheiro
    packetIndex++;

    for (size_t i = 0; i < strlen(app.fileName); i++)
    {
        packet[packetIndex] = app.fileName[i];
        packetIndex++;
    }

    packet[packetIndex] = 0x00;   //flag que indica o tamanho do ficheiro
    packetIndex++;

    packet[packetIndex] = numBytesFileSize;
    packetIndex++;

    packet[packetIndex] = (app.fileSize >> 24) & 0xFF;
    packetIndex++;

    packet[packetIndex] = (app.fileSize >> 16) & 0xFF;
    packetIndex++;

    packet[packetIndex] = (app.fileSize >> 8) & 0xFF;
    packetIndex++;

    packet[packetIndex] = app.fileSize & 0xFF;
    packetIndex++;

    if(llwrite(app.fdPort,packet,packetIndex) < packetIndex){
        printf("Error writing control packet to serial port!\n");
        return -1;
    }
    
    return 0;
}


int sendDataPacket(){

    int numPacketsSent = 0;
    int numPacketsToSend = app.fileSize/1024;            // numero máximo de de octetos num packet
    unsigned char buffer[1024];
    int bytesRead = 0;
    int length = 0;

    if(app.fileSize%1024 != 0){
        numPacketsToSend++;
    }

    while(numPacketsSent > numPacketsToSend){

        if((bytesRead = read(app.fdFile,buffer,1024)) < 0){
            printf("Error reading file\n");
        }
        char packet[4+1024];
        packet[0] = 0x01;
        packet[1] = numPacketsSent % 255;
        packet[2] = bytesRead / 256;
        packet[3] = bytesRead % 256;

        memcpy(&packet[4],buffer,bytesRead);
        length = bytesRead + 4;
        if(llwrite(app.fdPort,packet,length) < length){
            printf("Error writing data packet to serial port!\n");
            return -1;
        }

        numPacketsSent++;

    }

    return 0;
}

int sendFile(char* fileName){
    if(readFileInformation(fileName) < 0){
        printf("Error reading file information!\n");
    }

    if(sendControlPacket(0x02) < 0){
        printf("Error sending start control packet!\n");
        return -1;
    }

    if(sendDataPacket() < 0){
        printf("Error sending data packet!\n");
        return -1;
    }

    if(sendDataPacket(0x03) < 0){
        printf("Error sending end control packet!\n");
        return -1;
    }

    return 0;
}


int readControlPacket(){
    char packet[1024];
    int packetIndex = 1;
    int fileNameLength = 0;
    char* fileName;

    llread(app.fdPort,packet);


    if(packet[packetIndex] == 0x01){              //flag do nome do ficheiro
        packetIndex++;
        fileName = (char*) malloc(packet[packetIndex]+1);
        fileNameLength = packet[packetIndex];
        packetIndex++;

        for (size_t i = 0; i < fileNameLength; i++)
        {
            fileName[i] = packet[packetIndex];
            packetIndex++;

            if(i == fileNameLength-1){
                packet[packetIndex] = '\0';
            }
        }
        
    }

    app.fdFile = open(fileName,O_WRONLY | O_CREAT | O_APPEND);

    return 0;
}

int processDataPackets(char* packet){
    
    int informationSize = 256*packet[2]+packet[3];

    write(app.fdFile,&packet[4],informationSize);

    return 0;
}

int receiveFile(){
    char buffer[1024+4];
    int stop = 0;

    readControlPacket();

    while(!stop){
        if(llread(app.fdPort,buffer) != 0){
            if(buffer[0] == 0x01){
                processDataPackets(buffer);
            }
            else if(buffer[0] == 0x03){
                stop = 1;
            }
        }
    }

    close(app.fdFile);

    return 0;
}