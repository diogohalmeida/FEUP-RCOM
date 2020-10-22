#include "alarm.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>


//ConnectionInfo struct setup
void infoSetup();

//Auxiliary functions
int verifyControlByte(unsigned char byte);
void responseStateMachine(enum state* currentState, unsigned char byte, unsigned char* controlByte);
void informationFrameStateMachine(enum state* currentState, unsigned char byte, unsigned char* controlByte);
int verifyFrame(unsigned char* frame,int length);
int processControlByte(int fd, unsigned char *controlByte);

//Transmitter
void readReceiverResponse(int fd);

//Receiver
void readTransmitterResponse(int fd);
int readTransmitterFrame(int fd, unsigned char * buffer);

// ll functions
int llopen(char* port, int flag);
int llwrite(int fd, char* packet, int length);
int llread(int fd, char* buf);
int llclose(int fd, int flag);
