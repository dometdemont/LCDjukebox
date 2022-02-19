/*
  Select a MIDI file on an SD card and play it
  An LCD and 3 buttons enable the navigation and play selection
*/

#include <SdFat.h>
#include <MD_MIDIFile.h>
#include <LiquidCrystal.h>

// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(10, 9, 5, 4, 3, 2);
SdFat  SD;
MD_MIDIFile SMF;
#define SERIAL_RATE 31250

class button {
  public: 
    button(int pin);
    enum buttonState {ON, OFF, UNCHANGED, HOLD};
    buttonState getState();
  private:
    const int HOLDTIME=2000;  // a min long press duration
    const int PUSHTIME=200;   // a min normal press duration
    int pin;
    buttonState state;
    buttonState returnOff;    // the buttonState to return when the button will be depressed: ON for a normal press, OFF for a long one
    uint32_t timestamp;       // date of the last reported state
};

button::button(int aPin){
  pin=aPin;
  pinMode(pin, INPUT);
  state=digitalRead(pin)?ON:OFF;
  timestamp=millis();
  returnOff=ON;
}
button::buttonState button::getState(){
  buttonState result=UNCHANGED;
  buttonState current=digitalRead(pin)?ON:OFF;
  if(current != state){
    if(state == ON && (millis()-timestamp>PUSHTIME) && (millis()-timestamp<=HOLDTIME)){
      result=returnOff;
      returnOff=ON;
    }
    state=current;
    timestamp=millis();
  }else{
    if(state == ON && (millis()-timestamp>HOLDTIME)){
      result=HOLD;
      returnOff=OFF;
      timestamp=millis(); // Reset the last event timestamp to report only changes
    }
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
  const char msg[]={
    120,  // All sounds off
    121,  // Reset all controllers
    123   // All notes off
  };
  
  ev.size = 0;
  ev.data[ev.size++] = 0xb0;
  ev.data[ev.size++] = 0;
  ev.data[ev.size++] = 0;

  for(int i=sizeof(msg)/sizeof(msg[0]); i--; ){
    ev.data[1]=msg[i];
    for (ev.channel = 0; ev.channel < 16; ev.channel++)
    midiCallback(&ev);
  }
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

// Current display when browsing the catalog
int iPlay;
// Actual play
play* playing;

play::play(const char* aFile, const char* aTitle, const char* anAuthor){
  file = aFile;
  title = aTitle;
  author = anAuthor;
}
void play::start(){
  if(playing != NULL){
    lcd.setCursor(0, 0); lcd.print(F("Hold to stop   :"));
    lcd.setCursor(0, 1); lcd.print(playing->title); 
    return;    
  }
  int err = SMF.load(this->file);
  if (err != MD_MIDIFile::E_OK){
    lcd.setCursor(0, 0); lcd.print(F("SD load error:  "));
    lcd.setCursor(0, 1); lcd.print(err); lcd.print(F("                ")); 
  }else{
    lcd.setCursor(0, 0); lcd.print(F("Now playing:    "));
    lcd.setCursor(0, 1); lcd.print(this->title);
    playing=this;
  }
}
void play::cancel(){
  if(playing == NULL)return;
  lcd.setCursor(0, 0); lcd.print(F("Now stopping:   "));
  lcd.setCursor(0, 1); lcd.print(this->title);
  SMF.close();
}
void play::show(){
  lcd.setCursor(0, 0); lcd.print(this->title);
  lcd.setCursor(0, 1); lcd.print(this->author);
}

play liszt("LISZT.MID",       "P&F sur B.A.C.H.",  "Franz Liszt     ");
play messiaen("MESSIAEN.MID", "Banquet ce'leste",  "Olivier Messiaen");
play buxtehude("BUXTEHUD.MID","Passacaille     ",  "Dietr. Buxtehude");
play wagner("WAGNER.MID",     "Mort d'Isolde   ",  "Richard Wagner  ");
play dupre("DUPRE.MID",       "P&F en sol min  ",  "Marcel Dupre'   ");
play taille("COUPERIN.MID",   "Tierce en taille",  "Fr. Couperin    ");
play paix("LANGLAIS.MID",     "Chant de paix   ",  "Jean Langlais   ");
play franck("FRANCK.MID",     "3eme Choral     ",  "Cesar Franck    ");
play boelmann("BOELMANN.MID", "Toccata         ",  "Le'on Boelmann  ");
play chromorn("CHROMORN.MID", "Chromorne taille",  "Fr. Couperin    ");
play *playList[] = {&liszt, &franck, &taille, &wagner, &buxtehude, &chromorn};
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
// really be processed, so we just pass it through
// This callback is set up in the setup() function.
{
  Serial.write(pev->data, pev->size);
}

class swellBox {
  public:
    swellBox(int channel);
    void open();
    void close();
    void reverse();
    bool isOpened();
  private:
    int midiChannel;
    bool opened;
};
swellBox::swellBox(int channel){
  midiChannel=channel;
  open();  
};
bool swellBox::isOpened(){return opened;}
void swellBox::open(){
  midi_event ev;
  ev.size = 0;
  ev.channel = midiChannel;
  ev.data[ev.size++] = 0xb0;
  ev.data[ev.size++] = 0x0b;
  ev.data[ev.size++] = 100;
  midiCallback(&ev);  
  opened=true;
};
void swellBox::close(){
  midi_event ev;
  ev.size = 0;
  ev.channel = midiChannel;
  ev.data[ev.size++] = 0xb0;
  ev.data[ev.size++] = 0x0b;
  ev.data[ev.size++] = 0;
  midiCallback(&ev);  
  opened=false;
};
swellBox recit(2);
swellBox positif(3);


void welcome(){
  lcd.setCursor(0, 0); lcd.print(F("<<=    |>    =>>"));
  lcd.setCursor(0, 1); lcd.print(F("_POSf_ XX _RECt_"));
  if(positif.isOpened()){lcd.setCursor(0, 1);lcd.print(F("^POSf^"));}
  if(recit.isOpened()){lcd.setCursor(10, 1);lcd.print(F("^RECt^"));}
}

void swellBox::reverse(){
  if(opened)close();
  else open();
  welcome();
};

void setup() {
  Serial.begin(SERIAL_RATE);

  // set up the number of columns and rows on the LCD
  lcd.begin(16, 2);

  while(!SD.begin(chipSelect, SPI_FULL_SPEED)) {
    lcd.setCursor(0, 0);
    lcd.print(F("SD init failed  "));
    delay(5000);
  };

  recit.open(); positif.open();
  
  welcome();
  
  // Initialize MIDIFile
  SMF.begin(&SD);
  SMF.setMidiHandler(midiCallback);
  SMF.setSysexHandler(sysexCallback);
  
  iPlay=0;
  playing=NULL;
}

void loop() {
  switch(left.getState())   {
    case button::buttonState::ON: iPlay=(iPlay+iMaxPlay-1)%iMaxPlay; playList[iPlay]->show(); break; 
    case button::buttonState::HOLD:positif.reverse();
    }
  switch(middle.getState()) {
    case button::buttonState::ON: playList[iPlay]->start(); break; 
    case button::buttonState::HOLD: playList[iPlay]->cancel(); break;
    }
  switch(right.getState())  {
    case button::buttonState::ON: iPlay=(iPlay+1)%iMaxPlay; playList[iPlay]->show(); break; 
    case button::buttonState::HOLD:recit.reverse();
    }

  if(playing != NULL){
    if (SMF.isEOF()){
      SMF.close();
      midiSilence();
      positif.open(); recit.open();
      welcome();
      playing=NULL;
    }else{
      SMF.getNextEvent();
    }
  }
}
