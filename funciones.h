#ifndef funciones_h
#define funciones_h


#define DEBUG true

#define START_BYTE 0x7e
#define ESCAPE 0x7d
#define XON 0x11
#define XOFF 0x13

#define RESET_SLAVES_PIN 13

#define NO_ERROR 0
#define UNEXPECTED_START_BYTE 1
#define CHECKSUM_FAILURE 2
#define PACKET_EXCEEDS_BYTE_ARRAY_LENGTH 3
#define PARSER_UART_PARAMS_MAX 68 // maximo numero de parametros que se pueden almacenar por I2C request

#define SEND_RELAYS_METHOD 13 // method envio de relays state

#define BACKUP_INTERVAL 86400000 // once a day
#define RESTORE_GATEWAY_TIMEOUT 20000 // 10 seg maximum
#define GATEWAY_WHILES_TIMEOUT 5000 // 5 seg maximum
#define RELAY_STATE_REFRESH_INTERVAL 120000 // 2 minutes
#define LAST_SENDXBEE_INTERVAL 600000 //10 minutes

// Estados de la estructura buffer
#define ESPACIO_LIBRE_EN_BUFFER 0
#define PARA_ENVIAR_SERIAL1 1
#define PARA_ENVIAR_SERIAL2 2
#define PARA_ENVIAR_SERIAL3 3
#define PARA_ENVIAR_XBEE 4
//#define ESPERA_RESPUESTA_XBEE 

#define TAMANO_MAX_BUFFEROUT 40

/////////// METHOD ////////////
//MAXQ
#define MAXQ_GET_VARIABLE_METHOD 98 // get a single param and send it back to gateway, (used for calibration porpuses)
#define MAXQ_GET_NOW_SAMPLE_METHOD 99 // get all params available and send them to gateway
#define MAXQ_UPDATE_CALI_PARAMS_METHOD 100 // update calibration params and reload MAXQ
#define MAXQ_UPDATE_DATETIME_METHOD 97 // method to update datetime
#define MAXQ_INIT_EEPROM 232 // alter EEPROM remotely
//RELES
#define RELE_REGLA_MANUAL_METHOD 23 
#define RELE_REGLA_CONTROL_METHOD 24 
#define RELE_MEDICON_REFRESH_METHOD 25
#define RELE_MEDICION_SENSORES_METHOD 26 
#define RELE_UPDATE_DATETIME_METHOD 27
#define RELE_CLEAN_EEPROM_METHOD 28 // reset eeprom
//ASYNC SENSORS
#define ASYNC_NODE_METHOD 15   // nodo-> gateway 
//TOD
#define TOD_METHOD 11 // node -> gateway
// UTILS METHOS
#define RESET_SLAVES 128 // reset slaves

/* STRUCTURES */

typedef struct BufferUARTPacket_struct {
  byte Estado;
  unsigned long Turno;
  byte Method;
  byte params[PARSER_UART_PARAMS_MAX];
  byte len_params;
  byte Id;
}BufferUARTPacket;


///////////////////////// NEW /////////////////////////
void printBuffer(boolean isBufferOut, byte index);
void printParams(byte *cadena, byte len, boolean br);
boolean sendXbee();
boolean receiveXbee();
void defineEstado();
void saveInBufferOut();
boolean readPacket(HardwareSerial serial, int timeToWait);
void sendPacket(HardwareSerial serial, byte estado);
void sendByte(HardwareSerial serial, byte b, boolean escape);
void sendByteDebug(byte b, boolean escape);
void checkNewUART1AndSendOne();
void checkNewUART2AndSendOne();
void checkNewUART3AndSendOne();
void checkNewZigbeeAndSendOne();
void resetSlaves();
#endif
