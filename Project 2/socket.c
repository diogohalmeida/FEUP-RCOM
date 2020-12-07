#include "socket.h"


int openSocket(char* ipAddress, int port){
    int	sockfd;
	struct	sockaddr_in server_addr;

    bzero((char*)&server_addr,sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(ipAddress);	
	server_addr.sin_port = htons(port);		

    if ((sockfd = socket(AF_INET,SOCK_STREAM,0)) < 0) {
        perror("socket()");
        return -1;
    }
	/*connect to the server*/
    if(connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){
        perror("connect()");
        return -2;
	}

	return sockfd;
}