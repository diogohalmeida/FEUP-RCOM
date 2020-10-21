#include "ll.h"


ConnectionInfo info;


int infoSetup(){
    info.alarmFlag = 0;
    info.numTries = 0;
    info.ns = 0;
}


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
  while(state != STOP && info.alarmFlag == 0){
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


int llopen(char *port,int flag){

    int fd;
    fd = open(port, O_RDWR | O_NOCTTY );
    if (fd <0) {perror(port); exit(-1); }

    if (tcgetattr(fd,&info.oldtio) == -1) { /* save current port settings */
        perror("tcgetattr");
        exit(-1);
    }

    bzero(&info.newtio, sizeof(info.newtio));
    info.newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    info.newtio.c_iflag = IGNPAR;
    info.newtio.c_oflag = 0;

    /* set input mode (non-canonical, no echo,...) */
    info.newtio.c_lflag = 0;

    info.newtio.c_cc[VTIME] = 0;   /* inter-character timer unused */
    info.newtio.c_cc[VMIN] = 1;   /* blocking read until 5 chars received */

    tcflush(fd, TCIOFLUSH);

    if (tcsetattr(fd,TCSANOW,&info.newtio) == -1) {
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
       info.alarmFlag = 0;
       initializeAlarm();
       readReceiverResponse(fd);                    //read UA
      } while(info.numTries < MAX_TRIES && info.alarmFlag);

      disableAlarm();

      if(info.numTries >= MAX_TRIES){
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


int llWrite(int fd, char* buffer, int length){
    int charactersWritten = 0;
    char controlByte;
    if(info.ns == 1)
      info.ns = 0;
    do{
      //write frame
      char frameToSend[2*length+6];
      int frameIndex = 4, frameLength = 0;
      char bcc2 = 0x00;

      //Start to make the frame to be sent
      frameToSend[0] = 0x7E;    //FLAG
      frameToSend[1] = 0x03;    //UA
      if(info.ns == 0)
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

      info.alarmFlag = 0;
      initializeAlarm();

      //read receiver response
      if(processControlByte(fd,&controlByte) == -1){  // if there is an error sending the message, send again 
        disableAlarm();
        info.alarmFlag = 1;
      }


    }while(info.numTries < MAX_TRIES && info.alarmFlag);
    
    disableAlarm();

    return charactersWritten;   
}

void informationFrameStateMachine(enum state* currentState, char byte, char* controlByte){
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
            if(byte == 0x00 || byte == 0x40){
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
            if(byte != 0x7E)
              *currentState = DATA_RCV;
            break;
        case DATA_RCV:
            if(byte == 0x7E)
              *currentState = STOP;
            break;
        case STOP:
            break;
    }
}


int readTransmitterFrame(int fd, char * buffer){
    int lenght = 0;
    char byte;
    char controlByte;
    enum state state = START;
    while(state != STOP){
      read(fd,&byte,1);
      informationFrameStateMachine(&state,byte,&controlByte);
      buffer[lenght] = byte;
      lenght++;
    }

    return lenght;
}

int llread(int fd,char* buffer){
  int received = 0;


  while(received == 0){




  }

}
