#include "url.h"
#include "ftp.h"



int main(int argc, char** argv){

    if(argc != 2){
        perror("Wrong number of arguments!\n");
        return -1;
    }

    urlInfo url;
    int fdSocket;

    initializeUrlInfo(&url);

    parseUrlInfo(&url,argv[1]);

    ftpStartConnection(url.ipAddress,url.port);

    return 0;

}