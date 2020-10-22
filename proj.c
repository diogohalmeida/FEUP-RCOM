#include "ll.h"

int main(int argc, char** argv)
{
    if ( (argc < 3) || 
    ((strcmp("/dev/ttyS0", argv[1])!=0) && 
    (strcmp("/dev/ttyS1", argv[1])!=0) && 
    (strcmp("/dev/ttyS10", argv[1])!=0) &&  
    (strcmp("/dev/ttyS11", argv[1])!=0))) {
      printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
      exit(1);
    }

    int fd;
    int flag;
    char teste[3] = "ola";
    char final[3] = {0};

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

    if(flag == RECEIVER){
      llread(fd,final);
      printf("%s\n",final);
    }
    else if(flag == TRANSMITTER){
      llwrite(fd,teste,3);
    }

    llclose(fd,flag);

    return 0;
}
