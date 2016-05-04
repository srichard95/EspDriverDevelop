/*
 * uart.c
 *
 *  Created on: 2016 febr. 9
 *      Author: srich
 */

#include "ch.h"
#include "hal.h"
#include "EspUart.h"
#include "console.h"

int counter = 0;
char buffer[FRAME_SIZE_BYTE];


bool PauseTest = false;

int sent = 0;
int received = 0;
int WrongCrc = 0;
int SyncTimeout = 0;
int Sync = 0;

char ToSendCorrect[] = { 0x78, 0x1C, 0x60, 0x7D, 0xDC, 0x69, 0x3B, 0x56, 0xDB, 0xB5, 0xF1, 0xD7, 0xC4, 0xA5, 0x1D};
char ToSendSync[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

void SendSyncFrame(){
  sdWrite(&SD1, ToSendSync, FRAME_SIZE_BYTE);
}


void SyncProcedure()
{
  int FFs = 0;
  char c;

  SendSyncFrame();
  Sync++;

  while(FFs != FRAME_SIZE_BYTE)
  {
    SyncTimeout++;
    sdReadTimeout(&SD1, &c, 1, US2ST(1000));
    if(c == 0xFF)
    {
      FFs++;
      c = 0x00;
    }
    if(SyncTimeout >= SYNC_TIMEOUT_THRS){
      SendSyncFrame();
      SyncTimeout = 0;
    }
  }
  SyncTimeout = 0;
  PauseTest = false;
}




static THD_WORKING_AREA(waSDReceiving, 128);
static THD_FUNCTION(SDReceiving, arg) {
  chRegSetThreadName("Receiving Test");

  while(true)
  {
    int i;
    for(i = 0; i < FRAME_SIZE_BYTE; i++)
      sdRead(&SD1, &buffer[i], 1);

    if(CheckCRC(&buffer) == 0)
    {
      received++;
    }else
    {
      PauseTest = true;
      WrongCrc++;
      SyncProcedure();
    }




    palTogglePad(GPIOB, GPIOB_LED1);
  }
}

static SerialConfig uartCfg1 =
{
921600, // bit rate
0,
0,
0
};

void StartUart(void)
{

  sdStart(&SD1, &uartCfg1);
  chThdCreateStatic(waSDReceiving, sizeof(waSDReceiving), NORMALPRIO+1, SDReceiving, NULL);
}

bool StopTest = false;

int SendingFreq = 10000;
int SendingPiece = 1;

char ToSendWrong[] = { 0x50, 0x4D, 0xF4, 0xEB, 0xF8, 0xB1, 0x6F, 0x75, 0x43, 0x82, 0xAB, 0x23, 0x42, 0xE9 };
static THD_WORKING_AREA(waUartTest, 128);
static THD_FUNCTION(UartTestThread, arg) {
  chRegSetThreadName("Driver Test");
  systime_t time;
  time = chVTGetSystemTime();
  int i;
  while(!StopTest)
  {
    time += US2ST(SendingFreq);
    if(!PauseTest)
    {
      chSysLock();

      for(i=0; i<SendingPiece; i++)
        sdWrite(&SD1, ToSendCorrect, FRAME_SIZE_BYTE);
      chSysUnlock();
      sent+=SendingPiece;


      /*if((sent % 10000)==0)
        sdWrite(&SD1, ToSendWrong, FRAME_SIZE_BYTE);*/
    }
    chThdSleepUntil(time);
  }
}







void esp_send(BaseSequentialStream *chp, int argc, char *argv[]) {

  sdWrite(&SD1, ToSendCorrect, FRAME_SIZE_BYTE*2);
  chprintf(chp, "DATA SENT!\r\n");
}


void esp_send_sync(BaseSequentialStream *chp, int argc, char *argv[]) {

  sdWrite(&SD1, ToSendSync, FRAME_SIZE_BYTE);
  chprintf(chp, "DATA SENT!\r\n");
}

void start_driver_test(BaseSequentialStream *chp, int argc, char *argv[]) {
  StopTest = false;
  PauseTest = false;
  SendingFreq = atoi(argv[0]);
  SendingPiece = atoi(argv[1]);
  chThdCreateStatic(waUartTest, sizeof(waUartTest), NORMALPRIO+1, UartTestThread, NULL);
  while (chnGetTimeout((BaseChannel *)chp, TIME_IMMEDIATE) == Q_TIMEOUT) {
    chprintf(chp, "\x1B\x63");
    chprintf(chp, "\x1B[2J");
    chprintf(chp, "DRIVER STRESS TEST\r\n", sent);


    int lost = sent - received;
    chprintf(chp, "Sent: %d\r\n", sent);
    chprintf(chp, "Received: %d\r\n", received);
    chprintf(chp, "Lost: %d\r\n", lost);
    chprintf(chp, "WrongCrc: %d\r\n", WrongCrc);
    chprintf(chp, "Sync: %d\r\n", Sync);
    chprintf(chp, "SyncTimeout: %d\r\n", SyncTimeout);

    int i;
    for(i=0; i<FRAME_SIZE_BYTE;i++)
    {
      chprintf(chp, "%c", buffer[i]);
    }

    chThdSleepMilliseconds(100);
  }
  StopTest = true;
  sent = 0;
  received = 0;
  WrongCrc = 0;
  Sync = 0;
}
