#include <XBee.h>
#include <stdlib.h>
#include "funciones.h"

//Declaracion de todas las variables externas
extern XBee xbee;
extern XBeeResponse response;
extern ZBRxResponse rx;
extern ModemStatusResponse msr;
extern XBeeAddress64 addr64;
extern ZBTxRequest zbTx;
extern ZBTxStatusResponse txStatus;
// buffer que contiene el parseo 
extern BufferUARTPacket bufferIn;
extern BufferUARTPacket bufferOut[TAMANO_MAX_BUFFEROUT];
extern boolean dead;
extern byte payload[75];
extern byte posOverFlow;
extern unsigned long time_last_sendXbee; 
extern int _reset_slaves_pin;

////////////////////////////////////////////////////////////////
////////////////////// Comunicacion  ///////////////////////////
////////////////////////////////////////////////////////////////

boolean sendXbee()
{
  byte _pos = 0xFF;
  unsigned long _tempTurno = 0xFFFFFFFF;
  for (int i = 0; i < TAMANO_MAX_BUFFEROUT; i++){
    if ((PARA_ENVIAR_XBEE == bufferOut[i].Estado) && (bufferOut[i].Turno < _tempTurno)){
      _tempTurno = bufferOut[i].Turno;
      _pos = i;
    }
  }
  if (_pos != 0xFF){
    #if (DEBUG)
      Serial3.println("\n\t(XBEE SENDING)");
      Serial3.print("Manda desde posicion ");
      Serial3.print(_pos, DEC);
      Serial3.print(" turno ");
      Serial3.print(bufferOut[_pos].Turno, DEC);
      Serial3.println(" al XBee:");
      printBuffer(true,_pos);
      Serial3.print("\t(HEX)0x ");
    #endif
    payload[0] = bufferOut[_pos].Method;
    for (int i = 0; i < bufferOut[_pos].len_params; i++){
      payload[i + 1] = bufferOut[_pos].params[i];
    }
    payload[bufferOut[_pos].len_params + 1] = bufferOut[_pos].Id;
    #if (DEBUG)
      for (int i = 0; i < bufferOut[_pos].len_params + 2; i++){
        Serial3.print(payload[i], HEX);
        Serial3.print(" ");
      }
    #endif
    zbTx.setPayloadLength(bufferOut[_pos].len_params + 2);
    xbee.send(zbTx);// after sending a tx request, we expect a status response
    receiveXbee(); // Espera respuesta de estatus de un mensaje correcto
    /////////////////////////////////OJO revisar si cambiar el estado o no?? quizas ponerlo en espera hasta tener confirmación
    //Se borrara despues de ser recibido
    /////bufferOut[_pos].Estado = ESPACIO_LIBRE_EN_BUFFER;
  }
}

// continuously reads packets, looking for ZB Receive or Modem Status
boolean receiveXbee() {
  byte _pos = 0xFF;
  unsigned long _tempTurno = 0xFFFFFFFF; 
  xbee.readPacket(200);
  if (xbee.getResponse().isAvailable()) {
    // got something
    if (xbee.getResponse().getApiId() == ZB_RX_RESPONSE) {
      // got a zb rx packet
      // now fill our zb rx class
      xbee.getResponse().getZBRxResponse(rx);
      if (rx.getOption() == ZB_PACKET_ACKNOWLEDGED) {
          // the sender got an ACK
          bufferIn.Method = rx.getData(0);
          defineEstado(); // graba el bufferIn.Estado = ....
          for (int i = 1; i < rx.getDataLength()-1; i++){// No se cuenta el Method ni ID
            bufferIn.params[i-1] = rx.getData(i);
          }
          bufferIn.len_params = rx.getDataLength()-2; // No se cuenta el Method ni ID
          bufferIn.Id = rx.getData(rx.getDataLength()-1); // El ultimo dato es el Id
          if (bufferIn.Method == RESET_SLAVES){
            resetSlaves();
          }
          else {
            saveInBufferOut();
          }
          
            
      } else {
          // we got it (obviously) but sender didn't get an ACK
      }
    } else if (xbee.getResponse().getApiId() == ZB_TX_STATUS_RESPONSE) {
      xbee.getResponse().getZBTxStatusResponse(txStatus);
      // get the delivery status, the fifth byte
      if (txStatus.getDeliveryStatus() == SUCCESS) {
        // success.  time to celebrate
        //busca el menor turno y lo borra ya que fue el ultimo mensaje enviado
        for(int i = 0; i < TAMANO_MAX_BUFFEROUT; i++){
          if ((bufferOut[i].Estado == PARA_ENVIAR_XBEE) && (bufferOut[i].Turno < _tempTurno)){
            _tempTurno = bufferOut[i].Turno;
            _pos = i;
          }
        }
        if (_pos != 0xFF) bufferOut[_pos].Estado = ESPACIO_LIBRE_EN_BUFFER;
        #if (DEBUG)
          Serial3.println(" Success");
          Serial3.println("\t(ZIGBEE receiving)");
          printBuffer(false,0);
        #endif
        delay(200);
      }
    } else {
      // not something we were expecting 
      
    }
  } else {
    return false;
  }
  return true;
}

void defineEstado(){
  switch (bufferIn.Method){
    case MAXQ_GET_VARIABLE_METHOD: // get a single param and send it back to gateway, (used for calibration porpuses)
    case MAXQ_GET_NOW_SAMPLE_METHOD: // get all params available and send them to gateway
    case MAXQ_UPDATE_CALI_PARAMS_METHOD: // update calibration params and reload MAXQ
    case MAXQ_UPDATE_DATETIME_METHOD: // method to update datetime
    case MAXQ_INIT_EEPROM:
      bufferIn.Estado = PARA_ENVIAR_SERIAL1;
      #if (DEBUG)
        Serial3.print("\t(MAXQ METHOD):");Serial3.println(bufferIn.Method,DEC);
      #endif
      break;
    case RELE_REGLA_MANUAL_METHOD: 
    case RELE_REGLA_CONTROL_METHOD:
    case RELE_MEDICON_REFRESH_METHOD:
    case RELE_UPDATE_DATETIME_METHOD:
    case RELE_MEDICION_SENSORES_METHOD:
    case RELE_CLEAN_EEPROM_METHOD:
      bufferIn.Estado = PARA_ENVIAR_SERIAL2;
      #if (DEBUG)
        Serial3.print("\t(RELE METHOD):");Serial3.println(bufferIn.Method,DEC);
      #endif
      break;
    case 255:
      bufferIn.Estado = PARA_ENVIAR_SERIAL3;
    case TOD_METHOD: //Cuando recibe un tod lo no responde
    default:
      bufferIn.Estado = ESPACIO_LIBRE_EN_BUFFER;
  }
}
    
void saveInBufferOut(){
  byte _pos = 0xFF;
  unsigned long _tempTurno = 0;
  //Serial3.println("in out");
  //Primero busca si se puede sobre escribir o no
  switch (bufferIn.Method){
    case RELE_MEDICION_SENSORES_METHOD:
      for(int i = 0; i < TAMANO_MAX_BUFFEROUT; i++){
        if ((bufferOut[i].Estado == bufferIn.Estado) && (bufferOut[i].Method == bufferIn.Method) && (bufferOut[i].params[0] == bufferIn.params[0])){
          _pos = i;
          bufferIn.Turno = bufferOut[i].Turno;
          break;
        }
      }
      break;

    case RELE_MEDICON_REFRESH_METHOD:
    case MAXQ_UPDATE_DATETIME_METHOD:
      for(int i = 0; i < TAMANO_MAX_BUFFEROUT; i++){
        if ((bufferOut[i].Estado == bufferIn.Estado) && (bufferOut[i].Method == bufferIn.Method)){
          _pos = i;
          bufferIn.Turno = bufferOut[i].Turno;
          break;
        }
      }
  }
  if (_pos == 0xFF){
    for(int i = 0; i < TAMANO_MAX_BUFFEROUT; i++){
      if ((bufferOut[i].Estado == ESPACIO_LIBRE_EN_BUFFER) && (_pos == 0xFF)){
        _pos = i;
      }
      if ((bufferIn.Estado == bufferOut[i].Estado) && (bufferOut[i].Turno > _tempTurno)){
        _tempTurno = bufferOut[i].Turno;
      }
    }
    if (_pos == 0xFF){
      for(int i = 0; i < TAMANO_MAX_BUFFEROUT; i++){
        if ((bufferIn.Estado == bufferOut[i].Estado) && (bufferOut[i].Turno < _tempTurno)){
          _tempTurno = bufferOut[i].Turno;
          _pos = i;
        }
      }
    }
    if (_pos == 0xFF){ 
      posOverFlow++;
      if(posOverFlow == TAMANO_MAX_BUFFEROUT) posOverFlow = 0;
      _pos = posOverFlow;
    }
    //Serial3.println(_pos, DEC);
    bufferIn.Turno = _tempTurno + 1; //Le da el siguiente turno
  }
  bufferOut[_pos] = bufferIn;
  //Serial3.println(bufferOut[_pos].Method, DEC);
}

// START_BYTE|Lenght|Metodo and data|
boolean readPacket(HardwareSerial serial, int timeToWait) {
  byte _checksumTotal = 0;
  byte _pos = 0;
  byte b = 0;
  boolean _escape = false; 
  if(serial.available())delay(timeToWait);//Espera a que llegue todo el mensaje
  while (serial.available()) {
    b = serial.read();
    if (_pos == 0 && b != START_BYTE) {
      return true;
    }
    if (_pos > 0 && b == START_BYTE) {
      // new packet start before previous packeted completed -- discard previous packet and start over
      _pos = 0;
      _checksumTotal = 0;
    }
    if (_pos > 0 && b == ESCAPE) {
      if (serial.available()) {
        b = serial.read();
        b = 0x20 ^ b;
      } 
      else {
        // escape byte.  next byte will be
        _escape = true;
        continue;
      }
    }
    if (_escape == true) {
      b = 0x20 ^ b;
      _escape = false;
    }
    // checksum includes all bytes after len
    if (_pos >= 2) {
      _checksumTotal+= b;
    }
    switch(_pos) {
    case 0:
      if (b == START_BYTE) {
        _pos++;
      }
      break;
    case 1:
      // length msb
      bufferIn.len_params = b;
      _pos++;
      break;
    case 2:
      bufferIn.Method = b;
      _pos++;
      break;
    default:
      // check if we're at the end of the packet
      // packet length does not include start, length, method or checksum bytes, so add 4

      if (_pos == (bufferIn.len_params + 4)) {
        // verify checksum
        if ((_checksumTotal & 0xff) == 0xff) {
          bufferIn.Estado = PARA_ENVIAR_XBEE;
          bufferIn.Id = bufferIn.params[bufferIn.len_params];// El Id se grabo en el mismo arreglo
          saveInBufferOut();
          #if (DEBUG)
            Serial3.println("\t(UART Receiving)");
            Serial3.print("\tMethod:");Serial3.println(bufferIn.Method,DEC);
            Serial3.print("\tparams:");printParams(bufferIn.params,bufferIn.len_params,true);
            Serial3.print("\topID:");Serial3.print(bufferIn.Id,DEC);
          #endif
        } 
        else {
          // checksum failed
          //bufferIn.ErrorCode = CHECKSUM_FAILURE;
        }
        // reset state vars
        _pos = 0;
        _checksumTotal = 0;
        return true;
      } 
      else {
        // add to params array, Sin start byte, lenght, method ni checksum
        bufferIn.params[_pos - 3] = b;
        _pos++;
      }
    }
  }
  return false;
}

void sendPacket(HardwareSerial serial, byte estado) {
  byte _pos = 0xFF;
  unsigned long _tempTurno = 0xFFFFFFFF;
  byte checksum = 0;
  for(int i = 0; i < TAMANO_MAX_BUFFEROUT; i++){
    if ((estado == bufferOut[i].Estado) && (bufferOut[i].Turno < _tempTurno)){
      _tempTurno = bufferOut[i].Turno;
      _pos = i;
    }
  }
  if (_pos != 0xFF){
    #if DEBUG
      Serial3.println();
      Serial3.print("Manda desde pocision ");
      Serial3.print(_pos, DEC);
      Serial3.print(" turno ");
      Serial3.print(bufferOut[_pos].Turno, DEC);
      Serial3.println(" al RS232:");
      printBuffer(true,_pos);
      Serial3.print("\t(HEX)0x ");
    #endif
    
    sendByte(serial, START_BYTE, false);
    // send length
    sendByte(serial, bufferOut[_pos].len_params, true);
    // Metodo
    sendByte(serial, bufferOut[_pos].Method, true);
    // compute checksum
    checksum+= bufferOut[_pos].Method;
    for (int i = 0; i < bufferOut[_pos].len_params; i++) {
      sendByte(serial, bufferOut[_pos].params[i], true);
      checksum+= bufferOut[_pos].params[i];
    }
    sendByte(serial, bufferOut[_pos].Id,true);
    checksum+= bufferOut[_pos].Id;
    
    checksum = 0xff - checksum;// perform 2s complement
    sendByte(serial, checksum, true);// send checksum
    // send packet
    
    /////////////////////////////////OJO revisar si cambiar el estado o no?? quizas ponerlo en espera hasta tener confirmación
    
    bufferOut[_pos].Estado = ESPACIO_LIBRE_EN_BUFFER;
  }
}

void sendByte(HardwareSerial serial, byte b, boolean escape) {
  if (escape && (b == START_BYTE || b == ESCAPE || b == XON || b == XOFF)) {
    serial.print(ESCAPE, BYTE);
    serial.print(b ^ 0x20, BYTE);
  } 
  else serial.print(b, BYTE);
  #if (DEBUG)
    sendByteDebug(b, escape);// Par visualizar por Serial3
  #endif
}

void sendByteDebug(byte b, boolean escape) {
  if (escape && (b == START_BYTE || b == ESCAPE || b == XON || b == XOFF)) {
    Serial3.print(ESCAPE, HEX);
    Serial3.print(" ");
    Serial3.print(b ^ 0x20, HEX);
    Serial3.print(" ");
  } 
  else {
    Serial3.print(b, HEX);
    Serial3.print(" ");
  }
}

void checkNewUART1AndSendOne(){
  unsigned long while_timeout = millis();
  dead=0;
  while(readPacket(Serial1, 100)){
    if (millis() < while_timeout )while_timeout = millis();
    if (millis() - while_timeout > GATEWAY_WHILES_TIMEOUT) break; //poner time out;
  }
  sendPacket(Serial1, PARA_ENVIAR_SERIAL1);
}

void checkNewUART2AndSendOne(){
  unsigned long while_timeout = millis();
  dead=0;
  while(readPacket(Serial2, 100)){
    if (millis() < while_timeout )while_timeout = millis();
    if (millis() - while_timeout > GATEWAY_WHILES_TIMEOUT) break; //poner time out;
  }
  sendPacket(Serial2, PARA_ENVIAR_SERIAL2);
}

void checkNewUART3AndSendOne(){
  unsigned long while_timeout = millis();
  dead=0;
  while(readPacket(Serial3, 100)){
    if (millis() < while_timeout )while_timeout = millis();
    if (millis() - while_timeout > GATEWAY_WHILES_TIMEOUT) break; //poner time out;
  }
  sendPacket(Serial3, PARA_ENVIAR_SERIAL3);
}

void checkNewZigbeeAndSendOne(){
  unsigned long while_timeout = millis();
  dead=0;
  if (millis() < time_last_sendXbee) time_last_sendXbee = millis();
  while(receiveXbee()){
    if (millis() < while_timeout )while_timeout = millis();
    if (millis() - while_timeout > GATEWAY_WHILES_TIMEOUT) break; //poner time out;
    time_last_sendXbee = millis();
  }
//  if ((millis() - time_last_sendXbee) > LAST_SENDXBEE_INTERVAL){
//    bufferOut[0].Estado = PARA_ENVIAR_XBEE;
//    bufferOut[0].Turno = 0;
//    bufferOut[0].Method = TOD_METHOD;
//    bufferOut[0].params[0] = 1;
//    bufferOut[0].len_params = 1;
//    bufferOut[0].Id = 1;
//    time_last_sendXbee = millis();
//  }
 
  sendXbee();
}

void printParams(byte * params, byte len, boolean br) {
  #if (DEBUG)
    for (int r=0;r<len-1;r++) {
      Serial3.print(params[r],DEC);Serial3.print(", ");
    }
    Serial3.print(params[len-1],DEC);
    if (br) {
      Serial3.println();
    }
  #endif
}

void printBuffer(boolean isoutbuffer, byte index) {
  #if (DEBUG)
    if (isoutbuffer) {
      Serial3.print("\tMetodo:");
      Serial3.println(bufferOut[index].Method,DEC);
      Serial3.print("\tParams (");Serial3.print(bufferOut[index].len_params,DEC);Serial3.print("):");
      printParams(bufferOut[index].params,bufferOut[index].len_params,true);
      Serial3.print("\tId:");Serial3.println(bufferOut[index].Id,DEC);
    }
    else {
      Serial3.print("\tMetodo:");
      Serial3.println(bufferIn.Method,DEC);
      Serial3.print("\tParams (");Serial3.print(bufferIn.len_params,DEC);Serial3.print("):");
      printParams(bufferIn.params,bufferIn.len_params,true);
      Serial3.print("\tId:");Serial3.println(bufferIn.Id,DEC);    
    }
  #endif
}

void resetSlaves() {
  digitalWrite(_reset_slaves_pin,false);
  delay(500);
  digitalWrite(_reset_slaves_pin,true);
}
