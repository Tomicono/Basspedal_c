
// Basspedal.ino
// MIDI-Interface for a Basspedal
// original Richard Naegele 28.01.2017


// adapted for my hardware setup
// Thomas Richter

// quite a lot delay on the Midi output

// Version 3.1

// 2.0e > MIDI.h-Syntax angepasst

#include <Arduino.h>
#include <MIDI.h>
#include <Wire.h>
#include <EEPROM.h>
#include <LiquidCrystal_I2C.h>
#include <phi_interfaces.h>
#include <Bounce.h>

MIDI_CREATE_DEFAULT_INSTANCE();
#define Addr_LCD 0x27
LiquidCrystal_I2C lcd(Addr_LCD, 20, 4);


#define encoder0PinA 5     // encoder 0 pin a
#define encoder0PinB 6     // encoder 0 pin b
#define Encoder0Detent 24  // number of detents per rotation (impulse pro umdrehung)

#define switch4 4           // bank a switch
#define switchOct 2         // octave switch
#define switch2 3           // hold switch
#define in_ButtonEncoder 7  // encoder 0 switch
#define ledHold 8           // hold led
#define ledOct 9            // octave led
#define ledPresetGn 11      // preset led
#define ledPresetRt 10      // preset led

#define PRESSED 0x0         // button pressed --> Input low
#define RELEASED 0x1        // button not pressed --> Input open = high

const uint8_t noOfKeys = 13;  // number of keys of bass pedal

// Functions
void readEeprom();
void saveEeprom();
void Panic();
void showMenu();
void readkeys();
void sendMIDI();
void sethold();
void setoctave();
void recBankA();
void setEncoder0Button();
void handleRotaryKnob();
void printPresetName(int i, int j, int k);


char mapping0[] = { 'L', 'R' };  // This is a rotary encoder so it returns L for down/left and R for up/right on the dial.
phi_rotary_encoders my_encoder0(mapping0, encoder0PinA, encoder0PinB, Encoder0Detent);

Bounce pressandhold = Bounce(in_ButtonEncoder, 5);
boolean pah = false;
boolean pahlock = false;
Bounce holdbutton = Bounce(switch2, 30);
Bounce octavebutton = Bounce(switchOct, 30);
Bounce bankabutton = Bounce(switch4, 30);


volatile int encoder0PosM = 0;                              //encoder Menu counter
volatile int encoder0Pos[8] = { 0, 0, 0, 0, 0, -1, 1, 0 };  //encoder counter
volatile int encoder1PosVol = 0;                            //encoder volume-wheel counter
volatile int encoder2PosMod = 0;                            //encoder modulation-wheel counter

volatile boolean holdflag = false;                             //hold function
volatile boolean octflag = false;                              //octave switcher

/// @brief user interface: toggled by pushing the rotary encoder 
/// to enter or leave submenu. Value true indicates submenu is active
volatile boolean bUserIsEditing = false;

volatile int bankflag = 0;                                     //recall preset bank a-c
volatile boolean bouncer[4] = { false, false, false, false };  //toggle functions  0:hold 1:octaveswitch 2: Pushbutton encoder 3: recall preset bank a

int keyispressed[16];  //Is the key currently pressed
unsigned long keytime = 0;
int noteisplaying[noOfKeys];  //Is the Note currently playing?
boolean anynoteisplaying = false;

int octave    = 2;      //set currently played octave
int transpose = 0;   //transpose notes on the board
int velocity  = 127;  //set note velocity
int volume    = 127;    //set volume
int modulation = 0;  //set volume
int channel = 1;     //set midi channel
int prgchange = -1;  //
int notehold = 129;  //flag for hold function
int iCurrProgram = 0;     //recall preset number
int rP = 0;
int saveP = 0;       //save preset number
unsigned long menutimeout = 0;  // skip to menulevel 0 after the specified time, when the encoder was turned

// note font
byte notepic[8] = {
  B00110,
  B00101,
  B00100,
  B00100,
  B00100,
  B11100,
  B11100,
  B11100
};
// block font1
byte barpic1[8] = {
  B10000,
  B10000,
  B10000,
  B10000,
  B10000,
  B10000,
  B10000,
  B10000
};
// block font2
byte barpic2[8] = {
  B11000,
  B11000,
  B11000,
  B11000,
  B11000,
  B11000,
  B11000,
  B11000
};
// block font3
byte barpic3[8] = {
  B11100,
  B11100,
  B11100,
  B11100,
  B11100,
  B11100,
  B11100,
  B11100
};
// block font4
byte barpic4[8] = {
  B11110,
  B11110,
  B11110,
  B11110,
  B11110,
  B11110,
  B11110,
  B11110
};
// block font5
byte barpic5[8] = {
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111
};
//--- Menu
volatile int menulevel = 0;  //set menu to home
char boottext0[] = "   Bass Pedal       ";
char boottext1[] = "   version tag      ";
char blankline[] = "                    ";
char menutext_RECALL[] = "Recall";
char menutext_OCTAVE[] = "Octave";
char menutext_TRANSPOSE[] = "Transpose";
char menutext_VELOCITY[] = "Velocity";
char menutext_VOLUME[] = "Volume";
char menutext_PRGCHNGE[] = "Program Change";
char menutext_CHANNEL[] = "MIDI Channel";
char menutext_SAVE[] = "Save";

char displayText_PRESET[] = "PRESET ";
char displayText_RECALLED[] = "recalled   ";
char displayText_NOTE[] = "Note ";
char displayText_SAVED[] = "saved      ";

/// @brief 
void setup()
{
  #pragma message(GIT_REV)
  Wire.begin();                                   // setup the I2C bus
  MIDI.begin(MIDI_CHANNEL_OMNI);                  //initialise midi library
  MIDI.setThruFilterMode(midi::Thru::Full);       // Full (every incoming message is sent back)
  MIDI.turnThruOn();
  
  //--- LCD initializing
  lcd.init();
  lcd.backlight();

  //--- Set up pins
  pinMode(encoder0PinA, INPUT);
  pinMode(encoder0PinB, INPUT);
  pinMode(in_ButtonEncoder, INPUT);
  pinMode(switch2, INPUT);
  pinMode(switchOct, INPUT);
  pinMode(switch4, INPUT);
  pinMode(ledHold, OUTPUT);
  pinMode(ledOct, OUTPUT);
  pinMode(ledPresetGn, OUTPUT);
  pinMode(ledPresetRt, OUTPUT);

  digitalWrite(encoder0PinA, HIGH);
  digitalWrite(encoder0PinB, HIGH);
  digitalWrite(in_ButtonEncoder, HIGH);
  digitalWrite(switch2, HIGH);
  digitalWrite(switchOct, HIGH);
  digitalWrite(switch4, HIGH);
  digitalWrite(ledHold, LOW);
  digitalWrite(ledOct, LOW);
  digitalWrite(ledPresetGn, LOW);
  digitalWrite(ledPresetRt, LOW);

  readEeprom();  //load saved values
  Panic();
  lcd.createChar(1, barpic1);  // char(1) is barpic1
  lcd.createChar(2, barpic2);
  lcd.createChar(3, barpic3);
  lcd.createChar(4, barpic4);
  lcd.createChar(5, barpic5);
  
  lcd.createChar(8, notepic); ///< char(8) is note symbol (used when key is pressed)

  //Serial.begin(9600);  //debugmonitor
  //Serial.println("Ready");
  
  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print(boottext0);
  lcd.setCursor(0, 2);
  lcd.print(GIT_REV);
  delay(5000);
  lcd.clear();
  showMenu();
}



/// @brief Arduino Working LOOP
void loop() 
{
  readkeys();
  sendMIDI();
  MIDI.read();
  sethold();
  setoctave();
  recBankA();
  setEncoder0Button();
  handleRotaryKnob();

  // skip back to mainmenu after 30s
  if (millis() == menutimeout + 30000)  // 30 Seconds
  { 
    menulevel = 0;
    encoder0PosM = 0;
    bUserIsEditing = false;
    showMenu();
    menutimeout = millis();
  }
}

//-----------------------------------------------
/// @brief Print values up to 999 
/// @param value value to print
/// @param digits number of digits to print range 1-3
/// @param align 0=right, 1=left
void printValue(int value, int digits, int align) { 
  switch(digits) {
    case 1:
      break;
    case 2:
      if(value < 10) {
        if(!align) lcd.print(" ");
        lcd.print(value);
        if(align) lcd.print(" ");
      }
      else lcd.print(value);
      break;
    case 3:
      if(value == -1) {
        lcd.print("Off");
      }
      else if( value >= 0 && value < 10) {
        if(!align) lcd.print("  ");
        lcd.print(value);
        if(align) lcd.print("  ");
      }
      else if(value > 9 && value < 100) {
        if(!align) lcd.print(" ");
        lcd.print(value);
        if(align) lcd.print(" ");
      }
      else lcd.print(value);
      break;
  }
}


/// @brief Print a bar across a line on the LCD
/// @param column Start column of bar
/// @param row line (1-4)
/// @param length 
/// @param mvalue 
/// @param value 
void barGraph(int column, int row, int length, int mvalue, int value) 
{
  int b, c;
  float a = ((float)length / mvalue) * value;
  lcd.setCursor(column, row);
  // drawing black rectangles on LCD
  if (a >= 1) {
    for (int i = 1; i < a; i++) {
      lcd.print((char)5);
      b = i;
    }
    a = a - b;
  }
  c = a * 5;
  // drawing charater's colums
  switch (c) {
    case 0:
      lcd.print(" ");
      break;
    case 1:
      lcd.print((char)1);
      break;
    case 2:
      lcd.print((char)2);
      break;
    case 3:
      lcd.print((char)3);
      break;
    case 4:
      lcd.print((char)4);
      break;
    case 5:
      lcd.print((char)5);
      break;
  }
  // deleting charater's colums with blanks
  lcd.setCursor(column + b + 1, row);
  for (int i = b + 1; i < length; i++) {
    lcd.print(" ");
  }
}

/// @brief process user turning of the rotary encoder throughout
/// the menus and update LCD accordingly
void handleRotaryKnob() {
  char temp;
  temp = my_encoder0.getKey();
  //temp = dial0 -> getKey(); // Use the phi_interfaces to access the same keypad
  if (bUserIsEditing == false) {
    if (temp == 'R') {
      menutimeout = millis();
      if (encoder0PosM < 7) {
        encoder0PosM++;
        menulevel = encoder0PosM;
        showMenu();
      }
    }
    if (temp == 'L') {
      menutimeout = millis();
      if (encoder0PosM > 0) {
        encoder0PosM--;
        menulevel = encoder0PosM;
        showMenu();
      }
    }
  }
  else {
    if (menulevel == 0) {
      encoder0Pos[0] = rP;
      if (temp == 'R') {
        menutimeout = millis();
        if (encoder0Pos[0] < 38) {
          encoder0Pos[0]++;
          rP = encoder0Pos[0];
          lcd.setCursor(1, 1);
          lcd.print(displayText_PRESET);  // "Preset "
          printPresetName(8, 1, rP);
        }
      }
      if (temp == 'L') {
        menutimeout = millis();
        if (encoder0Pos[0] > 0) {
          encoder0Pos[0]--;
          rP = encoder0Pos[0];
          lcd.setCursor(1, 1);
          lcd.print(displayText_PRESET);  // "Preset "
          printPresetName(8, 1, rP);
        }
      }
    }
    if (menulevel == 1) {
      encoder0Pos[1] = octave;
      if (temp == 'R') {
        menutimeout = millis();
        if (encoder0Pos[1] < 10) {
          encoder0Pos[1]++;
          octave = encoder0Pos[1];
          lcd.setCursor(1, 1);
          lcd.print("C");
          lcd.print(octave - 1);
          if (octave > 0) lcd.print(" ");
        }
      }
      if (temp == 'L') {
        menutimeout = millis();
        if (encoder0Pos[1] > 0) {
          encoder0Pos[1]--;
          octave = encoder0Pos[1];
          lcd.setCursor(1, 1);
          lcd.print("C");
          lcd.print(octave - 1);
          if (octave > 0) lcd.print(" ");
        }
      }
    }
    if (menulevel == 2) {
      encoder0Pos[2] = transpose;
      if (temp == 'R') {
        menutimeout = millis();
        if (encoder0Pos[2] < 12) {
          encoder0Pos[2]++;
          transpose = encoder0Pos[2];
          lcd.setCursor(1, 1);
          lcd.print(displayText_NOTE);  // "Note "
          if (transpose > 0) lcd.print("+");
          lcd.print(transpose);
          if (transpose <= 9 && transpose >= -9) lcd.print(" ");
        }
      }
      if (temp == 'L') {
        menutimeout = millis();
        if (encoder0Pos[2] > -12) {
          encoder0Pos[2]--;
          transpose = encoder0Pos[2];
          lcd.setCursor(1, 1);
          lcd.print(displayText_NOTE);  // "Note "
          if (transpose > 0) lcd.print("+");
          lcd.print(transpose);
          if (transpose <= 9 && transpose >= -9) lcd.print(" ");
        }
      }
    }
    if (menulevel == 3) {
      encoder0Pos[3] = velocity;
      if (temp == 'R') {
        menutimeout = millis();
        if (encoder0Pos[3] < 127) {
          encoder0Pos[3]++;
          velocity = encoder0Pos[3];
          lcd.setCursor(1, 1);
          printValue(velocity, 3, 1);
        }
      }
      if (temp == 'L') {
        menutimeout = millis();
        if (encoder0Pos[3] > 0) {
          encoder0Pos[3]--;
          velocity = encoder0Pos[3];
          lcd.setCursor(1, 1);
          printValue(velocity, 3, 1);
        }
      }
    }
    if (menulevel == 4) {
      encoder0Pos[4] = volume;
      if (temp == 'R') {
        menutimeout = millis();
        if (encoder0Pos[4] < 127) {
          encoder0Pos[4]++;
          volume = encoder0Pos[4];
          MIDI.sendControlChange(7, volume, channel);
          lcd.setCursor(1, 1);
          printValue(volume, 3, 1);
          lcd.setCursor(0, 3);
          lcd.print("V");
          barGraph(1, 3, 16, 127, volume);
          lcd.setCursor(17, 3);
          printValue(volume, 3, 0);
        }
      }
      if (temp == 'L') {
        menutimeout = millis();
        if (encoder0Pos[4] > 0) {
          encoder0Pos[4]--;
          volume = encoder0Pos[4];
          MIDI.sendControlChange(7, volume, channel);
          lcd.setCursor(1, 1);
          printValue(volume, 3, 1);
          lcd.setCursor(0, 3);
          lcd.print("V");
          barGraph(1, 3, 16, 127, volume);
          lcd.setCursor(17, 3);
          printValue(volume, 3, 0);
        }
      }
    }
    if (menulevel == 5) {
      encoder0Pos[5] = prgchange;
      if (temp == 'R') {
        menutimeout = millis();
        if (encoder0Pos[5] < 127) {
          encoder0Pos[5]++;
          prgchange = encoder0Pos[5];
          lcd.setCursor(1, 1);
          printValue(prgchange, 3, 1);
          if (prgchange > -1) MIDI.sendProgramChange(prgchange, channel);
        }
      }
      if (temp == 'L') {
        menutimeout = millis();
        if (encoder0Pos[5] > -1) {
          encoder0Pos[5]--;
          prgchange = encoder0Pos[5];
          lcd.setCursor(1, 1);
          printValue(prgchange, 3, 1);
          if (prgchange > -1) MIDI.sendProgramChange(prgchange, channel);
        }
      }
    }
    if (menulevel == 6) {
      encoder0Pos[6] = channel;
      if (temp == 'R') {
        menutimeout = millis();
        if (encoder0Pos[6] < 16) {
          encoder0Pos[6]++;
          channel = encoder0Pos[6];
          lcd.setCursor(1, 1);
          printValue(channel, 2, 1);
        }
      }
      if (temp == 'L') {
        menutimeout = millis();
        if (encoder0Pos[6] > 1) {
          encoder0Pos[6]--;
          channel = encoder0Pos[6];
          lcd.setCursor(1, 1);
          printValue(channel, 2, 1);
        }
      }
    }
    if (menulevel == 7) {
      encoder0Pos[7] = saveP;
      if (temp == 'R') {
        menutimeout = millis();
        if (encoder0Pos[7] < 38) {
          encoder0Pos[7]++;
          saveP = encoder0Pos[7];
          lcd.setCursor(1, 1);
          lcd.print(displayText_PRESET);  // "Preset "
          printPresetName(8, 1, saveP);
        }
      }
      if (temp == 'L') {
        menutimeout = millis();
        if (encoder0Pos[7] > 0) {
          encoder0Pos[7]--;
          saveP = encoder0Pos[7];
          lcd.setCursor(1, 1);
          lcd.print(displayText_PRESET);  // "Preset "
          printPresetName(8, 1, saveP);
        }
      }
    }
  }
}

/// @brief user interface: set symbol ">" at the active line
/// used to mark the line where editing is done
void printActiveLineMarker() {
  if (bUserIsEditing == false) {
    lcd.setCursor(0, 1);
    lcd.print(" ");

    lcd.setCursor(0, 0);
    lcd.print(">");
  }
  else {
    lcd.setCursor(0, 0);
    lcd.print(" ");
    lcd.setCursor(0, 1);
    lcd.print(">");
  }
}
//-------------------------------------------
void showMenu() {

  switch (menulevel) {
    case 0:
      lcd.setCursor(0, 0);
      lcd.print(blankline);
      lcd.setCursor(0, 1);
      lcd.print(blankline);
      printActiveLineMarker();
      lcd.setCursor(1, 0);
      lcd.print(menutext_RECALL);
      // text octave
      lcd.setCursor(12, 1);
      lcd.print("C");
      lcd.print(octave - 1);
      if (octave > 0) lcd.print(" ");
      // text transpose
      lcd.setCursor(16, 1);
      lcd.print("T");
      if (transpose > 0) {
        lcd.setCursor(17, 1);
        lcd.print("+");
        lcd.print(transpose);
      }
      else {
        lcd.setCursor(17, 1);
        lcd.print(transpose);
      }
      // text 2. line
      lcd.setCursor(1, 1);
      lcd.print(displayText_PRESET);
      if (bUserIsEditing == true) printPresetName(8, 1, rP);
      else printPresetName(8, 1, iCurrProgram);
      // presetbank
      if (bankflag > 0) {
        lcd.setCursor(18, 0);
        if (bankflag == 1) lcd.print("A");
        if (bankflag == 2) lcd.print("B");
        if (bankflag == 3) lcd.print("C");
      }
      else {
        lcd.setCursor(18, 0);
        lcd.print(" ");
      }
      // notepic
      if (anynoteisplaying) {
        lcd.setCursor(19, 0);
        lcd.print((char)8);
      }
      else {
        lcd.setCursor(19, 0);
        lcd.print(" ");
      }
      // text prg change
      lcd.setCursor(12, 0);
      lcd.print("P");
      lcd.setCursor(13, 0);
      printValue(prgchange, 3, 1);
      // bargraph modulation
      lcd.setCursor(0, 2);
      lcd.print("M");
      barGraph(1, 2, 16, 127, modulation);
      lcd.setCursor(17, 2);
      printValue(modulation, 3, 0);
      // bargraph volume
      lcd.setCursor(0, 3);
      lcd.print("V");
      barGraph(1, 3, 16, 127, volume);
      lcd.setCursor(17, 3);
      printValue(volume, 3, 0);
      break;
    case 1:
      lcd.setCursor(0, 0);
      lcd.print(blankline);
      lcd.setCursor(0, 1);
      lcd.print(blankline);
      printActiveLineMarker();
      lcd.setCursor(1, 0);
      lcd.print(menutext_OCTAVE);
      lcd.setCursor(1, 1);
      lcd.print("C");
      lcd.print(octave - 1);
      break;
    case 2:
      lcd.setCursor(0, 0);
      lcd.print(blankline);
      lcd.setCursor(0, 1);
      lcd.print(blankline);
      printActiveLineMarker();
      lcd.setCursor(1, 0);
      lcd.print(menutext_TRANSPOSE);
      lcd.setCursor(1, 1);
      lcd.print(displayText_NOTE);
      if (transpose > 0) {
        lcd.setCursor(6, 1);
        lcd.print("+");
        lcd.setCursor(7, 1);
        lcd.print(transpose);
      }
      else {
        lcd.setCursor(6, 1);
        lcd.print(transpose);
      }
      break;
    case 3:
      lcd.setCursor(0, 0);
      lcd.print(blankline);
      lcd.setCursor(0, 1);
      lcd.print(blankline);
      printActiveLineMarker();
      lcd.setCursor(1, 0);
      lcd.print(menutext_VELOCITY);
      lcd.setCursor(1, 1);
      printValue(velocity, 3, 1);
      break;
    case 4:
      lcd.setCursor(0, 0);
      lcd.print(blankline);
      lcd.setCursor(0, 1);
      lcd.print(blankline);
      printActiveLineMarker();
      lcd.setCursor(1, 0);
      lcd.print(menutext_VOLUME);
      lcd.setCursor(1, 1);
      printValue(volume, 3, 1);
      break;
    case 5:
      lcd.setCursor(0, 0);
      lcd.print(blankline);
      lcd.setCursor(0, 1);
      lcd.print(blankline);
      printActiveLineMarker();
      lcd.setCursor(1, 0);
      lcd.print(menutext_PRGCHNGE);
      lcd.setCursor(1, 1);
      printValue(prgchange, 3, 1);
      break;
    case 6:
      lcd.setCursor(0, 0);
      lcd.print(blankline);
      lcd.setCursor(0, 1);
      lcd.print(blankline);
      printActiveLineMarker();
      lcd.setCursor(1, 0);
      lcd.print(menutext_CHANNEL);
      lcd.setCursor(1, 1);
      printValue(channel, 2, 1);
      break;
    case 7:
      lcd.setCursor(0, 0);
      lcd.print(blankline);
      lcd.setCursor(0, 1);
      lcd.print(blankline);
      printActiveLineMarker();
      lcd.setCursor(1, 0);
      lcd.print(menutext_SAVE);
      lcd.setCursor(1, 1);
      lcd.print(displayText_PRESET);
      printPresetName(8, 1, rP);  // Save Preset springt auf den Speicherplatz vom aktivem Recall Preset
      saveP = rP;           // Recall Preset an Save Preset übergeben
      break;
  }
}

/// @brief Print preset name at given screen position
/// @param iPosX first char 
/// @param iPosY line 
/// @param iPreset index of preset, splitted in A0-A12 B0-12, C0-C12
void printPresetName(int iPosX, int iPosY, int iPreset) {  
  if (iPreset >= 0 && iPreset < 13) {
    lcd.setCursor(iPosX, iPosY);
    lcd.print("A");
    lcd.print(iPreset);
    if (iPreset <= 9) lcd.print(" ");
  }
  if (iPreset > 12 && iPreset < 26) {
    lcd.setCursor(iPosX, iPosY);
    lcd.print("B");
    lcd.print(iPreset - 13);
    if (iPreset - 13 <= 9) lcd.print(" ");
  }
  if (iPreset > 25 && iPreset < 39) {
    lcd.setCursor(iPosX, iPosY);
    lcd.print("C");
    lcd.print(iPreset - 26);
    if (iPreset - 26 <= 9) lcd.print(" ");
  }
}
//-------------------------------------
// Read the pressed keys into
// keyispressed[]
// from state of the I2C chips. 1 is open, 0 is closed.
void readkeys() 
{  //Read the state of the I2C chips. 1 is open, 0 is closed.

  unsigned char data[2];  //data from chip 1/2/3

  Wire.requestFrom(0x38, 1);  // read the data from chip 1 in data[0]
  if (Wire.available()) 
    data[0] = Wire.read();
  
  Wire.requestFrom(0x39, 1);  // read the data freom chip 2 into data[1]
  if (Wire.available()) 
    data[1] = Wire.read();
  
  // copy key state to global variable
  for (unsigned char i = 0; i < 8; i++) 
  {        
    keyispressed[i] = ((data[0] >> i) & 1);      // set the key variable to the current state. chip 1
    keyispressed[i + 8] = ((data[1] >> i) & 1);  //chip 2
  }
}
// toggle holdflag on button_press_event
// use number "notehold" as offset to actual played note
//----------------------------------------
void sethold() 
{
  holdbutton.update();
  if (holdbutton.read() == PRESSED) 
  {
    if (holdflag == false && bouncer[0] == true) 
    {
      holdflag = true;
      bouncer[0] = false;
      notehold = 129;
      digitalWrite(ledHold, HIGH);
      showMenu();
    }
    if (holdflag == true && bouncer[0] == true) 
    {
      holdflag = false;
      bouncer[0] = false;
      MIDI.sendNoteOff(notehold - 1, 0, channel);
      anynoteisplaying = false;
      digitalWrite(ledHold, LOW);
      showMenu();
    }
  } 
  else 
  {
    bouncer[0] = true;
  }
}

// wenn octavebutton press_event
// toggle oct_flag
// bouncer[1] = true, sobald button losgelassen
// wird verwendet, um Flanke "pressed" zu erkennen

//----------------------------------------
void setoctave() {
  octavebutton.update();
  if (octavebutton.read() == PRESSED) 
  {
    if (octflag == false && bouncer[1] == true) 
    {
      bankflag = false;
      octflag = true;
      bouncer[1] = false;
      digitalWrite(ledOct, HIGH);
    }
    if (octflag == true && bouncer[1] == true) 
    {
      octflag = false;
      bouncer[1] = false;
      digitalWrite(ledOct, LOW);
    }
  } 
  else  // octavebutton Released
  {
    bouncer[1] = true;
  }
}


//----------------------------------------
void setEncoder0Button() {
  pressandhold.update();
  if (pressandhold.read() != pah && pressandhold.duration() > 200) {
    pah = pressandhold.read();
    if (!pah) {
      if (menulevel == 0 || menulevel == 7) {
        lcd.setCursor(0, 1);
        lcd.print("          ");
        pah = !pah;
      }
    }
  }
  if (pressandhold.read() != pah && pressandhold.duration() > 300) {
    pah = pressandhold.read();
    if (!pah) {
      if (menulevel == 0) {
        lcd.setCursor(0, 0);
        lcd.print(displayText_PRESET);  // "Preset "
        iCurrProgram = rP;
        readEeprom();
        printPresetName(7, 0, iCurrProgram);
        Panic();
        lcd.setCursor(0, 1);
        lcd.print(displayText_RECALLED);
        bUserIsEditing = false;
        delay(2000);
        showMenu();
      }
      if (menulevel == 7) {
        lcd.setCursor(0, 0);
        lcd.print(displayText_PRESET);  // "Preset "
        saveEeprom();
        printPresetName(7, 0, saveP);
        lcd.setCursor(0, 1);
        lcd.print(displayText_SAVED);
        bUserIsEditing = false;
        delay(2000);
        menulevel = 0;  // jump back to play menu
        encoder0PosM = menulevel;
        showMenu();
      }
    }
  } else {
    if (digitalRead(in_ButtonEncoder) == PRESSED) {
      if (bUserIsEditing == false && bouncer[2] == true) {
        bUserIsEditing = true;
        bouncer[2] = false;
        rP = iCurrProgram;
        printActiveLineMarker();
        digitalWrite(ledPresetGn, LOW);
        digitalWrite(ledPresetRt, LOW);
        bankflag = 0;
        digitalWrite(ledHold, LOW);
        holdflag = 0;
        digitalWrite(ledOct, LOW);
        octflag = 0;
        lcd.setCursor(18, 0);
        lcd.print(" ");
      }
      if (bUserIsEditing == true && bouncer[2] == true) {
        bUserIsEditing = false;
        bouncer[2] = false;
        showMenu();
      }
    } else {
      bouncer[2] = true;
    }
  }
}

// switches with every button_press_event
// from Bank A to B to C 
// with is stored in bankflag 1,2,3 and off is 0

//----------------------------------------
void recBankA() {
  bankabutton.update();
  if (bankabutton.read() == PRESSED) {
    if (bankflag == 0 && bouncer[3] == true) {
      octflag = false;
      bankflag = 1;
      bouncer[3] = false;
      digitalWrite(ledPresetGn, HIGH);
      lcd.setCursor(18, 0);
      lcd.print("A");
    }
    if (bankflag == 1 && bouncer[3] == true) {
      octflag = false;
      bankflag = 2;
      bouncer[3] = false;
      digitalWrite(ledPresetGn, LOW);
      digitalWrite(ledPresetRt, HIGH);
      lcd.setCursor(18, 0);
      lcd.print("B");
    }
    if (bankflag == 2 && bouncer[3] == true) {
      octflag = false;
      bankflag = 3;
      bouncer[3] = false;
      digitalWrite(ledPresetGn, HIGH);
      lcd.setCursor(18, 0);
      lcd.print("C");
    }
    if (bankflag > 2 && bouncer[3] == true) {
      bankflag = 0;
      bouncer[3] = false;
      digitalWrite(ledPresetGn, LOW);
      digitalWrite(ledPresetRt, LOW);
      lcd.setCursor(18, 0);
      lcd.print(" ");
    }
  } else {
    bouncer[3] = true;
  }
}
//-------------------------------------
// Send MIDI instructions via MIDI out
// makes no sense to use 
///-----------------------------
void sendMIDI() {  
  int note;            // absolute value of played note (0= C0, note 127 = G10)
  //for each key of the keyboad 
  for (unsigned int iKeyOfBoard = 0; iKeyOfBoard < noOfKeys; iKeyOfBoard++) {  
    if (keyispressed[iKeyOfBoard] == PRESSED) 
    {          //the key on the board is pressed
      if (bankflag > 0) {
        if (bankflag == 1) iCurrProgram = iKeyOfBoard;    // BUG: No sense to mix program with note played
        if (bankflag == 2) iCurrProgram = iKeyOfBoard + 13;
        if (bankflag == 3) iCurrProgram = iKeyOfBoard + 26;
        rP = iCurrProgram;
        readEeprom();
        showMenu();
        lcd.setCursor(18, 0);
        lcd.print("  ");
        digitalWrite(ledPresetGn, LOW);
        digitalWrite(ledPresetRt, LOW);
        holdflag = false;
        digitalWrite(ledHold, LOW);
        bankflag = 0;
        Panic();
        delay(1000);
        break;
      }
      if (octflag == true && iKeyOfBoard < 11) {
        octave = iKeyOfBoard;
        showMenu();
        digitalWrite(ledOct, LOW);
        octflag = false;
        delay(1000);
        break;
      } else {
        if (!noteisplaying[iKeyOfBoard]) {  //if the note is not already playing send MIDI instruction to start the note
          note = iKeyOfBoard + (octave * 12) + transpose + 1; // why +1?
          if (note < 129 && note > 0) {
            if (holdflag == true && notehold < 129) {  //if hold funcion active note off will send before starting the new note
              MIDI.sendNoteOff(notehold - 1, 0, channel);
              keytime = millis();
              //Serial.print("NoteOff ");
              //Serial.println(notehold-1);
            }
            MIDI.sendNoteOn(note - 1, velocity, channel);  // Send a Note
            noteisplaying[iKeyOfBoard] = note;                       // set the note playing flag to TRUE and store the note value
            notehold = note;                               //buffer the old holded note
            keytime = millis();
            anynoteisplaying = true;
            lcd.setCursor(19, 0);
            lcd.print((char)8);
            //Serial.print("NoteOn ");
            //Serial.print(channel);
            //Serial.println(note-1);
          }
        }
      }
    } 
    else 
    {                                               // key is released
      if (noteisplaying[iKeyOfBoard] && millis() > keytime + 100) {  //if the note is currently playing, turn it off
        note = noteisplaying[iKeyOfBoard];                           //retrieve the saved note value incase the octave has changed
        if (holdflag == false) {                           //send note off when hold function is inactive
          MIDI.sendNoteOff(note - 1, 0, channel);          // Stop the note
          keytime = millis();
          anynoteisplaying = false;
          lcd.setCursor(19, 0);
          lcd.print(" ");
          //Serial.print("NoteOff ");
          //Serial.println(note-1);
        }
        noteisplaying[iKeyOfBoard] = 0;  // clear the note is playing flag
      }
    }
  }
}
//----------------------------------------------------------
void saveEeprom() {
  int prg, trans;
  if (octave != EEPROM.read(saveP * 6)) EEPROM.write(saveP * 6, octave);
  if (transpose >= 0 && transpose < 13) trans = transpose;
  else trans = transpose + 30;  // Shift negative value to a positive above 12
  if (trans != EEPROM.read(saveP * 6 + 1)) EEPROM.write(saveP * 6 + 1, trans);
  if (velocity != EEPROM.read(saveP * 6 + 2)) EEPROM.write(saveP * 6 + 2, velocity);
  if (volume != EEPROM.read(saveP * 6 + 3)) EEPROM.write(saveP * 6 + 3, volume);
  if (prgchange == -1) prg = 128;  // Prg Change Off
  else prg = prgchange;
  if (prg != EEPROM.read(saveP * 6 + 4)) EEPROM.write(saveP * 6 + 4, prg);
  if (channel != EEPROM.read(saveP * 6 + 5)) EEPROM.write(saveP * 6 + 5, channel);
}
//----------------------------------------------------------
void readEeprom() {
  int prg, trans;

  if (EEPROM.read(iCurrProgram * 6) < 10 && EEPROM.read(iCurrProgram * 6) >= 0) {
    octave = EEPROM.read(iCurrProgram * 6);
  }
  if (EEPROM.read(iCurrProgram * 6 + 1) < 30 && EEPROM.read(iCurrProgram * 6 + 1) >= 0) {
    trans = EEPROM.read(iCurrProgram * 6 + 1);
    if (trans >= 0 && trans < 13) transpose = trans;
    else transpose = trans - 30;  // Shift the positive value back to a negative
  }
  if (EEPROM.read(iCurrProgram * 6 + 2) < 128 && EEPROM.read(iCurrProgram * 6 + 2) >= 0) {
    velocity = EEPROM.read(iCurrProgram * 6 + 2);
  }
  if (EEPROM.read(iCurrProgram * 6 + 3) < 128 && EEPROM.read(iCurrProgram * 6 + 3) >= 0) {
    volume = EEPROM.read(iCurrProgram * 6 + 3);
    MIDI.send(midi::MidiType::ControlChange, 7, volume, channel);
  }
  if (EEPROM.read(iCurrProgram * 6 + 4) < 129 && EEPROM.read(iCurrProgram * 6 + 4) >= 0) {
    prg = EEPROM.read(iCurrProgram * 6 + 4);
    if (prg == 128) prgchange = -1;
    else {
      prgchange = prg;
      MIDI.sendProgramChange(prgchange, channel);
    }
  }
  if (EEPROM.read(iCurrProgram * 6 + 5) < 17 && EEPROM.read(iCurrProgram * 6 + 5) > 0) {
    channel = EEPROM.read(iCurrProgram * 6 + 5);
  }
  modulation = 0;
}


/// @brief resets all notes and sends "NOTE_OFF" command for every possible note on MIDI
void Panic() {
  
  for (unsigned int j = 0; j < noOfKeys; j++) 
  {  
    keyispressed[j] = RELEASED;            // clear the keys array (High is off)
    noteisplaying[j] = 0;                  // no notes are playing
  }
  
  for (unsigned int j = 0; j < 128; j++) 
  {
    MIDI.sendNoteOff(j, 0, channel);
  }
}
