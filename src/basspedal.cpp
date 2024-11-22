
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
void readKeyboard();
void sendMIDI();
void sethold();
void setoctave();
void setBankOfProgramToLoad();
void handleEnc0ButtonPressEvent();
void handleRotaryKnob();
void printPresetName(int , int , int );
void abortActiveUserButtonInputs();
void setBankModeOutputs();
void userInputPropertyValue(int *target, int min, int max, bool *valueChanged);

char mapping0[] = { 'L', 'R' };  // This is a rotary encoder so it returns L for down/left and R for up/right on the dial.
phi_rotary_encoders my_encoder0(mapping0, encoder0PinA, encoder0PinB, Encoder0Detent);

Bounce encoder0Button = Bounce(in_ButtonEncoder, 30);
Bounce holdButton = Bounce(switch2, 30);
Bounce octaveButton = Bounce(switchOct, 30);
Bounce bankButton = Bounce(switch4, 30);


volatile int iEnc0Value = 0;  // rotory encoder value
//volatile int enc0ValueInMenu[8] = { 0, 0, 0, 0, 0, -1, 1, 0 };  //encoder value of each menu level

volatile boolean bHoldModeIsActive = false;                             //hold function
volatile boolean bOctaveSelectModeIsActive = false;                              //octave switcher

/// @brief user interface: toggled by pushing the rotary encoder 
/// to enter or leave submenu. Value true indicates submenu is active
volatile boolean bUserIsEditing = false;

volatile int iActiveBankSelection = 0;  
volatile boolean bPreviousState_BtnHold = RELEASED;
volatile boolean bPreviousState_BtnOctave = RELEASED;
volatile boolean bPreviousState_BtnEncoder = RELEASED;
volatile boolean bPreviousState_BtnBank = RELEASED;

int keyispressed[16];  //Is the key currently pressed?
unsigned long keytime = 0; // timestamp of last note ON/OFF event
int noteisplaying[noOfKeys];  //Is the Note currently playing?
boolean anynoteisplaying = false; // only used show note symbol in display

int octave    = 2;      //set currently played octave
int transpose = 0;   //transpose notes on the board
int velocity  = 127;  //set note velocity
int volume    = 127;    //set volume
int modulation = 0;  //set volume
int channel = 1;     //set midi channel
int prgchange = -1;  //
int notehold = 129;  //flag for hold function
int iCurrProgram = 0;     //recall preset number
int iUserInputProgramNumber = 0;
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

/// @brief 
enum e_menulevel
{
  eMenuRecall = 0,
  eMenuOctave,
  eMenuTranspose,
  eMenuVelocity,
  eMenuVolume,
  eMenuProgramChange,
  eMenuChannel,
  eMenuSave
};
volatile e_menulevel menulevel = eMenuRecall;  
const int ieMenuFirstItem = static_cast<int>(eMenuRecall);
const int ieMenuLastItem = static_cast<int>(eMenuSave);

char boottext0[] = "   Bass Pedal       ";
char boottext1[] = "   version tag      ";
char blankline[] = "                    ";
char countline[] = "12345678901234567890";
char menutext_RECALL[]      = "Program";
char menutext_OCTAVE[]      = "Octave";
char menutext_TRANSPOSE[]   = "Transpose";
char menutext_VELOCITY[]    = "Velocity";
char menutext_VOLUME[]      = "Volume";
char menutext_PRGCHNGE[]    = "Program Change";
char menutext_CHANNEL[]     = "MIDI Channel";
char menutext_SAVE[]        = "Save";

char displayText_PRESET[]   = "PRESET ";
char displayText_RECALLED[] = "recalled   ";
char displayText_NOTE[]     = "Note ";
char displayText_SAVED[]    = "saved      ";

struct lcdLocation
{
  int row;
  int line;
};

struct t_menuitemproperty
{
  char menutext[15];
  char abbreviation[1];
  lcdLocation dispLocation;
  int8_t  minValue;
  int8_t  maxValue;
};

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

  Serial.begin(9600);  //debugmonitor
  
  
  
  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print(boottext0);
  lcd.setCursor(0, 2);
  lcd.print(GIT_REV);
  delay(5000);
  lcd.clear();
  showMenu();
  Serial.println("Ready");
}



/// @brief Arduino Working LOOP
void loop() 
{
  readKeyboard();
  sendMIDI();
  MIDI.read();
  sethold();
  setoctave();
  setBankOfProgramToLoad();
  handleEnc0ButtonPressEvent();
  handleRotaryKnob();

  // // skip back to mainmenu after 30s
  // if (millis() == menutimeout + 30000)  // 30 Seconds
  // { 
  //   menulevel = eMenuRecall;
  //   bUserIsEditing = false;;digitalWrite(ledPresetGn, false);
  //   showMenu();
  //   menutimeout = millis();
  // }
}

//-----------------------------------------------
/// @brief print string of "width" characters, left-side filled with spaces if needed
/// if width is too small for the value, the value will increase the width as needed
/// @param value value to print (-128..+127)
/// @param width width of output without sign, if value is small, space if filled with white space 
/// @param sign  print sign before number, makes width 1 greater
void printValueAligned(int value, uint8_t width, bool sign) { 
// actual width is width + 1 char sign, if set

  uint8_t neededSpace = 0; 
  uint8_t neededDigits = 1;
  int neededFillerChars =0; 
  char signChar = '+';

  // limit width to 3 to avoid 
  // too large output
  if (width > 3)  width = 3;
  width += (sign ? 1 : 0); 
  
  // for negative numbers sign is always shown, regardless of input parameter
  if (value < 0) {
    signChar = '-';
    sign = true;
    value = -(value); // remove negative sign for printing purpose
  }

  // depending on digits print spaces then value
  if (value < 10)
    neededDigits = 1;
  else if (value < 100)
    neededDigits = 2;
  else 
    neededDigits = 3;

  // calcualte count of white space needed
  neededSpace = neededDigits + (sign ? 1 : 0); 
  neededFillerChars = width - neededSpace;
  while (neededFillerChars > 0){
    lcd.print(" ");
    neededFillerChars--;
  }
  if (sign)
    lcd.print(signChar);
  
  lcd.print(value);
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
void handleRotaryKnob() 
{
  bool userInputDetected = false;
  
  // scroll through menu level
  if (bUserIsEditing == false) {
    int newMenuLevel = static_cast<int>(menulevel); // cast to int;

    userInputPropertyValue(&newMenuLevel, ieMenuFirstItem, ieMenuLastItem, &userInputDetected);
    
    // need to update menu on-the-fly therefore 
    if (userInputDetected) // if any movement
    {
      menulevel = static_cast<e_menulevel>(newMenuLevel);
      showMenu();
    }
  }
  else // editing is active
  {
    // update value appording to menu 
    if (menulevel == eMenuRecall) {
      userInputPropertyValue(&iUserInputProgramNumber, 0, 38, &userInputDetected);
     
      if (userInputDetected) // update display
      {
          lcd.setCursor(1, 1);
          lcd.print(displayText_PRESET); 
          printPresetName(8, 1, iUserInputProgramNumber);
      }
    }
    if (menulevel == eMenuOctave) {
      userInputPropertyValue(&octave, 0, 10, &userInputDetected);
      
      if (userInputDetected) // update display
      {
          lcd.setCursor(1, 1);
          lcd.print("O");
          printValueAligned(octave - 1, 2, true);
      }
    }
    if (menulevel == eMenuTranspose) {
      userInputPropertyValue(&transpose, -12, +12, &userInputDetected);
      
      if (userInputDetected) // update display
      {
          lcd.setCursor(1, 1);
          lcd.print(displayText_NOTE);  // "Note "
          printValueAligned(transpose, 2, true);
      }
    }
    if (menulevel == eMenuVelocity) {
      userInputPropertyValue(&velocity, 0, 127, &userInputDetected);
      if (userInputDetected) // update display
      {
          lcd.setCursor(1, 1);
          printValueAligned(velocity, 3, false);
      }
    }
    if (menulevel == eMenuVolume) {
      userInputPropertyValue(&volume, 0, 127, &userInputDetected);
      if (userInputDetected) // update display
      {
          lcd.setCursor(1, 1);
          printValueAligned(volume, 3, false);
          MIDI.sendControlChange(7, volume, channel);
          // print bargraph
          lcd.setCursor(0, 3);
          lcd.print("V");
          barGraph(1, 3, 16, 127, volume);
          lcd.setCursor(17, 3);
          printValueAligned(volume, 3, false);
      }
    }
    if (menulevel == eMenuProgramChange) {
      userInputPropertyValue(&prgchange, -1, 127, &userInputDetected);
      if (userInputDetected) // update display
      {
          if (prgchange > -1) MIDI.sendProgramChange(prgchange, channel); // send on-the-fly program changes?
          lcd.setCursor(1, 1);
          if (prgchange > -1) 
            printValueAligned(prgchange, 3, false);
          else
           lcd.print("Off");
          
      }
    }
    if (menulevel == eMenuChannel) {
      userInputPropertyValue(&channel, 1, 16, &userInputDetected);
      if (userInputDetected) // update display
      {
          lcd.setCursor(1, 1);
          printValueAligned(channel, 2, false);
      }
    }
    if (menulevel == eMenuSave) {
      userInputPropertyValue(&saveP, 0, 38, &userInputDetected);
      if (userInputDetected) // update display
      {
          lcd.setCursor(1, 1);
          lcd.print(displayText_PRESET); 
          printPresetName(8, 1, saveP);
      }
    }
  }
}


/// @brief queries the user input device rotory encoder for input
/// if user turns knob, target content is counted up (if turning right)
/// or down (if turning left) to the limits given (including)
/// @details die Klammern (*target) sind wichtig! Sonst wird nicht der Inhalt von target 
/// verwendet, sondern ++ wird auf die Adresse angewendet und dann der inhalt 
/// genommen! 
/// @param target adress of target value to update
/// @param min minimum allowed value of target*
/// @param max maximum allowed value of target*
/// @param valueChanged value changed
void userInputPropertyValue(int *target, int min, int max, bool *valueChanged)
{
  char rotationDir;
  *valueChanged = false;
  rotationDir = my_encoder0.getKey();

  // Important: die Klammern (*target) sind wichtig! Sonst wird nicht der Inhalt von target 
  // verwendet, sondern ++ wird auf die Adresse angewendet und dann der inhalt 
  // genommen! 
  if (rotationDir == 'R') {
    if (*target < max) {
      (*target)++;      
      *valueChanged = true; // valueChanged only, if value is really changed!
    }
  }
  if (rotationDir == 'L') {
    if (*target > min) {
      (*target)--;
      *valueChanged = true;
    }
  }
  if (rotationDir) {
    menutimeout = millis();
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
      lcd.setCursor(0, 0); lcd.print(blankline);
      lcd.setCursor(0, 1); lcd.print(blankline);
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
      lcd.setCursor(17, 1);
      printValueAligned(transpose, 2, true);
      // text 2. line
      lcd.setCursor(1, 1);
      lcd.print(displayText_PRESET);
      if (bUserIsEditing == true) printPresetName(8, 1, iUserInputProgramNumber);
      else printPresetName(8, 1, iCurrProgram);
      // presetbank
      if (iActiveBankSelection > 0) {
        lcd.setCursor(18, 0);
        if (iActiveBankSelection == 1) lcd.print("A");
        if (iActiveBankSelection == 2) lcd.print("B");
        if (iActiveBankSelection == 3) lcd.print("C");
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
      printValueAligned(prgchange, 3, false);
      // bargraph modulation
      lcd.setCursor(0, 2);
      lcd.print("M");
      barGraph(1, 2, 16, 127, modulation);
      lcd.setCursor(17, 2);
      printValueAligned(modulation, 3, false);
      // bargraph volume
      lcd.setCursor(0, 3);
      lcd.print("V");
      barGraph(1, 3, 16, 127, volume);
      lcd.setCursor(17, 3);
      printValueAligned(volume, 3, false);
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
      printValueAligned(octave - 1, 2, false);
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
      printValueAligned(transpose, 2, true);
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
      printValueAligned(velocity, 3, false);
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
      printValueAligned(volume, 3, false);
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
      printValueAligned(prgchange, 3, false);
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
      printValueAligned(channel, 2, false);
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
      printPresetName(8, 1, iUserInputProgramNumber);  // Save Preset springt auf den Speicherplatz vom aktivem Recall Preset
      saveP = iUserInputProgramNumber;           // Recall Preset an Save Preset übergeben
      break;
  }
}

/// @brief Print preset name at given screen position
/// @param iPosX first char 
/// @param iPosY line 
/// @param iPreset index of preset, splitted in A0-A12 B0-12, C0-C12
void printPresetName(int iPosX, int iPosY, int iPreset) {  
  lcd.setCursor(iPosX, iPosY);
  if (iPreset >= 0 && iPreset < 13) {
    lcd.print("A");
    lcd.print(iPreset);
    if (iPreset <= 9) lcd.print(" ");
  }
  if (iPreset > 12 && iPreset < 26) {
    lcd.print("B");
    lcd.print(iPreset - 13);
    if (iPreset - 13 <= 9) lcd.print(" ");
  }
  if (iPreset > 25 && iPreset < 39) {
    lcd.print("C");
    lcd.print(iPreset - 26);
    if (iPreset - 26 <= 9) lcd.print(" ");
  }
}


/// @brief reads the keyboard input to variable keyispressed[]
/// from hardware signals using I2C. Signal 1 is RELEASED KEY, 0 is PRESSED key.
void readKeyboard() 
{  //Read the state of the I2C chips. 1 is open, 0 is closed.

  unsigned char data[2];  //data from chip 1/2

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
    keyispressed[i + 8] = ((data[1] >> i) & 1);  // chip 2
  }
}

// toggle bHoldModeIsActive on button_press_event
// use number "notehold" as offset to actual played note

/// @brief reads user input button "HOLD" and toggles
/// state of LED output and "bHoldModeIsActive" accrodingly
/// @details Hold Function used to keep note playing until another note
/// is played
void sethold() 
{
  holdButton.update();
  if (holdButton.read() == PRESSED) 
  {
    // run only once on button press event
    if (bPreviousState_BtnHold == RELEASED)
    {
      if (bHoldModeIsActive == false) // turn ON HoldMode
      {
        bHoldModeIsActive = true;
        notehold = 129;
        digitalWrite(ledHold, HIGH);
        showMenu();
      }
      else // turn OFF HoldMode
      {
        bHoldModeIsActive = false;
        MIDI.sendNoteOff(notehold - 1, 0, channel);
        anynoteisplaying = false;
        digitalWrite(ledHold, LOW);
        showMenu();
      }
      bPreviousState_BtnHold = PRESSED;
    }
  } 
  else 
  {
    bPreviousState_BtnHold = RELEASED;
  }
}

/// @brief reads user input button "OCT" and toggles
/// state of LED output and "bOctaveSelectModeIsActive" accrodingly
/// @details when bOctaveSelectModeIsActive is true, the next keypad
/// is choosing the octave from -1 to 10
void setoctave() {
  octaveButton.update();
  if (octaveButton.read() == PRESSED) 
  {
    // enter only on key press event
    if(bPreviousState_BtnOctave == RELEASED){
      if (bOctaveSelectModeIsActive == false) // turn ON Octave
      {
        abortActiveUserButtonInputs();
        bOctaveSelectModeIsActive = true; 
        digitalWrite(ledOct, HIGH);
      }
      else // turn OFF Octave
      {
        bOctaveSelectModeIsActive = false;
        digitalWrite(ledOct, LOW);
      }
      bPreviousState_BtnOctave = PRESSED;
    }
  } 
  else  // octaveButton Released
  {
    bPreviousState_BtnOctave = RELEASED;
  }
}


/// @brief 
void handleEnc0ButtonPressEvent() {

/// which step of the blocks were entered
/// used to block re-entering 
/// 1 = block 1 (print "loading"), 2 = block2 (actual loading / saving)
static uint8_t bBtnHoldState = 0; 

  encoder0Button.update(); 
    
  /// this code is called in a loop every xx ms. 
  /// so it shall detect a button press event for a longer duration
  /// stage 1  
  if (encoder0Button.read() == PRESSED && encoder0Button.duration() > 300 && bBtnHoldState < 1) 
  {
    if (menulevel == eMenuRecall || menulevel == eMenuSave) 
    {
      lcd.setCursor(0, 1);
      lcd.print("hold for ");
      if (menulevel == eMenuRecall) 
        lcd.print("load...");
      else
        lcd.print("save...");
      bBtnHoldState = 1; // stage 1 finished
    }
  }

  /// stage 2: User holds button pressed
  if (encoder0Button.read() == PRESSED && encoder0Button.duration() > 3000 && bBtnHoldState == 1) 
  {
    if (menulevel == eMenuRecall || menulevel == eMenuSave) 
    {
      bBtnHoldState = 2;
      lcd.setCursor(0, 0); lcd.print(displayText_PRESET); 
    }
    
    if (menulevel == eMenuRecall) 
    {
      iCurrProgram = iUserInputProgramNumber; // take user input as current Program
      printPresetName(7, 0, iCurrProgram);
      readEeprom();
      Panic();
      lcd.setCursor(0, 1);
      lcd.print(displayText_RECALLED);
    }
    if (menulevel == eMenuSave) 
    {
      saveEeprom();
      printPresetName(7, 0, saveP);  // use saveP as 
      lcd.setCursor(0, 1);
      lcd.print(displayText_SAVED);
      
    }
    if (menulevel == eMenuRecall || menulevel == eMenuSave) 
    {
      bUserIsEditing = false;digitalWrite(ledPresetGn, false);
      delay(2000);
      menulevel = eMenuRecall;  // reset menu to recall
      // iEnc0Value = menulevel; /// @todo: Makes sense?
      showMenu();
    }
  } 
  else // short button pressing 
  {
    // toggle edit mode
    if (digitalRead(in_ButtonEncoder) == PRESSED && bPreviousState_BtnEncoder == RELEASED)
    {
      Serial.println(__func__);
      bPreviousState_BtnEncoder = PRESSED;
      if (bUserIsEditing) 
      {
        bUserIsEditing = false; digitalWrite(ledPresetGn, false);/// TODO: DEBUG OUTPUT
        showMenu();
      }
      else // not in edit mode
      {
        bUserIsEditing = true; digitalWrite(ledPresetGn, true);
        printActiveLineMarker();
        abortActiveUserButtonInputs();
        // disable hold mode
        // but would need to stop playing notes also
        digitalWrite(ledHold, LOW);
        bHoldModeIsActive = 0;
        if (anynoteisplaying)
          Panic();
      }
      
    } 
  }
  
  // button released 
  if (encoder0Button.read() == RELEASED)
  {
    bPreviousState_BtnEncoder = RELEASED;
    if (bBtnHoldState > 0)
    {
      bBtnHoldState = 0;
      showMenu();
      Serial.println("enc buttin hold aborted");

    }
  }
}

/// @brief switches with every press of Bank Button 
/// from Bank A to B to C 
/// with is stored in iActiveBankSelection 1,2,3 and off is 0
/// next keypad input (1-13) loads the program A1..C13
void setBankOfProgramToLoad() {
  bankButton.update();
  if (bankButton.read() == PRESSED) 
  {
    if (bPreviousState_BtnBank == RELEASED)
    {
      // code enters on key press event
      bOctaveSelectModeIsActive = false; digitalWrite(ledOct, LOW); // abort an running OCT selection
      
      // toggle through bank OFF, A,B,C
      iActiveBankSelection++;
      if (iActiveBankSelection > 3)
        iActiveBankSelection = 0;
      
      setBankModeOutputs(); // Output current value to user
      bPreviousState_BtnBank = PRESSED;
    }
  } else {
    bPreviousState_BtnBank = RELEASED;
  }
}
//-------------------------------------
// Send MIDI instructions via MIDI out
// makes no sense to use 

/// @brief durcheinander aus verschiedenen funktionen. iActiveBankSelection und oct raus 
/// @note this function is not event-driven, therefore the noteisplaying[] array is 
/// used to detect, if a note changed shall be sent.
/// 
void sendMIDI() {  
  int note;            // absolute value of played note (0= C0, note 127 = G10)
  //for each key of the keyboad 
  for (unsigned int iKeyOfBoard = 0; iKeyOfBoard < noOfKeys; iKeyOfBoard++) {  
    if (keyispressed[iKeyOfBoard] == PRESSED) 
    {  
      // feature?: wenn Bankflag gesetzt, kann mithilfe des Keys ein Programm geladen werden
      // iActiveBankSelection sagt A-C, der Key das Programm
      if (iActiveBankSelection > 0) {
        if (iActiveBankSelection == 1) iCurrProgram = iKeyOfBoard;
        if (iActiveBankSelection == 2) iCurrProgram = iKeyOfBoard + 13;
        if (iActiveBankSelection == 3) iCurrProgram = iKeyOfBoard + 26;
        iUserInputProgramNumber = iCurrProgram;  // wird das Programm beim ersten Ton gesendet, falls ggw Prog. ungleich 
        readEeprom();
        showMenu();
        lcd.setCursor(18, 0);
        lcd.print("  ");
        digitalWrite(ledPresetGn, LOW);
        digitalWrite(ledPresetRt, LOW);
        bHoldModeIsActive = false;
        digitalWrite(ledHold, LOW);
        iActiveBankSelection = 0;
        Panic();
        delay(1000);
        break; // get out, no note is output
      }
      
      // feature? Der OctFlag wird gesetzt und der User sucht sich die Octave mit dem key aus
      // limit octave to 10
      if (bOctaveSelectModeIsActive == true && iKeyOfBoard <= 10) 
      {
        octave = iKeyOfBoard;
        showMenu();
        digitalWrite(ledOct, LOW);
        bOctaveSelectModeIsActive = false;
        delay(1000);
        break; // get out, no note is output
      } 
      else // standard path: output note to MIDI
      {
        if (!noteisplaying[iKeyOfBoard]) {  //if the note is not already playing send MIDI instruction to start the note
          note = iKeyOfBoard + (octave * 12) + transpose + 1; // why +1? 
          // distinguish hold function
          if (note < 129 && note > 0)  //1..128
          {
            if (bHoldModeIsActive == true) {  //if hold funcion active: Send "note off" before starting the new note
              MIDI.sendNoteOff(notehold - 1, 0, channel);
              keytime = millis();
              Serial.print("NoteOff ");
              //Serial.println(notehold-1);
            }
            MIDI.sendNoteOn(note - 1, velocity, channel);  // Send a Note
            noteisplaying[iKeyOfBoard] = note;             // set the note playing flag to TRUE and store the note value
            notehold = note;                               //buffer the old holded note
            keytime = millis();
            anynoteisplaying = true;
            lcd.setCursor(19, 0);
            lcd.print((char)8);
            Serial.print("NoteOn ");
            //Serial.print(channel);
            //Serial.println(note-1);
          }
        }
      }
    } 
    else  // key is NOT pressed 
    { 
      if (noteisplaying[iKeyOfBoard] && millis() > keytime + 100) {  //if the note is currently playing, turn it off
        note = noteisplaying[iKeyOfBoard];                           //retrieve the saved note value incase the octave has changed
        
        // Stop the note if hold is NOT active. 
        // if hold flag is active the note remains on 
        // until a new note is started or hold is deactivated
        if (bHoldModeIsActive == false) {                           
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
  Serial.println(__func__);
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



/// @brief Abort all user inputs controlled by buttons, HOLD, OCT, BANK
/// and reset LEDs and flags, avoid curcurrent inputs
void abortActiveUserButtonInputs()
{
  Serial.println(__func__);
  // Bank
  digitalWrite(ledPresetGn, LOW);
  digitalWrite(ledPresetRt, LOW);
  iActiveBankSelection = 0;
  // Octave
  bOctaveSelectModeIsActive = false; digitalWrite(ledOct, LOW);
  // Hold may remain active?
  // if stopped, active notes would have to be stopped
  // digitalWrite(ledHold, LOW);
  // bHoldModeIsActive = false;
}

/// @brief show user the state of iActiveBankSelection on LED and display
void setBankModeOutputs()
{
  lcd.setCursor(18, 0); // Prepare output of Bank Name
  switch (iActiveBankSelection)
  {
    case 1:
      digitalWrite(ledPresetGn, HIGH);
      digitalWrite(ledPresetRt, LOW);
      lcd.print("A");
      break;
    case 2:
      digitalWrite(ledPresetGn, LOW);
      digitalWrite(ledPresetRt, HIGH);
      lcd.print("B");
      break;
   case 3:
      digitalWrite(ledPresetGn, HIGH);
      digitalWrite(ledPresetRt, HIGH);
      lcd.print("C");
      break; 
    default:
      digitalWrite(ledPresetGn, LOW);
      digitalWrite(ledPresetRt, LOW);
      lcd.print(" ");
  }
}