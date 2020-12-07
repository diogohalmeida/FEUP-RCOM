#include "ftp.h"

int ftpStartConnection(char * ipAddress, int port){

    int fdSocket;

    if((fdSocket = openSocket(ipAddress,port)) < 0){
        perror("Error opening socket!\n");
        return -1;
    }

    return 0;

}