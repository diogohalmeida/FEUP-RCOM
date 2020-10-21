#include "ll.h"


ConnectionInfo info;


int infoSetup(){
    info.alarmFlag = 0;
    info.numTries = 0;
    info.ns = 0;
}


int verifyControlByte(unsigned char byte){
  return byte == 0x03 || byte == 0x0b || byte == 0x07 || byte == 0x05 || byte == 0x85 || byte == 0x81 || byte == 0x01;
}

void responseStateMachine(enum state* currentState, unsigned char byte, unsigned char* controlByte){
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
  unsigned char byte;
  enum state state = START;
  unsigned char controlByte;
  while(state != STOP && info.alarmFlag == 0){
      if(read(fd,&byte,1) < 0){
        perror("Error reading byte!");
      }
      responseStateMachine(&state,byte,&controlByte);
  }
}

void readTransmitterResponse(int fd){
  unsigned char byte;
  enum state state = START;
  unsigned char controlByte;
  while(state != STOP){
    if(read(fd,&byte,1) < 0){
      perror("Error reading byte!");
    }
    responseStateMachine(&state,byte,&controlByte);
  }
}


int llopen(unsigned char *port,int flag){
    //Open the connection
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

    info.newtio.c_cc[VTIME] = 0;   /* inter-unsigned character timer unused */
    info.newtio.c_cc[VMIN] = 1;   /* blocking read until 5 unsigned chars received */

    tcflush(fd, TCIOFLUSH);

    if (tcsetattr(fd,TCSANOW,&info.newtio) == -1) {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");

    //First frames' transmission
    if(flag == TRANSMITTER){
      unsigned char controlFrame[5];
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
        printf("Exceeded number of maximum tries!\n");
        return -1;
      }
    }

    else if(flag == RECEIVER){
      readTransmitterResponse(fd);
      unsigned char controlFrame[5];
      controlFrame[0] = 0x7E;
      controlFrame[1] = 0x03;
      controlFrame[2] = 0x07;
      controlFrame[3] = controlFrame[1] ^ controlFrame[2];
      controlFrame[4] = 0x7E;
      write(fd,controlFrame,5);  //send UA
    }

    else{
        printf("Unknown function, must be a TRANSMITTER/RECEIVER\n");
        return 1;
    }

    return fd;
}

int processControlByte(int fd, unsigned char *controlByte){
    unsigned char byte;
    enum state state = START;
    while(state != STOP){
        if(read(fd,&byte,1) < 1){
          perror("Error reading byte!");
        }
        responseStateMachine(&state,byte,controlByte);
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
}


int llwrite(int fd, unsigned char* buffer, int length){
    int unsigned charactersWritten = 0;
    unsigned char controlByte;
    if(info.ns == 1)
      info.ns = 0;
    do{
      //write frame
      unsigned char frameToSend[2*length+6];
      int frameIndex = 4, frameLength = 0;
      unsigned char bcc2 = 0x00;

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

      unsigned charactersWritten = write(fd,frameToSend,frameLength);

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

void informationFrameStateMachine(enum state* currentState, unsigned char byte, unsigned char* controlByte){
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


int readTransmitterFrame(int fd, unsigned char * buffer){
    int lenght = 0;
    unsigned char byte;
    unsigned char controlByte;
    enum state state = START;
    while(state != STOP){
      read(fd,&byte,1);
      informationFrameStateMachine(&state,byte,&controlByte);
      buffer[lenght] = byte;
      lenght++;
    }

    return lenght;
}

int verifyFrame(unsigned char* frame,int length){
  unsigned char addressField = frame[1];
  unsigned char controlByte = frame[2];
  unsigned char bcc1 = frame[3];
  unsigned char bcc2 = frame[length-2];
  unsigned char aux = 0x00;

  //verify if the bcc1 is correct
  if(controlByte != 0x00 && controlByte != 0x40){
    printf("Error in control byte!\n");
    return -1;
  }
  else if(bcc1 == (addressField^controlByte)){
    //check bcc2
    for (size_t i = 4; i < length-2; i++)
    {
      aux^=frame[i];
    }
    if(bcc2 != aux){
      printf("Error in bcc2!\n");
      return -2;
    }
  }

  return 0;
}

int llread(int fd,unsigned char* buffer){
  int received = 0;
  int length = 0;
  unsigned char controlByte;
  unsigned char auxBuffer[2500];
  int buffIndex = 0;
  while(received == 0){
    length = readTransmitterFrame(fd,auxBuffer);

    if(length > 0){
      unsigned char originalFrame[2*length+6];
      //destuffing
      originalFrame[0] = auxBuffer[0];
      originalFrame[1] = auxBuffer[1];
      originalFrame[2] = auxBuffer[2];
      originalFrame[3] = auxBuffer[3];
      int originalFrameIndex = 4;
      int escapeByteFound = 0;

      for (size_t i = 4; i < length-1; i++)
      {
        if(auxBuffer[i] == 0x7D){
          escapeByteFound = 1;
          continue;
        }
        else if(auxBuffer[i] == (0x7E^0x20) && escapeByteFound == 1){
          originalFrame[originalFrameIndex] = auxBuffer[i];
          originalFrameIndex++;
          escapeByteFound = 0;
        }
        else if(auxBuffer[i] == (0x7D^0x20) && escapeByteFound == 1){
          originalFrame[originalFrameIndex] = auxBuffer[i];
          originalFrameIndex++;
          escapeByteFound = 0;
        }
        else{
          originalFrame[originalFrameIndex] = auxBuffer[i];
          originalFrameIndex++;
        }
      }
      originalFrame[originalFrameIndex] = auxBuffer[length-1];
      controlByte = originalFrame[2];

      if(verifyFrame(originalFrame,originalFrameIndex+1) != 0){
        if(controlByte == 0x00){
          unsigned char frameToSend[5];
          frameToSend[0] = 0x7E;
          frameToSend[1] = 0x03;
          frameToSend[2] = 0x01;
          frameToSend[3] = frameToSend[1] ^ frameToSend[2];
          frameToSend[4] = 0x7E;

          write(fd,frameToSend,5);
        }
        else if(controlByte == 0x40){
          unsigned char frameToSend[5];
          frameToSend[0] = 0x7E;
          frameToSend[1] = 0x03;
          frameToSend[2] = 0x81;
          frameToSend[3] = frameToSend[1] ^ frameToSend[2];
          frameToSend[4] = 0x7E;

          write(fd,frameToSend,5);
        }
        return 0;
      }
      else{
        for (size_t i = 4; i < originalFrameIndex-1; i++)
        {
          buffer[buffIndex] = originalFrame[i];
          buffIndex++;
        }
        if(controlByte == 0x00){
          unsigned char frameToSend[5];
          frameToSend[0] = 0x7E;
          frameToSend[1] = 0x03;
          frameToSend[2] = 0x85;
          frameToSend[3] = frameToSend[1] ^ frameToSend[2];
          frameToSend[4] = 0x7E;

          write(fd,frameToSend,5);
        }
        else if(controlByte == 0x40){
          unsigned char frameToSend[5];
          frameToSend[0] = 0x7E;
          frameToSend[1] = 0x03;
          frameToSend[2] = 0x05;
          frameToSend[3] = frameToSend[1] ^ frameToSend[2];
          frameToSend[4] = 0x7E;

          write(fd,frameToSend,5);
        }
        received = 1;
      }
    }
  }
  return buffIndex;
}

int llclose(int fd, int flag){
    //Last frames' transmission
    if(flag == TRANSMITTER){
        unsigned char controlFrameDISC[5];
        controlFrameDISC[0] = FLAG;
        controlFrameDISC[1] = A_CMD;
        controlFrameDISC[2] = C_DISC;
        controlFrameDISC[3] = controlFrameDISC[1] ^ controlFrameDISC[2];
        controlFrameDISC[4] = FLAG;

        do{
            write(fd,controlFrameDISC,5);  //write DISC 
            info.alarmFlag = 0;
            initializeAlarm();
            readReceiverResponse(fd);  //read DISC
        } while(info.numTries < MAX_TRIES && info.alarmFlag);
        
        disableAlarm();

        if(info.numTries >= MAX_TRIES){
            printf("Exceeded number of maximum tries!\n");
            return -1;
        }
        else{
            unsigned char controlFrameUA[5];
            controlFrameUA[0] = FLAG;
            controlFrameUA[1] = A_CMD;
            controlFrameUA[2] = C_UA;
            controlFrameUA[3] = controlFrameUA[1] ^ controlFrameDISC[2];
            controlFrameUA[4] = FLAG;

            write(fd,controlFrameUA,5); //write UA after receiving DISC
            sleep(1);
        }
    }

    else if(flag == RECEIVER){
        readTransmitterResponse(fd);
        unsigned char controlFrame[5];
        controlFrame[0] = FLAG;
        controlFrame[1] = A_CMD;
        controlFrame[2] = C_DISC;
        controlFrame[3] = controlFrame[1] ^ controlFrame[2];
        controlFrame[4] = FLAG;
        write(fd,controlFrame,5);  //send DISC

        readTransmitterResponse(fd);
    }

    else{
        printf("Unknown function, must be a TRANSMITTER/RECEIVER\n");
        return 1;
    }
    //Close the connection

    tcflush(fd, TCIOFLUSH);

	if (tcsetattr(fd, TCSANOW, &info.oldtio) == -1) {
		perror("tcsetattr");
		exit(-1);
	}

	close(fd);

    return 0;
}