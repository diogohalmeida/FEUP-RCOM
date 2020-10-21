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

#define TRANSMITTER 0
#define RECEIVER 1

enum state{START, FLAG_RCV, A_RCV, C_RCV, BCC_OK, STOP};

struct termios oldtio, newtio;

#define MAX_TRIES 3

int numTries = 0;

int timerFlag = 0;

int verifyControlByte(char byte){
  return byte == 0x03 || byte == 0x0b || byte == 0x07 || byte == 0x05 || byte == 0x85 || byte == 0x81 || byte == 0x01;
}


void responseStateMachine(enum state* currentState, char byte, char* controlByte){
    switch(*currentState){
        case START:
            if(byte == 0x7E)     //flag
                *currentState = FLAG_RCV;
            break;
        case FLAG_RCV:
            if(byte == 0x03)   //acknowlegement
                *currentState = A_RCV;
            else if(byte != 0x7E)
                *currentState = START;
            break;
        case A_RCV:
            if(verifyControlByte(byte)){
              *currentState = C_RCV;
              *controlByte = byte;
            }
            else if(byte == 0x7E){
                *currentState = FLAG_RCV;
            }
            else{
                *currentState = START;
            }
            break;
        case C_RCV:
            if(byte == (0x03^(*controlByte)))
              *currentState = BCC_OK;
            else if(byte == 0x7E)
              *currentState = FLAG_RCV;
            else
              *currentState = START;
            
            break;
        case BCC_OK:
            if(byte == 0x7E)
              *currentState = STOP;
            else
              *currentState = START;
            break;
        case STOP:
            break;
    }
}

void readReceiverResponse(int fd){
  char byte;
  enum state state = START;
  char controlByte;
  while(state != STOP && timerFlag == 0){
      if(read(fd,&byte,1) < 0){
        perror("Error reading byte!");
      }
      responseStateMachine(&state,byte,&controlByte);
  }
}

void readTransmitterResponse(int fd){
  char byte;
  enum state state = START;
  char controlByte;
  while(state != STOP){
    if(read(fd,&byte,1) < 0){
      perror("Error reading byte!");
    }
    responseStateMachine(&state,byte,&controlByte);
  }
}

void sigAlarmHandler(){
  printf("Alarm!\n");
  timerFlag = 1;
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

    if (tcgetattr(fd,&oldtio) == -1) { /* save current port settings */
        perror("tcgetattr");
        exit(-1);
    }

    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    /* set input mode (non-canonical, no echo,...) */
    newtio.c_lflag = 0;

    newtio.c_cc[VTIME] = 0;   /* inter-character timer unused */
    newtio.c_cc[VMIN] = 1;   /* blocking read until 5 chars received */

    tcflush(fd, TCIOFLUSH);

    if (tcsetattr(fd,TCSANOW,&newtio) == -1) {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");

    if(flag == TRANSMITTER){
      char controlFrame[5];
      controlFrame[0] = 0x7E;
      controlFrame[1] = 0x03;
      controlFrame[2] = 0x03;
      controlFrame[3] = controlFrame[1] ^ controlFrame[2];
      controlFrame[4] = 0x7E;
      do{
       write(fd,controlFrame,5);  //write SET   
       timerFlag = 0;
       initializeAlarm();
       readReceiverResponse(fd);                    //read UA
      } while(numTries < MAX_TRIES && timerFlag);

      disableAlarm();

      if(numTries >= MAX_TRIES){
        printf("Exceed number of maximum tries!\n");
        return -1;
      }
    }

    else if(flag == RECEIVER){
      readTransmitterResponse(fd);
      char controlFrame[5];
      controlFrame[0] = 0x7E;
      controlFrame[1] = 0x03;
      controlFrame[2] = 0x07;
      controlFrame[3] = controlFrame[1] ^ controlFrame[2];
      controlFrame[4] = 0x7E;
      write(fd,controlFrame,5);  //send UA
    }

    return fd;
}


/*void stateMachineResponse(enum state* currentState, char byte, char* controlByte){
    switch(*currentState){
        case START:
            if(byte == 0x7E)     //flag
                *currentState = FLAG_RCV;
            break;
        case FLAG_RCV:
            if(byte == 0x03)   //acknowlegement
                *currentState = A_RCV;
            else if(byte != 0x7E)
                *currentState = START;
            break;
        case A_RCV:
            if(verifyControlByte(byte)){
              *currentState = C_RCV;
              *controlByte = byte;
            }
            else if(byte == 0x7E){
                *currentState = FLAG_RCV;
            }
            else{
                *currentState = START;
            }
            break;
        case C_RCV:
            if(byte == (0x03^0x07))
              *currentState = BCC_OK;
            else if(byte == 0x7E)
              *currentState = FLAG_RCV;
            else
              *currentState = START;
            
            break;
        case BCC_OK:
            if(byte == 0x7E)
              *currentState = FLAG_RCV;
            else
              *currentState = START;
            break;
        case STOP:
            break;
    }
}*/

/*int processControlByte(int fd, char *controlByte){
    char byte;
    enum state state = START;
    while(state != STOP){
        if(read(fd,&byte,1) < 1){
          perror("Error reading byte!");
        }
        stateMachineResponse(&state,byte,controlByte);
    }
    if(*controlByte == 0x05){
      return 0;
    }
    else if(*controlByte == 0x85){
      return 0;
    }
    else if(*controlByte == 0x01){
      return -1;
    }
    else if(*controlByte == 0x81){
      return -1;
    }
}*/

/*int llWrite(int fd, char* buffer, int length){
    int charactersWritten = 0;
    char controlByte;
    do{

      //write frame
      char frameToSend[2*length+6];
      int frameIndex = 4, frameLength = 0;
      char bcc2 = 0x00;

      //Start to make the frame to be sent
      frameToSend[0] = 0x7E;    //FLAG
      frameToSend[1] = 0x03;    //A
      if(ns == 0)
        frameToSend[2] = 0x00;
      else
      {
        frameToSend[2] = 0x40;
      }
      frameToSend[3] = frameToSend[1] ^ frameToSend[2]; 

      for (size_t i = 0; i < length; i++){
        bcc2 ^= buffer[i];
        if(buffer[i] == 0x7E || buffer[i] == 0x7D){   //if the byte its equal to the flag or to the escape byte
          frameToSend[frameIndex] = 0x7D;
          frameIndex++;
          frameToSend[frameIndex] = buffer[i] ^ 0x20;
          frameIndex++;
        }
        else{
          frameToSend[frameIndex] = buffer[i];
          frameIndex++;
        }
      }

      if(bcc2 == 0x7E || bcc2 == 0x7D){
        frameToSend[frameIndex] = 0x7D;
        frameIndex++;
        frameToSend[frameIndex] = bcc2 ^ 0x20;
      }
      else{
        frameToSend[frameIndex] = bcc2;
        frameIndex++;
      }

      frameToSend[frameIndex] = 0x7E;

      frameLength = frameIndex+1;

      charactersWritten = write(fd,frameToSend,frameLength);

      //read receiver response
      if(processControlByte(fd,&controlByte) == -1){  // if there is an error sending the message, send again 

      }


    }while(numTries < MAX_TRIES);
    
    

    return charactersWritten;
    
}*/

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


    /*fd = open(argv[1], O_RDWR | O_NOCTTY );
    if (fd <0) {perror(argv[1]); exit(-1); }

    if ( tcgetattr(fd,&oldtio) == -1) { // save current port settings
      perror("tcgetattr");
      exit(-1);
    }

    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;

    newtio.c_cc[VTIME]    = 0;   // inter-character timer unused 
    newtio.c_cc[VMIN]     = 5;   // blocking read until 5 chars received 



  /*
    VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a 
    leitura do(s) pr�ximo(s) caracter(es)
  */



    /*tcflush(fd, TCIOFLUSH);

    if ( tcsetattr(fd,TCSANOW,&newtio) == -1) {
      perror("tcsetattr");
      exit(-1);
    }

    printf("New termios structure set\n");



    for (i = 0; i < 255; i++) {
      buf[i] = 'a';
    }
    
    //testing
    buf[25] = '\n';
    
    res = write(fd,buf,255);   
    printf("%d bytes written\n", res);*/
 

  /* 
    O ciclo FOR e as instru��es seguintes devem ser alterados de modo a respeitar 
    o indicado no gui�o 
  */
  
  


    fd = llOpen(argv[1],TRANSMITTER);


   
    if ( tcsetattr(fd,TCSANOW,&oldtio) == -1) {
      perror("tcsetattr");
      exit(-1);
    }




    close(fd);
    return 0;
}
