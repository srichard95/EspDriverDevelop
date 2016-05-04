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

int sent = 0;
int received = 0;
int WrongCrc = 0;
bool sync = false;
int SyncTimeout = 0;

char ToSendCorrect[] = { 0x78, 0x1C, 0x60, 0x7D, 0xDC, 0x69, 0x3B, 0x56, 0xDB, 0xB5, 0xF1, 0xD7, 0xC4, 0xA5, 0x1D };

static void rxchar(UARTDriver *uartp, uint16_t c) {

  (void)uartp;
  (void)c;


}

static void rxend(UARTDriver *uartp) {

  (void)uartp;

  /*
  int i;
  for(i = 0; i<FRAME_SIZE_BYTE; i++)
  {
    sdPut(&SD2, buffer[i]);
  }*/
  if(sync == true)
  {
        sync_procedure_end();
        uartStartReceive (&UARTD1, FRAME_SIZE_BYTE, &buffer);
        return;
  }

  if(CheckCRC(&buffer) == 0)
  {
    received++;
  }else
  {
    WrongCrc++;
    sync_procedure(&buffer);
  }

  uartStartReceive (&UARTD1, FRAME_SIZE_BYTE, &buffer);
  palTogglePad(GPIOB, GPIOB_LED1);
}

void sync_procedure(char *LastFrame)
{
  //Create sync frame*********************
  char sync_frame[FRAME_SIZE_BYTE];
  int i;
  for (i = 0; i < FRAME_SIZE_BYTE; ++i)
      sync_frame[i] = 0xFF;
  //**************************************
  if(IsSyncFrame(&LastFrame))
  {
      uartStartSend(&UARTD1, FRAME_SIZE_BYTE, sync_frame);
      return;
  }
  uartStartSend(&UARTD1, FRAME_SIZE_BYTE, sync_frame);
  sync = true;
  return;
}
int FFs=0;
void sync_procedure_end(){
    SyncTimeout++;
    if(SyncTimeout >= SYNC_TIMEOUT_THRS)
    {
        sync = false;
        SyncTimeout = 0;
    }


    int i;
    for(i=0; i<FRAME_SIZE_BYTE; i++)
    {
        if(buffer[i] == 0xFF)
            FFs++;
    }
    if(FFs >= FRAME_SIZE_BYTE)
    {
        FFs = 0;
        sync=false;
    }
    return;
}

void IsSyncFrame(char *frame)
{
    int i;
    bool result = true;
    for (i = 0; i < FRAME_SIZE_BYTE; ++i)
        if(frame[i] != 0xFF)
            result = false;
    return result;
}











static UARTConfig uart_cfg_1 = {
  NULL,
  NULL,
  rxend,
  NULL,
  NULL,
  115200,
  0,
  USART_CR2_STOP1_BITS,
  0
};


void StartUart(void)
{
  uartStart(&UARTD1, &uart_cfg_1);
  uartStartReceive(&UARTD1, FRAME_SIZE_BYTE, &buffer);
}

bool StopTest = false;
static THD_WORKING_AREA(waUartTest, 128);
static THD_FUNCTION(UartTestThread, arg) {
  chRegSetThreadName("Driver Test");
  systime_t time;
  time = chVTGetSystemTime();
  while(!StopTest)
  {
    time += MS2ST(2);
    uartStartSend(&UARTD1, FRAME_SIZE_BYTE, ToSendCorrect);
    sent++;
    chThdSleepUntil(time);
  }
}





void esp_send(BaseSequentialStream *chp, int argc, char *argv[]) {

  uartStartSend(&UARTD1, FRAME_SIZE_BYTE, ToSendCorrect);
  chprintf(chp, "DATA SENT!\r\n");
}
char ToSendSync[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

void esp_send_sync(BaseSequentialStream *chp, int argc, char *argv[]) {

  uartStartSend(&UARTD1, FRAME_SIZE_BYTE, ToSendSync);
  chprintf(chp, "DATA SENT!\r\n");
}

void start_driver_test(BaseSequentialStream *chp, int argc, char *argv[]) {
  StopTest = false;
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
}
