#include <termios.h>


#define BAUDRATE B38400
#define MODEMDEVICE "/dev/ttyS1"
#define _POSIX_SOURCE 1 /* POSIX compliant source */

#define TRANSMITTER 0
#define RECEIVER 1

#define C0     0x00
#define C1     0x40

#define FLAG 0x7E
#define A_CMD 0x03
#define A_REP 0x01

#define C_SET 	0x03
#define C_DISC 	0x0B
#define C_UA 	0x07
#define C_RR0	0x05
#define C_RR1	0x85
#define C_REJ0	0x81
#define C_REJ1	0x01

#define ESC 0x7D
#define STUFFING

#define MAX_TRIES 3

enum state{START, FLAG_RCV, A_RCV, C_RCV, BCC_OK, STOP, DATA_RCV};


typedef struct {
    //alarm info
    int numTries;
    int alarmFlag;
    int ns;
    
    //struct termios
    struct termios oldtio, newtio;

}   ConnectionInfo;

extern ConnectionInfo info;