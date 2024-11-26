/********************************* */
/*      Storage Functions          */
/*  functions to handle eprom      */
/*  storage                        */
/********************************* */



#ifndef STORAGEFUNCTIONS_h
#define STORAGEFUNCTIONS_h

#include <EEPROM.h>
typedef struct
{
  // removed prgName, would need functions to enter name
  // too much effort
  // char prgName[4];       // programm name 
  int8_t octave;            // set currently played octave
  int8_t transpose;         // transpose notes on the board
  int8_t velocity;          // set note velocity
  uint8_t volume;            // set volume
  uint8_t channel;           // set midi channel
  int8_t prgchange;         //
  uint8_t checksum; // sum without carry over of all bytes in struct including this must equal zero
} T_Setting;

const int ERR_NO_ERROR = 0;
const int ERR_OUT_OF_RANGE = -10;
const int ERR_CHECKSUM_INVALID = -20;

int epromFixFormat(int *);
int epromGetElement(T_Setting *item, unsigned int index);
int epromSetElement(T_Setting item, unsigned int index);

const int eprom_Items = 3 * 13; // 13, because user can select A1..A13 with the keypad
const char epromHeader[4] = {'B', 'P', '0', '1'};
const int epromHeaderSize = sizeof(epromHeader);
const int epromItemSize = sizeof(T_Setting);

#endif
