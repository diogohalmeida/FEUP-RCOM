#include "ftp.h"

int ftpStartConnection(int* fdSocket, char * ipAddress, int port){

    char response[1024] = {0};

    if((*fdSocket = openSocket(ipAddress,port)) < 0){
        perror("Error opening socket!\n");
        return -1;
    }

    readSocketResponse(*fdSocket,response);

    printf("Response: %s\n",response);

    return 0;

}