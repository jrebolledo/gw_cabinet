#include <XBee.h>
#include <stdlib.h>
#include "funciones.h"

//Definiciones para la inicializacion del timer
#define INIT_TIMER_COUNT 0
#define RESET_TIMER2 TCNT2 = INIT_TIMER_COUNT

//Configuracion del Xbee
byte payload[75];
byte posOverFlow = 0;
int dead;
int count;
int enabled;
unsigned long time_last_sendXbee; 

XBee xbee = XBee();
XBeeResponse response = XBeeResponse();
ZBRxResponse rx = ZBRxResponse();
ModemStatusResponse msr = ModemStatusResponse();
XBeeAddress64 addr64 = XBeeAddress64(0x0013a200, 0x40600de1);			//Reemplazar con direccion del Gateway
ZBTxRequest zbTx = ZBTxRequest(addr64, payload, sizeof(payload));
ZBTxStatusResponse txStatus = ZBTxStatusResponse();

BufferUARTPacket bufferIn;
BufferUARTPacket bufferOut[TAMANO_MAX_BUFFEROUT];


void setup() {
  Serial1.begin(9600);
  Serial2.begin(9600);
  Serial3.begin(9600);
  xbee.begin(9600);
  //Seteo de Timer para interrupciones
  //Timer2 Settings:  Timer Prescaler /1024
  TCCR2B |= ((1 << CS22) | (1 << CS21) | (1 << CS20));
  //Timer2 Overflow Interrupt Enable
  TIMSK2 |= (1 << TOIE2);
  RESET_TIMER2;
  sei();
  pinMode(13,OUTPUT);
  Serial3.println("Comenzando");						//Envia el ToD para que el Gateway lo registre como conectado
//  bufferOut[0].Estado = PARA_ENVIAR_XBEE;
//  bufferOut[0].Turno = 0;
//  bufferOut[0].Method = TOD_METHOD;
//  bufferOut[0].params[0] = 1;
//  bufferOut[0].len_params = 1;
//  bufferOut[0].Id = 1;

  for(int i = 0; i < TAMANO_MAX_BUFFEROUT; i++){
    bufferOut[i].Estado = ESPACIO_LIBRE_EN_BUFFER;
  }
  enabled=1;
}


void loop() {
  //backupData(); // save all structure data ones a day
  checkNewZigbeeAndSendOne();
  checkNewUART1AndSendOne();  
  
  checkNewZigbeeAndSendOne();
  checkNewUART2AndSendOne();
  
//  checkNewZigbeeAndSendOne();
//  checkNewUART3AndSendOne();
 		//Flag que se tiene que poner en 0 para no activar el reset por Timer
}


//Interrupcion del Timer para el reset
ISR(TIMER2_OVF_vect) {
  if(enabled){
    if(count>600){		//Aprox 10 seg
      count=0;
      if(false){
        Serial3.println("reset");
        enabled=0;
        asm volatile ("jmp 0x0000"); 
      }
      dead=1;
    }
    count++;
  }
};
