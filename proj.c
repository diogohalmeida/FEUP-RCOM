#include "application.h"

int main(int argc, char** argv)
{
    if ( (argc < 5) || 
    ((strcmp("/dev/ttyS0", argv[1])!=0) && 
    (strcmp("/dev/ttyS1", argv[1])!=0) && 
    (strcmp("/dev/ttyS10", argv[1])!=0) &&  
    (strcmp("/dev/ttyS11", argv[1])!=0))) {
      printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
      exit(1);
    }

    int fd;
    int flag;
    int packetSize = atoi(argv[4]);

    if(strcmp("1",argv[2]) == 0){
      flag = RECEIVER;
    }
    else if(strcmp("0",argv[2]) == 0){
      flag = TRANSMITTER;
    }
    else{
      printf("Invalid argument for flag\n");
      exit(2);
    }
    
    fd = llopen(argv[1],flag);

    applicationSetUp(argv[3],packetSize,fd);

    infoSetup();

    if(fd > 0){
      if(flag == RECEIVER){
        receiveFile(fd);
      }
      else if(flag == TRANSMITTER){
        sendFile();
      }
      llclose(fd,flag);
    }
    
    return 0;
}
