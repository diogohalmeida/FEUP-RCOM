#include "alarm.h"

void sigAlarmHandler(){
  printf("Timeout!\n");
  info.alarmFlag = 1;
  info.numTries++;
}

void initializeAlarm(){
  struct sigaction sa;
	sa.sa_handler = &sigAlarmHandler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
  info.alarmFlag = 0;

	sigaction(SIGALRM, &sa, NULL);

  alarm(20); 

}

void disableAlarm(){
  struct sigaction sa;
	sa.sa_handler = NULL;

  sigaction(SIGALRM, &sa, NULL);

  info.alarmFlag = 0;

  alarm(0);
}