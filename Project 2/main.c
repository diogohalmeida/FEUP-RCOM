#include "url.h"
#include "ftp.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>



int main(int argc, char** argv){

    if(argc != 2){
        perror("Wrong number of arguments!\n");
        return -1;
    }

    urlInfo url;
    int fdSocket;

    initializeUrlInfo(&url);

    parseUrlInfo(&url,argv[1]);

    getIpAddressFromHost(&url);

    ftpStartConnection(&fdSocket,url.ipAddress,url.port);

    char userCommand[512] = {0};
    char passwordCommand[512] = {0};
    char responseToUserCommand[1024] = {0};
    char responseToPasswordCommand[1024] = {0};
    char responseToPassiveMode[1024] = {0};

    //log in

    sprintf(userCommand,"user %s\n",url.user);
    sprintf(passwordCommand,"pass %s\n",url.password);

    writeCommandToSocket(fdSocket,userCommand);
    
    readSocketResponse(fdSocket,responseToUserCommand);

    printf("Response: %s\n",responseToUserCommand);

    writeCommandToSocket(fdSocket,passwordCommand);

    readSocketResponse(fdSocket,responseToPasswordCommand);

    printf("Response: %s\n",responseToPasswordCommand);

    //enter passive mode
    writeCommandToSocket(fdSocket,"pasv\n");

    readSocketResponse(fdSocket,responseToPassiveMode);

    printf("Response: %s\n",responseToPassiveMode);

    int firstIpElement, secondIpElement, thirdIpElement, fourthIpElement;
    int firstPortElement, secondPortElement;

    char serverIp[1024] = {0};
    int serverPort;


    sscanf(responseToPassiveMode,"227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)",&firstIpElement,&secondIpElement,&thirdIpElement,&fourthIpElement,&firstPortElement,&secondPortElement);

    sprintf(serverIp,"%d.%d.%d.%d",firstIpElement,secondIpElement,thirdIpElement,fourthIpElement);

    serverPort = 256*firstPortElement+secondPortElement;

    int dataSocket = openSocket(serverIp,serverPort);

    char retrieveCommand[1024] = {0};
    char responseToRetrieve[1024] = {0};

    sprintf(retrieveCommand,"retr ./%s\n",url.urlPath);

    printf("%s\n",retrieveCommand);

    writeCommandToSocket(fdSocket,retrieveCommand);


    readSocketResponse(fdSocket,responseToRetrieve);

    printf("Response to Retrieve: %s\n",responseToRetrieve);

    int fd;
    int bytesRead;
    char buffer[1024];

    if((fd = open(url.fileName, O_WRONLY | O_CREAT, 0666)) < 0) {
		perror("open()\n");
		return 1;
	}

    while ((bytesRead = read(dataSocket, buffer, sizeof(buffer)))) {

		if (bytesRead < 0) {
			perror("read()\n");
			return 1;
		}

		if (write(fd, buffer, bytesRead) < 0) {
			perror("write()\n");
			return 1;
		}

	}

    close(fd);
    close(dataSocket);
    close(fdSocket);


    return 0;

}