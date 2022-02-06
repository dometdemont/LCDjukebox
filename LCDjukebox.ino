/*
  Select a MIDI file on an SD card and play it
*/

#include <SdFat.h>
#include <MD_MIDIFile.h>
#include <LiquidCrystal.h>

// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(10, 9, 5, 4, 3, 2);
SdFat  SD;
MD_MIDIFile SMF;
#define SERIAL_RATE 31250

enum buttonState {ON, OFF, UNCHANGED, HOLD};
const int HOLDTIME=2000;

class button {
  public: 
    button(int pin);
    buttonState getState();
  private:
    int pin;
    int state;    
    uint32_t timestamp;
};

button::button(int aPin){
  pin=aPin;
  pinMode(pin, INPUT);
  state=digitalRead(pin);
  timestamp=millis();
}
buttonState button::getState(){
  buttonState result=UNCHANGED;
  int current=digitalRead(pin);
  if(current != state){
    if(state && millis()-timestamp<=HOLDTIME)result=ON;
    state=current;
    timestamp=millis();
  }else{
    if(state && millis()-timestamp>HOLDTIME)result=HOLD;
  }
  return result;
}

// Three buttons inputs
button left(6);
button middle(7);
button right(8);

void midiSilence(void)
// Turn everything off on every channel.
// Some midi files are badly behaved and leave notes hanging, so between songs turn
// off all the notes and sound
{
  midi_event ev;

  // All sound off
  // When All Sound Off is received all oscillators will turn off, and their volume
  // envelopes are set to zero as soon as possible.
  ev.size = 0;
  ev.data[ev.size++] = 0xb0;
  ev.data[ev.size++] = 120;
  ev.data[ev.size++] = 0;

  for (ev.channel = 0; ev.channel < 16; ev.channel++)
    midiCallback(&ev);
}

class play {
  public:
    play(const char* aFile, const char* aTitle, const char* anAuthor);
    void start();
    void cancel();
    void show();
  private:
    const char* file;
    const char* title;
    const char* author;
};

int iPlay;
bool playing;

play::play(const char* aFile, const char* aTitle, const char* anAuthor){
  file = aFile;
  title = aTitle;
  author = anAuthor;
}
void play::start(){
  int err = SMF.load(this->file);
  if (err != MD_MIDIFile::E_OK){
    lcd.setCursor(0, 0); lcd.print(F("SD load error:  "));
    lcd.setCursor(0, 1); lcd.print(err); lcd.print(F("                ")); 
  }else{
    lcd.setCursor(0, 0); lcd.print(F("Now playing:    "));
    lcd.setCursor(0, 1); lcd.print(this->title);
    playing=true;
  }
}
void play::cancel(){
  if(!playing)return;
  lcd.setCursor(0, 0); lcd.print(F("Now stopping:   "));
  lcd.setCursor(0, 1); lcd.print(this->title);
  SMF.close();
}
void play::show(){
  lcd.setCursor(0, 0); lcd.print(this->title);
  lcd.setCursor(0, 1); lcd.print(this->author);
}

void welcome(){
  lcd.setCursor(0, 0); lcd.print(F("  Select & Play "));
  lcd.setCursor(0, 1); lcd.print(F("<<=    |>    =>>"));
}


play elise("ELISE.MID",     "Lettre a` Elise ",  "Ludwig Beethoven");
play menuet("MINUET.MID",   "Menuet          ",  "W Amadeus Mozart");
play nacht("MOZART.MID",    "Symphonie 40    ",  "W Amadeus Mozart");
play fugue("FUGUEGM.MID",   "Fugue en Sol M. ",  "Johann Seb. Bach");
play fernand("FERNANDO.MID","Fernande        ",  "Anonyme         ");
play taille("COUPERIN.MID", "Tierce en taille",  "Fr. Couperin    ");
play paix("LANGLAIS.MID",   "Chant de paix   ",  "Jean Langlais   ");
//play franck("FRANCK.MID",   "3eme Choral     ",   "Cesar Franck   ");

play *playList[] = {&elise, &nacht, &taille, &paix, &menuet, &fugue, &fernand};
const int iMaxPlay=sizeof(playList)/sizeof(playList[0]);

// change this to match your SD shield or module;
// Arduino Ethernet shield: pin 4
// Adafruit SD shields and modules: pin 10
// Sparkfun SD shield: pin 8
// MKRZero SD: SDCARD_SS_PIN
const int chipSelect = 10;

void midiCallback(midi_event *pev)
// Called by the MIDIFile library when a file event needs to be processed
// thru the midi communications interface.
// This callback is set up in the setup() function.
{
  if ((pev->data[0] >= 0x80) && (pev->data[0] <= 0xe0))
  {
    Serial.write(pev->data[0] | pev->channel);
    Serial.write(&pev->data[1], pev->size-1);
  }
  else
    Serial.write(pev->data, pev->size);
}

void sysexCallback(sysex_event *pev)
// Called by the MIDIFile library when a system Exclusive (sysex) file event needs 
// to be processed through the midi communications interface. Most sysex events cannot 
// really be processed, so we just ignore it here.
// This callback is set up in the setup() function.
{
  char msg[20];
  sprintf(msg, "SysEx on %d: %d", pev->track, pev->size);
  lcd.setCursor(0, 0); lcd.print(msg);
  for (uint8_t i=0; i<pev->size; i++)
  {
  }
}

void setup() {
  Serial.begin(SERIAL_RATE);

  // set up the number of columns and rows on the LCD
  lcd.begin(16, 2);

  while(!SD.begin(chipSelect, SPI_FULL_SPEED)) {
    lcd.setCursor(0, 0);
    lcd.print(F("SD init failed  "));
    delay(5000);
  };
  welcome();
  
  // Initialize MIDIFile
  SMF.begin(&SD);
  SMF.setMidiHandler(midiCallback);
  SMF.setSysexHandler(sysexCallback);
  
  iPlay=0;
  playing=false;
}



void loop() {
  switch(left.getState())   {case ON: iPlay=(iPlay+iMaxPlay-1)%iMaxPlay; playList[iPlay]->show(); break;}
  switch(middle.getState()) {case ON: playList[iPlay]->start(); break; case HOLD: playList[iPlay]->cancel(); break;}
  switch(right.getState())  {case ON: iPlay=(iPlay+1)%iMaxPlay; playList[iPlay]->show(); break;}

  if(playing){
    if (SMF.isEOF()){
      SMF.close();
      midiSilence();
      welcome();
      playing=false;
    }else{
      SMF.getNextEvent();
    }
  }
}
