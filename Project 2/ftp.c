#include "ftp.h"

int ftpStartConnection(int* fdSocket, urlInfo* url){

    char response[1024] = {0};

    if((*fdSocket = openSocket(url->ipAddress,url->port)) < 0){
        perror("Error opening socket!\n");
        return -1;
    }

    readSocketResponse(*fdSocket,response);

    printf("Response: %s\n",response);

    return 0;

}

int ftpLoginIn(urlInfo* url, int fdSocket){
    char userCommand[512] = {0};
    char passwordCommand[512] = {0};
    char responseToUserCommand[1024] = {0};
    char responseToPasswordCommand[1024] = {0};

    sprintf(userCommand,"user %s\n",url->user);
    sprintf(passwordCommand,"pass %s\n",url->password);

    if(writeCommandToSocket(fdSocket,userCommand) < 0){
        return -1;
    }
    
    if(readSocketResponse(fdSocket,responseToUserCommand) < 0){
        return -1;
    }

    printf("Response to the user: %s\n",responseToUserCommand);

    if(writeCommandToSocket(fdSocket,passwordCommand) < 0){
        return -1;
    }

    if(readSocketResponse(fdSocket,responseToPasswordCommand) < 0){
        return -1;
    }

    printf("Response to the password: %s\n",responseToPasswordCommand);

    return 0;
}

int ftpPassiveMode(urlInfo* url, int fdSocket, int* dataSocket){
    
    char responseToPassiveMode[1024] = {0};
    
    if(writeCommandToSocket(fdSocket,"pasv\n") < 0){
        return -1;
    }

    if(readSocketResponse(fdSocket,responseToPassiveMode) < 0){
        return -1;
    }

    printf("Response to the passive mode: %s\n",responseToPassiveMode);

    int firstIpElement, secondIpElement, thirdIpElement, fourthIpElement;
    int firstPortElement, secondPortElement;

    char serverIp[1024] = {0};
    int serverPort;


    sscanf(responseToPassiveMode,"227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)",&firstIpElement,&secondIpElement,&thirdIpElement,&fourthIpElement,&firstPortElement,&secondPortElement);

    sprintf(serverIp,"%d.%d.%d.%d",firstIpElement,secondIpElement,thirdIpElement,fourthIpElement);

    serverPort = 256*firstPortElement+secondPortElement;

    if((*dataSocket = openSocket(serverIp,serverPort)) < 0){
        return -1;
    }
  
    return 0;
}

int ftpRetrieveFile(urlInfo* url, int fdSocket){

    char retrieveCommand[1024] = {0};
    char responseToRetrieve[1024] = {0};

    sprintf(retrieveCommand,"retr ./%s\n",url->urlPath);

    if(writeCommandToSocket(fdSocket,retrieveCommand) < 0){
        return -1;
    }


    if(readSocketResponse(fdSocket,responseToRetrieve) < 0){
        return -1;
    }

    printf("Response to Retrieve Command: %s\n",responseToRetrieve);

    return 0;
}

int ftpDownloadAndCreateFile(urlInfo* url, int fdDataSocket){
    
    int fd;
    int bytesRead;
    char buffer[1024];

    if((fd = open(url->fileName, O_WRONLY | O_CREAT, 0666)) < 0) {
		perror("open()\n");
		return 1;
	}

    while ((bytesRead = read(fdDataSocket, buffer, sizeof(buffer)))) {

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
    close(fdDataSocket);

    return 0;
}
