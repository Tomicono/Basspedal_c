#include <Arduino.h>
#include "StorageFunctions.h"


T_Setting t_Eprom_Item;


void epromPrintContent(void);

//int epromFormat();
int epromnInitItem(unsigned int index);
void createDefaultElement(T_Setting *item);
void PrintItem(T_Setting);

unsigned int checksum_calc(T_Setting);
void checksum_set(T_Setting*);

void testRun()
{
  int error = 0;
  int nfixedElements = 0;
  T_Setting testItem;
  // initialize serial and wait for port to open:
  Serial.begin(9600);
  while (!Serial)
  {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  
  createDefaultElement(&testItem);

  Serial.println("Start");
  Serial.println("Default Item:");
  // PrintItem(testItem);
  // Serial.print("Check sum: ");
  // Serial.println(checksum_calc(testItem));

  testItem.octave  =  'O'; // just for read in debug ascii
  testItem.transpose  = 'T'; 
  testItem.velocity  = 'v'; 
  testItem.volume  = 'V'; 
  testItem.channel ='C';
  testItem.prgchange  = 'E'; 
  checksum_set(&testItem);

  Serial.println("TestItem:");
  PrintItem(testItem);
  Serial.print("Check sum: ");
  Serial.println(checksum_calc(testItem));

  error = epromFixFormat(&nfixedElements);
  Serial.print("epromFixFormat: error = ");
  Serial.print(error);
  Serial.print(" fixed items: ");
  Serial.print(nfixedElements);
  Serial.println();

  Serial.println("Writing elements: ");
  for (int i = 0; i < eprom_Items; i++)
  {
    // prepare item
    testItem.octave = (i / 10)+ 0x30; 
    testItem.transpose = (i % 10)+ 0x30; 
    testItem.velocity = 'T';
    testItem.volume = '+';
    testItem.channel = '1';
    testItem.prgchange = '8';
    checksum_set(&testItem);

    // write item
    error = epromSetElement(testItem, i);

  }
  

  Serial.println("reading all ");
    
  epromPrintContent();
  

}


/// @brief print Eprom Content as raw data to serial out
void epromPrintContent()
{
  int chunkSize = 16; // Print 16 bytes per line
  int chunks = 0;     // Needed Chunks to read whole eprom
  int chunk = 0;      // processed chunk
  int i = 0;          // index within chunk

  chunks = EEPROM.length() / chunkSize;

  Serial.println("Dump of EEPROM content of this Arduino");

  Serial.print("Eprom Size: ");
  Serial.println(EEPROM.length(), DEC);

  Serial.print("Output will be ");
  Serial.print(chunks);
  Serial.println(" lines");
  Serial.println();

  // Do all chunks = blocks of 16 bytes
  for (chunk = 0; chunk < chunks; chunk++)
  {
    // Print adress
    Serial.print(chunk * chunkSize, HEX);

    // do the 16 bytes of this block
    for (i = 0; i < chunkSize; i++)
    {
      int address = (chunk * chunkSize) + i; // adress to read
      Serial.print("\t");
      Serial.print(EEPROM[address], HEX);
    }
    // do the 16 bytes as ASCII
    for (i = 0; i < chunkSize; i++)
    {
      int address = (chunk * chunkSize) + i; // adress to read
      Serial.print("\t");
      Serial.print((char)EEPROM[address]);
    }
    Serial.print("\n");
  }

  Serial.println("Finished");
}

/// @brief check if epromformat is ok
/// check header and check all checksums
/// if header is not equal to "epromHeader" then: do nothing
/// it would require a conversion stragegy to upgrade a format definition
///
/// it replace all items with invalid checksums with default walues
/// @param numOfFixedItems count of fixed memeory entries
/// @return headerNotOk header not ok
int epromFixFormat(int * numOfFixedItems)
{
  
  T_Setting item;
  char myEpromHeader[epromHeaderSize];
  int headerNotOk = 0;

  // step 1: Check header
  eeprom_read_block(myEpromHeader, 0, epromHeaderSize);
  headerNotOk = memcmp(myEpromHeader, epromHeader, epromHeaderSize);

  // step 2: Check checksum
  numOfFixedItems = 0;
  for (uint8_t i = 0; i < eprom_Items; i++)
  {
    int ret = 0;
    ret = epromGetElement(&item, i);
    if (ret == ERR_CHECKSUM_INVALID)
    {
      epromnInitItem(i);
      numOfFixedItems++;
    }
  }
  return headerNotOk;
}

/// @brief write default values to one element of preset
///  int octave = 2;
/// int transpose = 0
/// int velocity = 127
/// int volume = 127
/// int channel = 1
/// int prgchange = -1
/// @param index
/// @return error code
int epromnInitItem(unsigned int index)
{
  T_Setting defaultElement;

  // exit if given index is ouf of range
  if (index >= eprom_Items)
    return ERR_OUT_OF_RANGE;

  createDefaultElement(&defaultElement);
  epromSetElement(defaultElement, index);

  return ERR_NO_ERROR;
}

/// @brief create a T_Setting item with default values for a new item
/// @param item fills one T_Setting item with default values
void createDefaultElement(T_Setting *item)
{
  T_Setting defaultElement = {
    .octave = 2, 
    .transpose = 0, 
    .velocity =  127, 
    .volume =  127, 
    .channel = 1, 
    .prgchange = -1, 
    .checksum = 0 ///< important: init checksum with 0, because is also added during the checksum calculation
    }; 
  checksum_set(&defaultElement);
  memcpy(item, &defaultElement, epromItemSize);
}

int epromGetElement(T_Setting *item, unsigned int index)
{
  int error = ERR_NO_ERROR;
  T_Setting localItem;
  int targetAdress;

  // exit if given index is ouf of range
  if (index >= eprom_Items)
    return ERR_OUT_OF_RANGE;

  targetAdress = (index * epromItemSize) + epromHeaderSize;

  eeprom_read_block(&localItem, (void *)(targetAdress), epromItemSize);
  if (checksum_calc(localItem) == 0)
  {
    memcpy(item, &localItem, epromItemSize);
    error = ERR_NO_ERROR;
  }
  else
  {
    createDefaultElement(item); // return valid default element
    error = ERR_CHECKSUM_INVALID;
  }
  return error;
}

int epromSetElement(T_Setting item, unsigned int index)
{
  int targetAdress;
  targetAdress = (index * epromItemSize) + epromHeaderSize;

  eeprom_write_block(&item, (void *)(targetAdress), epromItemSize);
  return 0;
}

unsigned int checksum_calc(T_Setting item)
{
  uint8_t sum = 0;
  unsigned char *p = (unsigned char *)&item;
  //Serial.println("Checksum");

  for (unsigned i = 0; i < sizeof(T_Setting); i++)
  {
    sum += p[i];
   /*  Serial.print("i ");Serial.print(i);
    Serial.print(" = ");Serial.print(p[i],HEX);
    Serial.print(" CS = ");Serial.print(sum, HEX);
    Serial.println(); */
  }
  return sum;

}

void checksum_set(T_Setting* item)
{
  uint8_t temp_cs;

  // the default vales with cchecksum 0 result a number, but checksum shall be set, so calculation with valid
  // valid checksum is zero
  item->checksum = 0;
  
  temp_cs = checksum_calc(*item);
  item->checksum = 0 - temp_cs;
}

void PrintItem(T_Setting item)
{
  
  int i;
  unsigned char *p = (unsigned char *)&item;

  // Header line
  for (i = 0; i < epromItemSize; i++)
  {

    Serial.print("\t");
    Serial.print(i, HEX);
  }
  Serial.print("\n");
  
  for (unsigned int i = 0; i < sizeof(T_Setting); i++)
  {
    Serial.print("\t");
    Serial.print(p[i], HEX);
  }
  Serial.print("\n");
}
