// Tempo change when stopped deletes 'BPM'


#include <SendOnlySoftwareSerial.h>
#include <SoftwareSerial.h>

#include <light_CD74HC4067.h>
#include "TapTempo.h" // Note the quotation marks. Because it's in the same folder as the code itself. Get it, e.g. from https://github.com/Andymann/ArduinoTapTempo
#include <uClock.h>
#include <Button2.h>
#include <Adafruit_NeoPixel.h>
#include "MIDIUSB.h"
#include <EEPROM.h>

#include <Wire.h>
#include "SSD1306Ascii.h"
#include "SSD1306AsciiWire.h"

// 0X3C+SA0 - 0x3C or 0x3D
#define DISPLAY_I2C_ADDRESS 0x3C
SSD1306AsciiWire oled;

#define VERSION "3.02"
#define DEMUX_PIN A0

//const int signal_pin = A0; // Pin Connected to Sig pin of CD74HC4067
CD74HC4067 mux(6,7,8,9);  // create a new CD74HC4067 object with its four select lines

#define NUM_LEDS 4
#define DATA_PIN 10
#define LED_ON 10 //60
#define LED_OFF 0
Adafruit_NeoPixel pixels(NUM_LEDS, DATA_PIN, NEO_GRB + NEO_KHZ800);

#define MIDI_CLOCK 0xF8
#define MIDI_START 0xFA
#define MIDI_STOP  0xFC


#define QUANTIZERESTARTOFFSET 94 // Damit ein restart echt mega genau ankommt; test per Ableton Metronom
#define LONGCLICKTIMEMS 2000

#define NEXTPRESET_NONE 0
#define NEXTPRESET_1 1
#define NEXTPRESET_2 2
#define NEXTPRESET_3 3
uint8_t iNextPreset;

TapTempo tapTempo;
Button2 btnHelper_TAP;
Button2 btnHelper_START;
Button2 btnHelper_STOP;
Button2 btnHelper_PRESET1;
Button2 btnHelper_PRESET2;
Button2 btnHelper_PRESET3;

//SoftwareSerial(rxPin, txPin, inverse_logic)
SoftwareSerial midi(A2, A3);
//SendOnlySoftwareSerial midi(A3);

uint8_t bpm_blink_timer = 1;
float fBPM_Cache = 94.0;
float fBPM_Sysex;
//float fTempBPM = fBPM_Cache;
bool bQuantizeRestartWaiting = false;
uint8_t iMeasureCount = 0;

float fBPM_Preset1 = 80.0;
float fBPM_Preset2 = 100.0;
float fBPM_Preset3 = 120.0;

bool bIsPlaying = false;
bool bUpdateStatusDisplay = false;
bool bDisplayInverted = false;
bool bNewBPM = false;
bool bNudgeActive = false;

String sActivePreset = "AAAAAAAAAAAAAA";
bool bNewPresetSelected = false;

#define CLOCKMODE_MASTER 1
#define CLOCKMODE_MIXXX 2 
uint8_t iClockMode = CLOCKMODE_MASTER;

byte muxValue[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}; // The value of the Buttons as read from the multiplexer

#define BUFFERSIZE 60
byte sysexBuffer[BUFFERSIZE];

//----Display
//----SCL - 22
//----SDA - 21
//#define OLED_RESET -1 // not used / nicht genutzt bei diesem Display
//Adafruit_SSD1306 display(128, 64, &Wire, OLED_RESET);


void setup()
{
  iNextPreset = NEXTPRESET_NONE;
  Serial.begin(9600);
  //delay(251);
  pixels.begin();
  ledInit();
  midi.begin(31250);
  pinMode(DEMUX_PIN, INPUT); // Set as input for reading through signal pin

  tapTempo.setTotalTapValues(4);
  //tapTempo.setBPM(fBPM_Cache);

  // Inits the clock
  uClock.init();
  // Set the callback function for the clock output to send MIDI Sync message.
  uClock.setClock96PPQNOutput(ClockOut96PPQN);
  // Set the callback function for MIDI Start and Stop messages.
  uClock.setOnClockStartOutput(onClockStart);  
  uClock.setOnClockStopOutput(onClockStop);
  // Set the clock BPM to 126 BPM
  //uClock.setTempo( tapTempo.getBPM() );
  setGlobalBPM( fBPM_Cache );
  // Starts the clock, tick-tac-tick-tac...
  //uClock.start();
  
  Serial.print("Clock Ready ... "); Serial.println( tapTempo.getBPM() );

  //btnHelper_TAP.begin(TAP_BUTTON, INPUT_PULLDOWN, true);
  btnHelper_TAP.begin(VIRTUAL_PIN);
  btnHelper_TAP.setButtonStateFunction(tapButtonStateHandler);
  btnHelper_TAP.setTapHandler(tapHandler);

  btnHelper_START.begin(VIRTUAL_PIN);
  btnHelper_START.setButtonStateFunction(startButtonStateHandler);
  btnHelper_START.setTapHandler(startHandler);

  btnHelper_STOP.begin(VIRTUAL_PIN);
  btnHelper_STOP.setButtonStateFunction(stopButtonStateHandler);
  btnHelper_STOP.setTapHandler(stopHandler);

  btnHelper_PRESET1.begin(VIRTUAL_PIN,INPUT, false);
  btnHelper_PRESET1.setButtonStateFunction(preset1ButtonStateHandler);
  btnHelper_PRESET1.setLongClickTime( LONGCLICKTIMEMS );
  btnHelper_PRESET1.setClickHandler(preset1ClickHandler);
  btnHelper_PRESET1.setLongClickDetectedHandler(preset1LongClickDetected);
  btnHelper_PRESET1.setChangedHandler(preset1ChangeHandler);  // Falls externer sync via sysex

  btnHelper_PRESET2.begin(VIRTUAL_PIN, INPUT, false);
  btnHelper_PRESET2.setButtonStateFunction(preset2ButtonStateHandler);
  btnHelper_PRESET2.setLongClickTime( LONGCLICKTIMEMS );
  btnHelper_PRESET2.setClickHandler(preset2ClickHandler);
  btnHelper_PRESET2.setLongClickDetectedHandler(preset2LongClickDetected);
  btnHelper_PRESET2.setTapHandler(restartHandler);

  btnHelper_PRESET3.begin(VIRTUAL_PIN, INPUT, false);
  btnHelper_PRESET3.setButtonStateFunction(preset3ButtonStateHandler);
  btnHelper_PRESET3.setLongClickTime( LONGCLICKTIMEMS );
  btnHelper_PRESET3.setClickHandler(preset3ClickHandler);
  btnHelper_PRESET3.setLongClickDetectedHandler(preset3LongClickDetected);
  btnHelper_PRESET3.setChangedHandler(preset3ChangeHandler);  // Falls externer sync via sysex

  Wire.begin();
  Wire.setClock(400000L);
  oled.begin(&Adafruit128x64, DISPLAY_I2C_ADDRESS);
  
 
  getPresetsFromEeprom();
  //delay(1111);
  showInfo(1500);
  ledOff();
}

void loop()
{
  readMux();
  btnHelper_TAP.loop();
  btnHelper_START.loop();
  btnHelper_STOP.loop();
  btnHelper_PRESET1.loop();
  btnHelper_PRESET2.loop();
  btnHelper_PRESET3.loop();

  tapTempo.update(false);

  // ---- Nur ganzzahlige Werte darstellen, Rundungsfehler ueberdecken
  if(iClockMode==CLOCKMODE_MASTER){
    if( abs(tapTempo.getBPM() - fBPM_Cache) >= 1.0){
      bNewBPM = true;
      fBPM_Cache = uint8_t(tapTempo.getBPM());
      uClock.setTempo( fBPM_Cache );
      //tapTempo.setBPM( fBPM_Cache );
      if(bIsPlaying){
        showBPM( fBPM_Cache );
      }else{
        showStatus( 0, false );
      }
    }
  }else if(iClockMode==CLOCKMODE_MIXXX){
    //Serial.print("tapTempo.getBPM()"); Serial.println(tapTempo.getBPM());
    //Serial.print("fBPM_Sysex"); Serial.println( fBPM_Sysex );
    //Serial.print("clock.getTempo()"); Serial.println( uClock.getTempo() );
    if( abs(uClock.getTempo() - fBPM_Sysex) >= .1){
      if( bNudgeActive==false ){
        fBPM_Cache = fBPM_Sysex;
        setGlobalBPM(fBPM_Sysex);
        if(bIsPlaying){
          showBPM( fBPM_Sysex );
        }else{
          showStatus( 0, false );
        }
      }else{
        if(bIsPlaying){
          showBPM( fBPM_Cache );
        }else{
          showStatus( 0, false );
        }
      }
    }
  }
  

  int iEncoder = queryEncoder();
  if(iEncoder!=0){
    if(iClockMode==CLOCKMODE_MASTER){
      if(muxValue[13]==0){
        //setGlobalBPM( /*tapTempo.getBPM()*/ fBPM_Cache + (iEncoder*1.0) );
        fBPM_Cache += iEncoder;
      }else{
        //setGlobalBPM( /*tapTempo.getBPM()*/ fBPM_Cache + (iEncoder*5.0) );
        fBPM_Cache += iEncoder*5.0;
      }
      bNewBPM = true;
      setGlobalBPM( fBPM_Cache);
    }else if(iClockMode==CLOCKMODE_MIXXX){
      if(muxValue[13]==0){
        //setGlobalBPM( /*tapTempo.getBPM()*/ fBPM_Cache + (iEncoder*1.0) );
        fBPM_Sysex += iEncoder*0.01;
        fBPM_Cache = fBPM_Sysex;
      }else{
        //setGlobalBPM( /*tapTempo.getBPM()*/ fBPM_Cache + (iEncoder*5.0) );
        fBPM_Sysex += iEncoder*1.0;
        fBPM_Cache = fBPM_Sysex;
      }
      bNewBPM = true;
      setGlobalBPM( fBPM_Cache);
    }
    
  }

  if(iNextPreset != NEXTPRESET_NONE){
    selectPreset(iNextPreset);
    iNextPreset = NEXTPRESET_NONE;
  }

  if(bUpdateStatusDisplay==true){
    showBPM(fBPM_Cache);
    oled.setInvertMode( bDisplayInverted );
    oled.setFont(ZevvPeep8x16);
    oled.set1X();
    oled.setRow(6);
    if( iClockMode == CLOCKMODE_MASTER){
      oled.setCol(105);
      oled.print("BPM");
    }else if( iClockMode == CLOCKMODE_MIXXX ){
      oled.setCol(88);
      oled.print("SYSEX");
    }
    bUpdateStatusDisplay=false;
  }

    if(bNewPresetSelected == true){
      oled.setInvertMode( false );
      oled.setFont(ZevvPeep8x16);
      oled.set1X();
      oled.setRow(6);
      oled.setCol(5);
      oled.print(sActivePreset);
      bNewPresetSelected = false;
    }

  midiEventPacket_t rx = MidiUSB.read();
  if (rx.header != 0) {
    processMidiIn( rx );
  }
}//

void processMidiIn(midiEventPacket_t pRX){
  switch (pRX.header & 0x0F) {
    case 0x00:  // Misc. Reserved for future extensions.
      break;
    case 0x01:  // Cable events. Reserved for future expansion.
      break;
    case 0x02:  // Two-byte System Common messages
      break;
    case 0x0C:  // Program Change
      break;
    case 0x0D:  // Channel Pressure
      // do something with 2 byte messages
      break;
    case 0x03:  // Three-byte System Common messages
      break;
    case 0x08:  // Note-off
      break;
    case 0x09:  // Note-on
      break;
    case 0x0A:  // Poly-KeyPress
      break;
    case 0x0B:  // Control Change
      break;
    case 0x0E:  // PitchBend Change
      // do something with 3 byte messages
      break;
    case 0x04:  // SysEx starts or continues
      // append sysex buffer with 3 bytes
      iClockMode = CLOCKMODE_MIXXX;
      fillSysexBuffer( pRX, 3 );
      break;
    case 0x05:  // Single-byte System Common Message or SysEx ends with the following single byte
      // append sysex buffer with 1 byte
      // process completed sysex buffer
      // init sysex buffer empty
      fillSysexBuffer( pRX, 1 );
      Serial.println("header 0x05");
      break;
    case 0x06:  // SysEx ends with the following two bytes
      // append sysex buffer with 2 bytes
      // process completed sysex buffer
      // init sysex buffer empty
      fillSysexBuffer( pRX, 2 );
      processSysexBuffer();
      //Serial.println("header 0x06");
      break;
    case 0x07:  // SysEx ends with the following three bytes
      // append sysex buffer with 3 bytes
      // process completed sysex buffer
      // init sysex buffer empty
      fillSysexBuffer( pRX, 3 );
      Serial.println("header 0x07");
      break;
    case 0x0F:  // Single Byte, TuneRequest, Clock, Start, Continue, Stop, etc.
      // process since byte messages
      break;
    
  }
}

void selectPreset(uint8_t pPresetID){
  if(pPresetID == NEXTPRESET_1){
    fBPM_Cache = uint8_t(fBPM_Preset1);
    setGlobalBPM( fBPM_Preset1 );
    bNewBPM = true;
    displayPresetName("Preset 1");
  }else if(pPresetID == NEXTPRESET_2){
    fBPM_Cache = uint8_t(fBPM_Preset2);
    setGlobalBPM( fBPM_Preset2 );
    bNewBPM = true;
    displayPresetName("Preset 2");
  }else if(pPresetID == NEXTPRESET_3){
    fBPM_Cache = uint8_t(fBPM_Preset3);
    setGlobalBPM( fBPM_Preset3 );
    bNewBPM = true;
    displayPresetName("Preset 3");
  }
  showBPM( fBPM_Cache );
  if(!bIsPlaying){
    startPlaying();
  }
}


void displayPresetName(String p){
  sActivePreset = p;
  bNewPresetSelected = true;
}

void readMux(){
  // loop through channels 0 - 15
    for (byte i = 0; i < 16; i++) {
        mux.channel(i);
        int val = digitalRead(DEMUX_PIN);                       // Read analog value
        muxValue[i] = val;
    }
}

// The callback function wich will be called by Clock each Pulse of 96PPQN clock resolution.
void ClockOut96PPQN(uint32_t tick) {
    // Send MIDI_CLOCK to external gears
    sendMidiClock();
    handle_bpm_led(tick);
}


// The callback function wich will be called when clock starts by using Clock.start() method.
void onClockStart() {
  sendMidiStart();
}

// The callback function wich will be called when clock stops by using Clock.stop() method.
void onClockStop() {
  sendMidiStop();
  ledOff();
}

void sendMidiClock(){
  midiEventPacket_t p = {0x0F, MIDI_CLOCK, 0, 0};
  MidiUSB.sendMIDI(p);
  MidiUSB.flush();
  midi.write(MIDI_CLOCK);
}

void sendMidiStart(){
  midiEventPacket_t p = {0x0F, MIDI_START, 0, 0};
  MidiUSB.sendMIDI(p);
  MidiUSB.flush();
  midi.write(MIDI_START);
}

void sendMidiStop(){
  midiEventPacket_t p = {0x0F, MIDI_STOP, 0, 0};
  MidiUSB.sendMIDI(p);
  MidiUSB.flush();
  midi.write(MIDI_STOP);
}

void handle_bpm_led(uint32_t tick)
{
  // BPM led indicator
  if( (tick % (96) == (QUANTIZERESTARTOFFSET) )){
    if( bQuantizeRestartWaiting == true){
      bQuantizeRestartWaiting = false;
      sendMidiStart();
    }
  }
  if ( !(tick % (96)) || (tick == 1) ) {  // first compass step will flash longer
    bpm_blink_timer = 12;
    digitalWrite(LED_BUILTIN, HIGH);
    iMeasureCount = 0;
    //ledIndicateMeasure( iMeasureCount );
    ledIndicateStart();
    //showStatus(iMeasureCount+1, true);
    showStatus(0, true);


  } else if ( !(tick % (24)) ) {   // each quarter led on
    bpm_blink_timer = 12;
    iMeasureCount++;
    digitalWrite(LED_BUILTIN, HIGH);
    ledIndicateMeasure( iMeasureCount );
    //showStatus(iMeasureCount+1, true);
    showStatus(0, true);
    
  } else if ( !(tick % bpm_blink_timer) ) { // get led off
    digitalWrite(LED_BUILTIN, LOW);
    if(!bQuantizeRestartWaiting){
      ledOff();
    }
    //showStatus(iMeasureCount+1, false);
    showStatus(0, false);
  }
}

void tapHandler(Button2& btn) {
    //Serial.println("tapHandler");
    tapTempo.update(true);
}

byte tapButtonStateHandler() {
  return muxValue[0] == 0 ? false:true;
}

void startHandler(Button2& btn) {
  startPlaying();
}

void restartHandler(Button2& btn){
  if(iClockMode==CLOCKMODE_MIXXX){
    //stopPlaying();
    //startPlaying();
    bNewBPM = true;
    showBPM( fBPM_Cache );
    uClock.start();
  }
  
}

void startPlaying(){
  //Serial.println("startHandler");
    if(!bIsPlaying){
      bNewBPM = true;
      showBPM( fBPM_Cache );
      uClock.start();
    }else{
      bQuantizeRestartWaiting = true;
    }
    bIsPlaying = true;
    showStatus(iMeasureCount+1, false);
}

byte startButtonStateHandler() {
  return muxValue[1] == 0 ? false:true;
}

void stopHandler(Button2& btn) {
    stopPlaying();
}

void stopPlaying(){
  bIsPlaying = false;
  bQuantizeRestartWaiting = false;
  uClock.stop();
  showStatus(iMeasureCount+1, false);
}

byte stopButtonStateHandler() {
  return muxValue[2] == 0 ? false:true;
}

byte preset1ButtonStateHandler() {
  return muxValue[3] == 0 ? false : true;
}

void preset1ClickHandler(Button2& btn) {
  if(iClockMode == CLOCKMODE_MASTER){
    iNextPreset = NEXTPRESET_1;
  }
}

void preset1LongClickDetected(Button2& btn) {
    if(iClockMode == CLOCKMODE_MASTER){
      fBPM_Preset1 = tapTempo.getBPM();
      EEPROM.update(10, byte(fBPM_Preset1));
      ledRed();
    }
}

void preset1ChangeHandler( Button2& btn ){
  if(iClockMode == CLOCKMODE_MIXXX){
    //Serial.println("Preset1 Button changed " + String(btn.isPressed() ) );
    nudgeMinus( btn.isPressed() );
  }
}

void preset3ChangeHandler( Button2& btn ){
  if(iClockMode == CLOCKMODE_MIXXX){
    //Serial.println("Preset2 Button changed " + String(btn.isPressed() ) );
    nudgePlus( btn.isPressed() );
  }
}

byte preset2ButtonStateHandler() {
  return muxValue[4] == 0 ? false:true;
}

void preset2ClickHandler(Button2& btn) {
  if(iClockMode == CLOCKMODE_MASTER){
    iNextPreset = NEXTPRESET_2;
  }
}

void preset2LongClickDetected(Button2& btn) {
  if(iClockMode == CLOCKMODE_MASTER){
    fBPM_Preset2 = tapTempo.getBPM();
    EEPROM.update(20, byte(fBPM_Preset2));
    ledRed();
  }
}

byte preset3ButtonStateHandler() {
  return muxValue[5] == 0 ? false:true;
}

void preset3ClickHandler(Button2& btn) {
  if(iClockMode == CLOCKMODE_MASTER){
    iNextPreset = NEXTPRESET_3;
  }
}

void preset3LongClickDetected(Button2& btn) {
  if(iClockMode == CLOCKMODE_MASTER){
    fBPM_Preset3 = tapTempo.getBPM();
    EEPROM.update(30, byte(fBPM_Preset3));
    ledRed();
  }
}

void ledIndicateStart(){
  pixels.clear();
  for(int i=0;i<NUM_LEDS; i++){
    pixels.setPixelColor(i, pixels.Color(LED_OFF, LED_ON*.5, LED_OFF));
  }
    pixels.show();
}

void setGlobalBPM(float f){
  bNewBPM = true;
  if(iClockMode==CLOCKMODE_MASTER){
    tapTempo.setBPM( f );
  }
  uClock.setTempo( f );
  //showBPM( f );
}

void ledIndicateMeasure(int pMeasure){
  
  if(bQuantizeRestartWaiting){
    pixels.setPixelColor(pMeasure, pixels.Color(LED_ON, LED_OFF, LED_ON));
  }else{
    pixels.clear();
    pixels.setPixelColor(pMeasure, pixels.Color(LED_OFF, LED_OFF, LED_ON));
  }
  pixels.show();
}

void ledGreen(){
  pixels.clear();
  for(int i=0;i<NUM_LEDS; i++){
    pixels.setPixelColor(i, pixels.Color(LED_OFF, LED_ON, LED_OFF));
  }
    pixels.show();
}

void ledOff(){
  pixels.clear();
  pixels.show();
}

void ledRed(){
  pixels.clear();
  for(int i=0;i<NUM_LEDS; i++){
    pixels.setPixelColor(i, pixels.Color(LED_ON, LED_OFF, LED_OFF));
  }
    pixels.show();
}

void ledInit(){
  pixels.clear();
  for(int i=0; i<NUM_LEDS; i++){
    pixels.setPixelColor(i, pixels.Color(LED_OFF, LED_ON, LED_ON));
  }
  pixels.show();
}

void getPresetsFromEeprom(){
  uint8_t val;
  val = EEPROM.read(10);
  if(val!=255){ // a.k.a. hier wurde schonmal etwas gespeichert
    fBPM_Preset1 = val;
  }

  val = EEPROM.read(20);
  if(val!=255){
    fBPM_Preset2 = val;
  }

  val = EEPROM.read(30);
  if(val!=255){
    fBPM_Preset3 = val;
  }
}

bool encoder0PinALast = false;
bool encoder0PinBLast = false;
uint8_t encoder0Pos = 128;
uint8_t encoder0PosOld = 128;

// Returns -1 / +1
int queryEncoder(){
  int iReturn = 0;
  if ((encoder0PinALast == false) && (muxValue[14] == true)) {
     if (muxValue[15] == false) {
       encoder0Pos--;
     } else {
       encoder0Pos++;
     }
   } 
   
   if ((encoder0PinALast == true) && (muxValue[14] == false)) {
     if (muxValue[15] == true) {
       encoder0Pos--;
     } else {
       encoder0Pos++;
     }
   }
   
   if (encoder0Pos != encoder0PosOld){
     //Serial.print (encoder0Pos);
      //Serial.print(" ");
     if(encoder0Pos%2==0){
      if(encoder0Pos<encoder0PosOld){
        iReturn = -1;
      }else{
        iReturn = 1;
      }
     }
     //Serial.println(iReturn);
     encoder0PosOld = encoder0Pos;
   }
   
   encoder0PinALast = muxValue[14];
  return iReturn;
}

void clearDisplay(){
  //oled.setFont(utf8font10x16);
  oled.clear();
}

void testDisplay(){
  //oled.setFont(utf8font10x16);
  oled.clear();
  oled.print("Hello world!");
}

void showBPM(float p){
  oled.setInvertMode( false );
  if(iClockMode==CLOCKMODE_MASTER){
    oled.setFont(Verdana_digits_24);
  }else if(iClockMode==CLOCKMODE_MIXXX){

  }
  if( bNewBPM ){
    oled.clear();
    bNewBPM = false;
  }

  // to prevent the 'BPM' string from disappearing when tempo is changed on a stopped box
  if(!bIsPlaying){
    bUpdateStatusDisplay = true;
  }
  
  oled.set2X();
  oled.setRow(0);
  if(p<100){
    oled.setCol(30);
  }else{
    oled.setCol(15);
  }
  oled.print( String(p) );

}


void showStatus(int iMeasure, bool pInverted){
  bDisplayInverted = pInverted;
  bUpdateStatusDisplay = true;
}


void showInfo(int pWaitMS){
  oled.setFont(ZevvPeep8x16);
  oled.clear();
  oled.set2X();
  oled.println("ClockBox");
  oled.set1X();
  oled.print("  Version ");oled.println(VERSION);
  oled.println();
  oled.println(" Andyland.info");
  delay(pWaitMS);
}


void fillSysexBuffer(midiEventPacket_t pRX, uint8_t pSize){
  //Serial.println("received data " + String(pRX.byte1, 16));
  if(pRX.byte1 == 0xF0){
    //Serial.println("Clear buffer");
    clearSysexBuffer();
  }

  for(int i=0; i<BUFFERSIZE; i++){
    if(sysexBuffer[i]==0xFF){
      if( pSize==1 ){
        sysexBuffer[i]=pRX.byte1;
      }else if ( pSize==2 ) {
        sysexBuffer[i]=pRX.byte1;
        sysexBuffer[i+1]=pRX.byte2;
      }else if( pSize==3 ){
        sysexBuffer[i]=pRX.byte1;
        sysexBuffer[i+1]=pRX.byte2;
        sysexBuffer[i+2]=pRX.byte3;
      }
      break; //exit for
    }
    if(sysexBuffer[i] == 0xF7){
      processSysexBuffer();
    }
  }
}

void processSysexBuffer(){
  uint8_t x=0;
  for(int i=0; i<BUFFERSIZE; i++){
    if(sysexBuffer[i] != 0xFF){
      x++;
    }
  }
  if(x > 10){
    //Serial.println("Sysex Buffer processing: " + String(x));
    //"0x7F 0x01 0x1"; BPM 
    for(int i=0; i<BUFFERSIZE; i++){
      if((sysexBuffer[i] == 0x7F) && (sysexBuffer[i+1] == 0x01) && (sysexBuffer[i+2] == 0x11)){
        //BPM Deck 1
        
        String s = String(sysexBuffer[i+3], 16) + String(sysexBuffer[i+4], 16) + String(sysexBuffer[i+5], 16) + String(sysexBuffer[i+6], 16) + String(sysexBuffer[i+7], 16);
        //float fBPM = s.toInt();
        float temp = s.toInt();
        temp /= 100.0;
        if(temp > 10.0 ){
          fBPM_Sysex = temp;
          fBPM_Cache = temp;
          //bNewBPM = true;
          setGlobalBPM( fBPM_Sysex );
          uClock.start();
          //showBPM(fBPM_Sysex);
        }
        Serial.print("Found BPM: ");
        Serial.println(String(fBPM_Sysex, 10));

        break;
      }
    }
  }
}

void clearSysexBuffer(){
  for(int i=0; i<BUFFERSIZE; i++){
    sysexBuffer[i] = 0xFF;
  }
}

void nudgePlus(bool pOnOff){
  if(pOnOff){
    //fBPM_NudgeCache = tapTempo.getBPM();//uClock.getTempo();
    bNudgeActive=true;
    fBPM_Cache *= 1.05;
    setGlobalBPM( fBPM_Cache);
  }else{
    bNudgeActive = false;
    fBPM_Cache = fBPM_Sysex;
    setGlobalBPM( fBPM_Cache );
  }
}

void nudgeMinus(bool pOnOff){
  if(pOnOff){
    //fBPM_NudgeCache = tapTempo.getBPM(); //uClock.getTempo();
    bNudgeActive = true;
    fBPM_Cache *= 0.95;
    setGlobalBPM( fBPM_Cache );
  }else{
    fBPM_Cache = fBPM_Sysex;
    setGlobalBPM( fBPM_Cache );
    bNudgeActive = false;
  }
}
