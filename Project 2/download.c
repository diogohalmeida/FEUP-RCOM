#include "ftp.h"


int main(int argc, char** argv){

    if(argc != 2){
        perror("Wrong number of arguments!\n");
        return -1;
    }

    urlInfo url;
    int fdSocket;
    int fdDataSocket;

    initializeUrlInfo(&url);

    if(parseUrlInfo(&url,argv[1]) < 0){
        printf("Error parsing url!\n");
        return -1;
    }

    if(getIpAddressFromHost(&url) < 0){
        printf("Error gettting IP Address!\n");
        return -1;
    }

    if(ftpStartConnection(&fdSocket,&url) < 0){
        printf("Error initializing connection!\n");
        return -1;
    }

    if(ftpLoginIn(&url,fdSocket) < 0){
        printf("Error while logging in!\n");
        return -1;
    }

    if(ftpPassiveMode(&url,fdSocket,&fdDataSocket) < 0){
        printf("Error entering in passive mode!\n");
        return -1;
    }

    if(ftpRetrieveFile(&url,fdSocket) < 0){
        printf("Error retrieving file!\n");
        return -1;
    }

    if(ftpDownloadAndCreateFile(&url,fdDataSocket) < 0){
        printf("Error downloading/creating file!\n");
        return -1;
    }

    close(fdSocket);

    return 0;

}