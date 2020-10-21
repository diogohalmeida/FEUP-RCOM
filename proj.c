#include "alarm.h"

int main(int argc, char** argv)
{
    int fd,c, res;
    //struct termios oldtio,newtio;
    char buf[255];
    int i, sum = 0, speed = 0;
    
    if ( (argc < 2) || 
  	     ((strcmp("/dev/ttyS0", argv[1])!=0) && 
  	      (strcmp("/dev/ttyS1", argv[1])!=0) )) {
      printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
      exit(1);
    }

    fd = llopen(argv[1], TRANSMITTER);

    printf("ll open done\n");
    
    //llclose(fd, TRANSMITTER);

    return 0;
}
