#include "ll.h"


ConnectionInfo info;


void infoSetup(){
    info.alarmFlag = 0;
    info.numTries = 0;
    info.ns = 0;    
}


int verifyControlByte(unsigned char byte){
  return byte == CONTROL_BYTE_SET || byte == CONTROL_BYTE_DISC || byte == CONTROL_BYTE_UA || byte == CONTROL_BYTE_RR0 || byte == CONTROL_BYTE_RR1 || byte == CONTROL_BYTE_REJ0 || byte == CONTROL_BYTE_REJ1;
}


void responseStateMachine(enum state* currentState, unsigned char byte, unsigned char* controlByte){
    switch(*currentState){
        case START:
            if(byte == FLAG){    //flag
                *currentState = FLAG_RCV;
            }
            break;
        case FLAG_RCV:
            if(byte == ADDRESS_FIELD_CMD){   //acknowlegement
                *currentState = A_RCV;
            }
            else if(byte != FLAG){
              *currentState = START;
            }
            break;
        case A_RCV:
            if(verifyControlByte(byte)){
              *currentState = C_RCV;
              *controlByte = byte;
            }
            else if(byte == FLAG){
                *currentState = FLAG_RCV;
            }
            else{
                *currentState = START;
            }
            break;
        case C_RCV:
            if(byte == (ADDRESS_FIELD_CMD^(*controlByte))){
              *currentState = BCC_OK;
            }        
            else if(byte == FLAG){
              *currentState = FLAG_RCV;
            }    
            else{
              *currentState = START;
            }     
            break;
        case BCC_OK:
            if(byte == FLAG){
              *currentState = STOP;
            }
            else{
              *currentState = START;
            }
            break;
        case STOP:
            break;
        default:
            break;
    }
}

void readReceiverResponse(int fd){
  unsigned char byte;
  enum state state = START;
  unsigned char controlByte;
  while(state != STOP && info.alarmFlag == 0){
      if(read(fd,&byte,1) < 0){
        perror("Error reading byte of the receiver response");
      }
      responseStateMachine(&state,byte,&controlByte);
  }
}

void readTransmitterResponse(int fd){
  unsigned char byte;
  enum state state = START;
  unsigned char controlByte;
  while(state != STOP && info.alarmFlag == 0){
    if(read(fd,&byte,1) < 0){
      perror("Error reading byte of the transmitter response");
    }
    responseStateMachine(&state,byte,&controlByte);
  }
}


int llopen(char *port,int flag){
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

    info.newtio.c_cc[VTIME] = 1;   /* inter-unsigned character timer unused */
    info.newtio.c_cc[VMIN] = 1;   /* blocking read until 5 unsigned chars received */

    tcflush(fd, TCIOFLUSH);

    if (tcsetattr(fd,TCSANOW,&info.newtio) == -1) {
        perror("tcsetattr");
        exit(-1);
    }

    infoSetup();
    printf("New termios structure set\n");

    //First frames' transmission
    if(flag == TRANSMITTER){
      unsigned char controlFrame[5];
      controlFrame[0] = FLAG;
      controlFrame[1] = ADDRESS_FIELD_CMD;
      controlFrame[2] = CONTROL_BYTE_SET;
      controlFrame[3] = controlFrame[1] ^ controlFrame[2];
      controlFrame[4] = FLAG;
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
      do{
        initializeAlarm();
        info.alarmFlag = 0;
        readTransmitterResponse(fd);
      }
      while(info.alarmFlag == 1 && info.numTries < MAX_TRIES);
        
      disableAlarm();

      if(info.numTries >= MAX_TRIES){
        printf("Exceeded number of maximum tries!\n");
        return -2;
      }

      unsigned char controlFrame[5];
      controlFrame[0] = FLAG;
      controlFrame[1] = ADDRESS_FIELD_CMD;
      controlFrame[2] = CONTROL_BYTE_UA;
      controlFrame[3] = controlFrame[1] ^ controlFrame[2];
      controlFrame[4] = FLAG;
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
    while(state != STOP && info.alarmFlag == 0){
        if(read(fd,&byte,1) < 0){
          perror("Error reading byte");
        }
        responseStateMachine(&state,byte,controlByte);
    }

    if(*controlByte == CONTROL_BYTE_RR0 && info.ns == 1){
      return 0;
    }
    else if(*controlByte == CONTROL_BYTE_RR1 && info.ns == 0){
      return 0;
    }
    else if(*controlByte == CONTROL_BYTE_REJ0 && info.ns == 1){
      return -1;
    }
    else if(*controlByte == CONTROL_BYTE_REJ1 && info.ns == 0){
      return -1;
    }
    else
    {
      return -1;
    }
    
    return 0;
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
      frameToSend[0] = FLAG;    //FLAG
      frameToSend[1] = ADDRESS_FIELD_CMD;    //UA
      if(info.ns == 0)
        frameToSend[2] = CONTROL_BYTE_0;
      else
      {
        frameToSend[2] = CONTROL_BYTE_1;
      }
      frameToSend[3] = frameToSend[1] ^ frameToSend[2]; 
      
      for (size_t i = 0; i < length; i++){
        bcc2 ^= buffer[i];
        if(buffer[i] == FLAG || buffer[i] == ESC_BYTE){   //if the byte its equal to the flag or to the escape byte
          frameToSend[frameIndex] = ESC_BYTE;
          frameIndex++;
          frameToSend[frameIndex] = buffer[i] ^ STUFFING_BYTE;
          frameIndex++;
        }
        else{
          frameToSend[frameIndex] = buffer[i];
          frameIndex++;
        }
      }

      if(bcc2 == FLAG || bcc2 == ESC_BYTE){
        frameToSend[frameIndex] = ESC_BYTE;
        frameIndex++;
        frameToSend[frameIndex] = bcc2 ^ STUFFING_BYTE;
        frameIndex++;
      }
      else{
        frameToSend[frameIndex] = bcc2;
        frameIndex++;
      }

      frameToSend[frameIndex] = FLAG;

      frameLength = frameIndex+1;
      charactersWritten = write(fd,frameToSend,frameLength);

      info.alarmFlag = 0;
      initializeAlarm();
      
      //read receiver response
      if(processControlByte(fd,&controlByte) == -1){  // if there is an error sending the message, send again 
        disableAlarm();
        info.alarmFlag = 1;
        info.numTries++;
      }

    }while(info.numTries < MAX_TRIES && info.alarmFlag);

    disableAlarm();
    
    if(info.numTries >= MAX_TRIES){
        printf("Exceeded number of maximum tries!\n");
        return -1;
    }


    return charactersWritten; 
}

void informationFrameStateMachine(enum state* currentState, unsigned char byte, unsigned char* controlByte){
    switch(*currentState){
        case START:
            if(byte == FLAG)     //flag
                *currentState = FLAG_RCV;
            break;
        case FLAG_RCV:
            if(byte == ADDRESS_FIELD_CMD)   //acknowlegement
                *currentState = A_RCV;
            else if(byte != FLAG)
                *currentState = START;
            break;
        case A_RCV:
            if(byte == CONTROL_BYTE_0 || byte == CONTROL_BYTE_1){
              *currentState = C_RCV;
              *controlByte = byte;
            }
            else if(FLAG == 0x7E){
                *currentState = FLAG_RCV;
            }
            else{
                *currentState = START;
            }
            break;
        case C_RCV:
            if(byte == (ADDRESS_FIELD_CMD^(*controlByte)))
              *currentState = BCC_OK;
            else if(byte == FLAG)
              *currentState = FLAG_RCV;
            else
              *currentState = START;
            
            break;
        case BCC_OK:
            if(byte != FLAG)
              *currentState = DATA_RCV;
            break;
        case DATA_RCV:
            if(byte == FLAG)
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
    while(state != STOP && info.alarmFlag == 0){
      if(read(fd,&byte,1) < 0){
        perror("Error reading byte");
      }   
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
  if(controlByte != CONTROL_BYTE_0 && controlByte != CONTROL_BYTE_1){
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
  unsigned char auxBuffer[131072];
  int buffIndex = 0;
  while(received == 0){
    initializeAlarm();
    length = readTransmitterFrame(fd,auxBuffer);
    disableAlarm();
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
        if(auxBuffer[i] == ESC_BYTE){
          escapeByteFound = 1;
          continue;
        }
        else if(auxBuffer[i] == (FLAG^STUFFING_BYTE) && escapeByteFound == 1){
          originalFrame[originalFrameIndex] = FLAG;
          originalFrameIndex++;
          escapeByteFound = 0;
        }
        else if(auxBuffer[i] == (ESC_BYTE^STUFFING_BYTE) && escapeByteFound == 1){
          originalFrame[originalFrameIndex] = ESC_BYTE;
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
        if(controlByte == CONTROL_BYTE_0){
          unsigned char frameToSend[5];
          frameToSend[0] = FLAG;
          frameToSend[1] = ADDRESS_FIELD_CMD;
          frameToSend[2] = CONTROL_BYTE_REJ0;
          frameToSend[3] = frameToSend[1] ^ frameToSend[2];
          frameToSend[4] = FLAG;
          write(fd,frameToSend,5);
        }
        else if(controlByte == CONTROL_BYTE_1){
          unsigned char frameToSend[5];
          frameToSend[0] = FLAG;
          frameToSend[1] = ADDRESS_FIELD_CMD;
          frameToSend[2] = CONTROL_BYTE_REJ1;
          frameToSend[3] = frameToSend[1] ^ frameToSend[2];
          frameToSend[4] = FLAG;
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
        if(controlByte == CONTROL_BYTE_0){
          unsigned char frameToSend[5];
          frameToSend[0] = FLAG;
          frameToSend[1] = ADDRESS_FIELD_CMD;
          frameToSend[2] = CONTROL_BYTE_RR1;
          frameToSend[3] = frameToSend[1] ^ frameToSend[2];
          frameToSend[4] = FLAG;
          write(fd,frameToSend,5);
        }
        else if(controlByte == CONTROL_BYTE_1){
          unsigned char frameToSend[5];
          frameToSend[0] = FLAG;
          frameToSend[1] = ADDRESS_FIELD_CMD;
          frameToSend[2] = CONTROL_BYTE_RR0;
          frameToSend[3] = frameToSend[1] ^ frameToSend[2];
          frameToSend[4] = FLAG;
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
        controlFrameDISC[1] = ADDRESS_FIELD_CMD;
        controlFrameDISC[2] = CONTROL_BYTE_DISC;
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
            controlFrameUA[1] = ADDRESS_FIELD_CMD;
            controlFrameUA[2] = CONTROL_BYTE_UA;
            controlFrameUA[3] = controlFrameUA[1] ^ controlFrameUA[2];
            controlFrameUA[4] = FLAG;

            write(fd,controlFrameUA,5); //write UA after receiving DISC
            sleep(1);
        }
    }

    else if(flag == RECEIVER){
        readTransmitterResponse(fd);
        unsigned char controlFrame[5];
        controlFrame[0] = FLAG;
        controlFrame[1] = ADDRESS_FIELD_CMD;
        controlFrame[2] = CONTROL_BYTE_DISC;
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