/*Non-Canonical Input Processing*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <signal.h>

#define BAUDRATE B38400
#define MODEMDEVICE "/dev/ttyS1"
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

enum state{START, FLAG_RCV, A_RCV, C_RCV, BCC_OK, STOP};

struct termios oldtio, newtio;

#define MAX_TRIES 3

int numTries = 0;

void stateMachineUA(enum state* currentState, char byte){
    switch(*currentState){
        case START:
            if(byte == 0x7e)     //flag
                *currentState = FLAG_RCV;
            break;
        case FLAG_RCV:
            if(byte == 0x03)   //acknowlegement
                *currentState = A_RCV;
            else if(byte != 0x7e)
                *currentState = START;
            break;
        case A_RCV:
            if(byte == 0x07)
                *currentState = C_RCV;
            else if(byte == 0x7e){
                *currentState = FLAG_RCV;
            }
            else{
                *currentState = START;
            }
            break;
        case C_RCV:
            if(byte == (0x03^0x07))
              *currentState = BCC_OK;
            else if(byte == 0x7e)
              *currentState = FLAG_RCV;
            else
              *currentState = START;
            
            break;
        case BCC_OK:
            if(byte == 0x7e)
              *currentState = FLAG_RCV;
            else
              *currentState = START;
            break;
        case STOP:
            break;
    }
}

int readUA(int fd){
    char byte;
    enum state state = START;
    while(state != STOP){
        if(read(fd,&byte,1) < 1){
          perror("Error reading byte!");
        }
        stateMachineUA(&state,byte);
    }
}

void sigAlarmHandler(){
  printf("Alarm!");
  numTries++;
}

void initializeAlarm(){
  struct sigaction sa;
	sa.sa_handler = &sigAlarmHandler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;

	sigaction(SIGALRM, &sa, NULL);

  alarm(3); // need to define a macro for the time of the alarm

}

void disableAlarm(){
  struct sigaction sa;
	sa.sa_handler = NULL;

  sigaction(SIGALRM, &sa, NULL);

  alarm(0);
}


int llOpen(char *port,int flag){

    int fd;
    fd = open(port, O_RDWR | O_NOCTTY );
    if (fd <0) {perror(port); exit(-1); }

    if ( tcgetattr(fd,&oldtio) == -1) { /* save current port settings */
        perror("tcgetattr");
        exit(-1);
    }

    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    /* set input mode (non-canonical, no echo,...) */
    newtio.c_lflag = 0;

    newtio.c_cc[VTIME]    = 0;   /* inter-character timer unused */
    newtio.c_cc[VMIN]     = 5;   /* blocking read until 5 chars received */

    tcflush(fd, TCIOFLUSH);

    if ( tcsetattr(fd,TCSANOW,&newtio) == -1) {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");

    do{
       write(fd,0x03,sizeof(0x03));    
       alarm();
       readUA(fd);
    } while(numTries < MAX_TRIES);

    disableAlarm();

    if(numTries >= MAX_TRIES){
      printf("Exceed number of maximum tries!");
      return -1;
    }

    return fd;
}

//volatile int STOP=FALSE;

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


  /*
    Open serial port device for reading and writing and not as controlling tty
    because we don't want to get killed if linenoise sends CTRL-C.
  */


    fd = open(argv[1], O_RDWR | O_NOCTTY );
    if (fd <0) {perror(argv[1]); exit(-1); }

    if ( tcgetattr(fd,&oldtio) == -1) { /* save current port settings */
      perror("tcgetattr");
      exit(-1);
    }

    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    /* set input mode (non-canonical, no echo,...) */
    newtio.c_lflag = 0;

    newtio.c_cc[VTIME]    = 0;   /* inter-character timer unused */
    newtio.c_cc[VMIN]     = 5;   /* blocking read until 5 chars received */



  /* 
    VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a 
    leitura do(s) pr�ximo(s) caracter(es)
  */



    tcflush(fd, TCIOFLUSH);

    if ( tcsetattr(fd,TCSANOW,&newtio) == -1) {
      perror("tcsetattr");
      exit(-1);
    }

    printf("New termios structure set\n");



    for (i = 0; i < 255; i++) {
      buf[i] = 'a';
    }
    
    /*testing*/
    buf[25] = '\n';
    
    res = write(fd,buf,255);   
    printf("%d bytes written\n", res);
 

  /* 
    O ciclo FOR e as instru��es seguintes devem ser alterados de modo a respeitar 
    o indicado no gui�o 
  */



   
    if ( tcsetattr(fd,TCSANOW,&oldtio) == -1) {
      perror("tcsetattr");
      exit(-1);
    }




    close(fd);
    return 0;
}
