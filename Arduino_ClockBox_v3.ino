/*

    Update uClock to latest version, set internal PPQN to 24, implement CV/Gate out.
    Implement boot into update mode (hold STOP while power on).

    button2 2.2.4

    Bei Clocks mit 2 Presetbuttons gibt es ein Problem beim Sync mit externer Clock und NudgePlus, bzw NudgeMinus

    ToDO: Documentation for incoming midiclock (in via usb, out via ?)
          why uclock AND taptempo?
*/


#include <SoftwareSerial.h>

#include <light_CD74HC4067.h>
#include "TapTempo.h" // Note the quotation marks. Because it's in the same folder as the code itself. Get it, e.g. from https://github.com/Andymann/ArduinoTapTempo
#include <uClock.h>
#include <Button2.h> // FORCE 2.2.4
#include <Adafruit_NeoPixel.h>
#include "MIDIUSB.h"
#include <EEPROM.h>
#include <Wire.h>
#include "SSD1306Ascii.h"
#include "SSD1306AsciiWire.h"


/*
    SELECT WHICH HARDWARE WILL BE USED
*/
#define V3_PROTOBOARD 0
//#define V3_PCB 0

#ifdef V3_PROTOBOARD
#define TAPBUTTON 0
#define STARTBUTTON 1
#define STOPBUTTON 2
#define PRESETBUTTON1 3
#define PRESETBUTTON2 4
#define PRESETBUTTON3 5
#define PRESETSWITCH1 6
#define PRESETSWITCH2 7
#define PRESETSWITCH3 8
#define ENCODERCLICK 13
#define ENCODERPINA 14
#define ENCODERPINB 15
#endif

#ifdef V3_PCB
#define TAPBUTTON 0
#define STARTBUTTON 1
#define STOPBUTTON 2
#define PRESETBUTTON1 3
#define PRESETBUTTON2 4
#define PRESETBUTTON3 5
#define PRESETSWITCH1 6
#define PRESETSWITCH2 7
#define PRESETSWITCH3 8
#define ENCODERCLICK 12
#define ENCODERPINA 13
#define ENCODERPINB 14
#endif


#define MEMLOC_PRESET_1 10
#define MEMLOC_PRESET_2 20
#define MEMLOC_PRESET_3 30
#define MEMLOC_MODE 40
#define MEMLOC_QRSOFFSET 50

// 0X3C+SA0 - 0x3C or 0x3D
#define DISPLAY_I2C_ADDRESS 0x3C
SSD1306AsciiWire oled;

#define VERSION "3.22d"
#define DEMUX_PIN A0

#define SYNC_TX_PIN A2

CD74HC4067 mux(6,7,8,9);  // create a new CD74HC4067 object with its four select lines

#define NUM_LEDS 4
#define DATA_PIN 10
#define LED_ON 59 //60
#define LED_OFF 0
Adafruit_NeoPixel pixels(NUM_LEDS, DATA_PIN, NEO_GRB + NEO_KHZ800);

#define MIDI_CLOCK 0xF8
#define MIDI_START 0xFA
#define MIDI_STOP  0xFC

#define INTERNAL_PPQN 24  // needs to be 24 for CV/ Gate to work properly

uint8_t iQuantizeRestartOffset; // Damit ein restart echt mega genau ankommt; test per Ableton Metronom: //94 sweet spot. 
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

SoftwareSerial midi(16, A3);

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



#define CLOCKMODE_STANDALONE_A 1  // Send Midi-Start on quantized restart
#define CLOCKMODE_STANDALONE_B 2  // Send Midi Stop-Start on quantized restart
#define CLOCKMODE_MIXXX 3 
#define CLOCKMODE_FOLLOW_24PPQN 4
#define CLOCKMODE_FOLLOW_48PPQN 5
#define CLOCKMODE_FOLLOW_72PPQN 6
#define CLOCKMODE_FOLLOW_96PPQN 7
#define MODECOUNT 3
uint8_t arrModes[] = {CLOCKMODE_STANDALONE_A, CLOCKMODE_STANDALONE_B, CLOCKMODE_FOLLOW_24PPQN /*, CLOCKMODE_FOLLOW_48PPQN, CLOCKMODE_FOLLOW_72PPQN, CLOCKMODE_FOLLOW_96PPQN, CLOCKMODE_MIXXX*/};

// you can define whether clock-ticks ("0xF8") are sent continuously or only when the box is playing
// First option might improve syncing for, e.g., Ableton and other products that adopt to midi clock rather slowly 
#define SENDCLOCK_ALWAYS 1
#define SENDCLOCK_WHENPLAYING 2

uint8_t iClockBehaviour = SENDCLOCK_ALWAYS;
uint8_t iClockMode = CLOCKMODE_STANDALONE_A;
//uint8_t iClockMode = CLOCKMODE_FOLLOW;

byte muxValue[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}; // The value of the Buttons as read from the multiplexer

#define BUFFERSIZE 60
byte sysexBuffer[BUFFERSIZE];

midiEventPacket_t midiPacket;

int iTickCounter=0;
boolean bModeSwitched = false;
boolean bQRSChange = false;

boolean bFirmwareUpdateMode = false;

void setup(){

  iNextPreset = NEXTPRESET_NONE;
  //Serial.begin(115200);
  //delay(251);
  pixels.begin();
  ledInit();
  midi.begin(31250);
  pinMode(DEMUX_PIN, INPUT); // Set as input for reading through signal pin
  pinMode(SYNC_TX_PIN, OUTPUT);

  tapTempo.setTotalTapValues(4);

  // Inits the clock
  uClock.init();
  // Set the callback function for the clock output to send MIDI Sync message.
  uClock.setPPQN(INTERNAL_PPQN);
  uClock.setOnPPQN(ClockOutPPQN);
  //uClock.setOnSync24(ClockOutPPQN);
  // Set the callback function for MIDI Start and Stop messages.
  uClock.setOnClockStart(onClockStart);  
  uClock.setOnClockStop(onClockStop);
  // Set the clock BPM
  setGlobalBPM( fBPM_Cache );
  
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
  //btnHelper_PRESET2.setTapHandler(restartHandler);
  btnHelper_PRESET2.setChangedHandler(restartHandler);

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
  iClockMode = getModeFromEeprom();
  iQuantizeRestartOffset = getQRSOffsetFromEeprom();


  showInfo(1500);
  ledOff();


  if(iClockBehaviour == SENDCLOCK_ALWAYS){
    uClock.start();
  }

  readMux();
  if( muxValue[STOPBUTTON] == 1 ){
    bFirmwareUpdateMode = true;
    showUpdateInfo();
  }

}//setup

void loop(){

  if( bFirmwareUpdateMode ){
    // Not sure if this helps but when uploading new stuff it might be
    // beneficial to not shoot midi data (via USB)
    delay(1);
    return 0;
  }
  
  readMux();

  checkForModeSwitch();

  btnHelper_TAP.loop();
  btnHelper_START.loop();
  btnHelper_STOP.loop();
  btnHelper_PRESET1.loop();
  btnHelper_PRESET2.loop();
  btnHelper_PRESET3.loop();

  tapTempo.update(false);

  if((iClockMode==CLOCKMODE_STANDALONE_A) || (iClockMode==CLOCKMODE_STANDALONE_B)){
    if( abs(tapTempo.getBPM() - fBPM_Cache) >= 1.0){
      bNewBPM = true;
      fBPM_Cache = uint8_t(tapTempo.getBPM());   // Nur ganzzahlige Werte darstellen, Rundungsfehler ueberdecken
      //uClock.setTempo( fBPM_Cache );
      setGlobalBPM(fBPM_Cache);
      if(bIsPlaying){
//        showBPM( fBPM_Cache );
      }else{
        showStatus( 0, false );
      }
    }
  }else if(iClockMode==CLOCKMODE_MIXXX){
    if( abs(uClock.getTempo() - fBPM_Sysex) >= .1){
      if( bNudgeActive==false ){
        fBPM_Cache = fBPM_Sysex;
        setGlobalBPM(fBPM_Sysex);
        if(bIsPlaying){
//          showBPM( fBPM_Sysex );
        }else{
          showStatus( 0, false );
        }
      }else{
        if(bIsPlaying){
//          showBPM( fBPM_Cache );
        }else{
          showStatus( 0, false );
        }
      }
    }
  }
  
  else if(iClockMode==CLOCKMODE_FOLLOW_24PPQN || CLOCKMODE_FOLLOW_48PPQN || CLOCKMODE_FOLLOW_72PPQN || CLOCKMODE_FOLLOW_96PPQN){
    if( abs(tapTempo.getBPM() - fBPM_Cache) >= 1.0){
      if( !bNudgeActive ){
        bNewBPM = true;
        fBPM_Cache = (tapTempo.getBPM());
        uClock.setTempo( fBPM_Cache );
        if(bIsPlaying){
//          showBPM( fBPM_Cache );
        }else{
          showStatus( 0, false );
        }
      }
    }
  }
  
  int iEncoder = queryEncoder();


  if(iEncoder!=0){
    if( !bQRSChange ){
      if((iClockMode==CLOCKMODE_STANDALONE_A) || (iClockMode==CLOCKMODE_STANDALONE_B)){
        if(muxValue[ENCODERCLICK]==0){
          fBPM_Cache += iEncoder;
        }else{
          fBPM_Cache += iEncoder*5.0;
        }
        bNewBPM = true;
        setGlobalBPM( fBPM_Cache);
      }else if(iClockMode==CLOCKMODE_MIXXX){
        if(muxValue[ENCODERCLICK]==0){
          fBPM_Sysex += iEncoder*1.;
          fBPM_Cache = fBPM_Sysex;
        }else{
          fBPM_Sysex += iEncoder*0.1;
          fBPM_Cache = fBPM_Sysex;
        }
        bNewBPM = true;
        setGlobalBPM( fBPM_Cache );
      }else if(iClockMode==CLOCKMODE_FOLLOW_24PPQN || CLOCKMODE_FOLLOW_48PPQN || CLOCKMODE_FOLLOW_72PPQN || CLOCKMODE_FOLLOW_96PPQN){
        if(muxValue[ENCODERCLICK]==0){
          fBPM_Cache += iEncoder;
        }else{
          fBPM_Cache += iEncoder*.10;
        }
        bNewBPM = true;
        setGlobalBPM( fBPM_Cache);
      }
    }else{
      // bQRSChange = true
      if(iEncoder < 0){
        if(iQuantizeRestartOffset>1){
          iQuantizeRestartOffset += iEncoder;
        }
      }else if(iEncoder > 0){
        if(iQuantizeRestartOffset<INTERNAL_PPQN-1){
          iQuantizeRestartOffset += iEncoder;
        }
      }
    }
  }

  if(iNextPreset != NEXTPRESET_NONE){
    selectPreset(iNextPreset);
    iNextPreset = NEXTPRESET_NONE;
  }

  if(bUpdateStatusDisplay==true){
    updateStatusDisplay();
    bUpdateStatusDisplay=false;
  }

  if(bNewPresetSelected == true){
    displaySelectedPreset( sActivePreset );
    bNewPresetSelected = false;
  }

  checkMidiUSB();
  checkMidiDIN();
  
}//

// Standalone, mixxx, follow ...if(muxValue[ENCODERCLICK]==

void checkForModeSwitch(){
  if(muxValue[ENCODERCLICK] && muxValue[STARTBUTTON]){
    if( !bModeSwitched ){
      bModeSwitched = true;
      
      for(int i=0; i<MODECOUNT; i++){
        if(iClockMode==arrModes[i]){
          iClockMode = arrModes[(i+1)%MODECOUNT];
          iTickCounter=0;
          oled.clear();
          EEPROM.update(MEMLOC_MODE, byte(iClockMode));
          break;
        }
      }
    }else{
      //bModeSwitched = false;
    }
  }
  if((muxValue[ENCODERCLICK] == 0) && (muxValue[STARTBUTTON] == 0)){
    bModeSwitched = false;
  }

  if(muxValue[ENCODERCLICK] && muxValue[STOPBUTTON]){
    if( !bQRSChange ){
      bQRSChange = true;
    }
  }
  if((muxValue[ENCODERCLICK] == 0) && (muxValue[STOPBUTTON] == 0)){
      
    if( bQRSChange == true){
      oled.clear(); // Artefakte loeschen
      EEPROM.update(MEMLOC_QRSOFFSET, byte(iQuantizeRestartOffset));
    }
    bQRSChange = false;
    
  }

}

void updateStatusDisplay(){
  if( !bQRSChange ){
    showBPM(fBPM_Cache);
    oled.setInvertMode( bDisplayInverted );
    oled.setFont(ZevvPeep8x16);
    oled.set1X();
    oled.setRow(6);
    if(iClockMode==CLOCKMODE_STANDALONE_A){
      oled.setCol(1);
      oled.setInvertMode( false );
      oled.print("QRS Start");
      oled.setCol(112);
      oled.setInvertMode( bDisplayInverted );
      oled.setRow(0);
      oled.print("  ");
    }else if(iClockMode==CLOCKMODE_STANDALONE_B){
      oled.setCol(1);
      oled.setInvertMode( false );
      oled.print("QRS Stop-Start");
      oled.setCol(112);
      oled.setInvertMode( bDisplayInverted );
      oled.setRow(0);
      oled.print("  ");
    }else if( iClockMode == CLOCKMODE_MIXXX ){
      oled.setCol(88);
      oled.print("SYSEX");
    }else if( iClockMode == CLOCKMODE_FOLLOW_24PPQN ){
      oled.setCol(32);
      oled.print("ext.Clock 24");
    }else if( iClockMode == CLOCKMODE_FOLLOW_48PPQN ){
      oled.setCol(32);
      oled.print("ext.Clock 48");
    }else if( iClockMode == CLOCKMODE_FOLLOW_72PPQN ){
      oled.setCol(32);
      oled.print("ext.Clock 72");
    }else if( iClockMode == CLOCKMODE_FOLLOW_96PPQN ){
      oled.setCol(32);
      oled.print("ext.Clock 96");
    }
  }else{
    // bQRSChange = true
    oled.clear();
    oled.setInvertMode( false );
    oled.set1X();
    oled.setRow(0);
    oled.print( "QRS Offset:");
    oled.setRow(3);
    oled.set2X();
    if(iQuantizeRestartOffset<100){
      oled.setCol(30);
    }else{
      oled.setCol(15);
    }
    oled.print( String( iQuantizeRestartOffset) );
  }
  
}


void displaySelectedPreset(String p){
  oled.setInvertMode( false );
  oled.setFont(ZevvPeep8x16);
  oled.set1X();
  oled.setRow(6);
  oled.setCol(5);
  oled.print( p );
}

void checkMidiUSB(){
  midiPacket = MidiUSB.read();
  if (midiPacket.header != 0) {
    processMidiIn( midiPacket );
  }
}

void checkMidiDIN(){
    while (midi.available()>0){
      //uint8_t x = midi.read();
      //midiPacket.byte1 = x;
      midiPacket.byte1 = midi.read();
      if( midiPacket.byte1==0xF0 ){
        iClockMode = CLOCKMODE_MIXXX;
        //fillSysexBuffer( midiPacket, 1 );
      }else if(midiPacket.byte1==0xF7){

      }else{

      } 

      if( iClockMode == CLOCKMODE_MIXXX ){
        fillSysexBuffer( midiPacket, 1 );
        processSysexBuffer();
      }
      
    }
}


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
      //iClockMode = CLOCKMODE_MIXXX;
      if(iClockMode==CLOCKMODE_MIXXX){
        fillSysexBuffer( pRX, 3 );
        processSysexBuffer();
      }
      break;
    case 0x05:  // Single-byte System Common Message or SysEx ends with the following single byte
      // append sysex buffer with 1 byte
      if(iClockMode==CLOCKMODE_MIXXX){
        fillSysexBuffer( pRX, 1 );
        processSysexBuffer();
      }
      break;
    case 0x06:  // SysEx ends with the following two bytes
      // append sysex buffer with 2 bytes
      if(iClockMode==CLOCKMODE_MIXXX){
        fillSysexBuffer( pRX, 2 );
        processSysexBuffer();
      }
      break;
    case 0x07:  // SysEx ends with the following three bytes
      // append sysex buffer with 3 bytes
      if(iClockMode==CLOCKMODE_MIXXX){
        fillSysexBuffer( pRX, 3 );
        processSysexBuffer();
      }
      break;
    case 0x0F:  // Single Byte, TuneRequest, Clock, Start, Continue, Stop, etc.
      processIncomingClock();
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
//  showBPM( fBPM_Cache );
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

// The callback function wich will be called by Clock each Pulse of clock resolution.
void ClockOutPPQN(uint32_t tick) {
    // Send MIDI_CLOCK to external gears
    sendMidiClock();
    handle_bpm_led(tick);
}


// The callback function wich will be called when clock starts by using Clock.start() method.
void onClockStart() {
  //sendMidiStart();
}

// The callback function wich will be called when clock stops by using Clock.stop() method.
void onClockStop() {
  //sendMidiStop();
  //ledOff();
}



void processIncomingClock(){
  if(iClockMode==CLOCKMODE_FOLLOW_24PPQN){
    if(iTickCounter>=23){
      iTickCounter=0;
      if( !bNudgeActive ){
        tapTempo.update( true );
      }
    }else{
      iTickCounter++;
    }
  }else if(iClockMode==CLOCKMODE_FOLLOW_48PPQN){
    if(iTickCounter>=47){
      iTickCounter=0;
      if( !bNudgeActive ){
        tapTempo.update( true );
      }
    }else{
      iTickCounter++;
    }
  }else if(iClockMode==CLOCKMODE_FOLLOW_72PPQN){
    if(iTickCounter>=71){
      iTickCounter=0;
      if( !bNudgeActive ){
        tapTempo.update( true );
        //Serial.println(tapTempo.getBPM() );
      }
    }else{
      iTickCounter++;
    }
  }else if(iClockMode==CLOCKMODE_FOLLOW_96PPQN){
    if(iTickCounter>=95){
      iTickCounter=0;
      if( !bNudgeActive ){
        tapTempo.update( true );
      }
    }else{
      iTickCounter++;
    }
  }
}

void sendMidiClock(){
  if((iClockMode==CLOCKMODE_STANDALONE_A) || (iClockMode==CLOCKMODE_STANDALONE_B)){
     midiEventPacket_t p = {0x0F, MIDI_CLOCK, 0, 0};
    MidiUSB.sendMIDI(p);
    MidiUSB.flush();
    midi.write(MIDI_CLOCK);
  }else if(iClockMode==CLOCKMODE_MIXXX){
     midiEventPacket_t p = {0x0F, MIDI_CLOCK, 0, 0};
    MidiUSB.sendMIDI(p);
    MidiUSB.flush();
    midi.write(MIDI_CLOCK);
  }else if((iClockMode == CLOCKMODE_FOLLOW_24PPQN)||(iClockMode == CLOCKMODE_FOLLOW_48PPQN)||(iClockMode == CLOCKMODE_FOLLOW_72PPQN)||(iClockMode == CLOCKMODE_FOLLOW_96PPQN)){
    midi.write(MIDI_CLOCK);
  }
 
}

void sendMidiStart(){
  if((iClockMode==CLOCKMODE_STANDALONE_A) || (iClockMode==CLOCKMODE_STANDALONE_B)){
    midiEventPacket_t p = {0x0F, MIDI_START, 0, 0};
    MidiUSB.sendMIDI(p);
    MidiUSB.flush();
    midi.write(MIDI_START);
  }else if(iClockMode==CLOCKMODE_MIXXX){
    midiEventPacket_t p = {0x0F, MIDI_START, 0, 0};
    MidiUSB.sendMIDI(p);
    MidiUSB.flush();
    midi.write(MIDI_START);
  }else if((iClockMode == CLOCKMODE_FOLLOW_24PPQN)||(iClockMode == CLOCKMODE_FOLLOW_48PPQN)||(iClockMode == CLOCKMODE_FOLLOW_72PPQN)||(iClockMode == CLOCKMODE_FOLLOW_96PPQN)){
    midi.write(MIDI_START);
  }
}

void sendMidiStop(){
  if((iClockMode==CLOCKMODE_STANDALONE_A) || (iClockMode==CLOCKMODE_STANDALONE_B)){
    midiEventPacket_t p = {0x0F, MIDI_STOP, 0, 0};
    MidiUSB.sendMIDI(p);
    MidiUSB.flush();
    midi.write(MIDI_STOP);
  }else if(iClockMode==CLOCKMODE_MIXXX){
    midiEventPacket_t p = {0x0F, MIDI_STOP, 0, 0};
    MidiUSB.sendMIDI(p);
    MidiUSB.flush();
    midi.write(MIDI_STOP);
  }else if((iClockMode == CLOCKMODE_FOLLOW_24PPQN)||(iClockMode == CLOCKMODE_FOLLOW_48PPQN)||(iClockMode == CLOCKMODE_FOLLOW_72PPQN)||(iClockMode == CLOCKMODE_FOLLOW_96PPQN)){
    midi.write(MIDI_STOP);
  }
}

void handle_bpm_led(uint32_t tick)
{

  if( bQuantizeRestartWaiting == true){
    if( (tick % (INTERNAL_PPQN*4) == (INTERNAL_PPQN*4 - iQuantizeRestartOffset) )){
      bQuantizeRestartWaiting = false;
      if(iClockMode==CLOCKMODE_STANDALONE_B){
        sendMidiStop();
      }
      sendMidiStart();
    }
  }
  
  // at the beginning or on every downbeat
  if((tick==1)||( !(tick % (INTERNAL_PPQN*4)))){
    bpm_blink_timer = 12;
    iMeasureCount = 0;
    ledIndicateMeasure( iMeasureCount );
    iMeasureCount++;
    showStatus(0, true);

  } else if ( !(tick % (INTERNAL_PPQN)) ) {   // each quarter note //led on
    bpm_blink_timer = 8;
    ledIndicateMeasure( iMeasureCount );
    iMeasureCount++;
    showStatus(0, true);

  } else if ( !(tick % bpm_blink_timer) ) { // get led off
    if(!bQuantizeRestartWaiting){
      ledOff();
    }
    showStatus(0, false);
  }

  //CV Gate out
  if ( !(tick % 6) ) {
    digitalWrite(SYNC_TX_PIN, true);
  }

  if ( !((tick-1)% 6) ) {
    digitalWrite(SYNC_TX_PIN, false);
  }


}

void tapHandler(Button2& btn) {
    //Serial.println("tap Handler");
    //if((iClockMode==CLOCKMODE_STANDALONE_A)||(iClockMode==CLOCKMODE_MIXXX)){
      tapTempo.update(true);
    //}
}

byte tapButtonStateHandler() {
  return muxValue[TAPBUTTON] == 0 ? false:true;
}


void startHandler(Button2& btn) {
  if( !muxValue[ENCODERCLICK] ){
    startPlaying();
  }
}

void restartHandler(Button2& btn){
  if(iClockMode==CLOCKMODE_MIXXX){
    //bool b = muxValue[4]||muxValue[7]; ;
    if( btn.isPressed() ){
      if(!bIsPlaying){
        bNewBPM = true;
//        showBPM( fBPM_Cache );
        //sendMidiStop();
        sendMidiStart();
        //if(iClockBehaviour == SENDCLOCK_WHENPLAYING){
          uClock.start();
        //}
        bIsPlaying = true;
      }else{
        stopPlaying();
      }
      
    }
  }
  
}

void startPlaying(){
  if((!bIsPlaying) && (!bModeSwitched)){
    bNewBPM = true;
//    showBPM( fBPM_Cache );
    sendMidiStart();
    uClock.start(); //if already running this causes a clock reset (-> LED handling, tick,)
  }else{
    bQuantizeRestartWaiting = true;
  }
  bIsPlaying = true;
  showStatus(iMeasureCount+1, false);
}

byte startButtonStateHandler() {
  return muxValue[STARTBUTTON] == 0 ? false:true;
}


void stopHandler(Button2& btn) {
    stopPlaying();
}

void stopPlaying(){
  bIsPlaying = false;
  bQuantizeRestartWaiting = false;
  sendMidiStop();
  ledOff();
  showStatus(iMeasureCount+1, false);
  if(iClockBehaviour == SENDCLOCK_WHENPLAYING){
      uClock.stop();
    }
}

byte stopButtonStateHandler() {
  return muxValue[STOPBUTTON] == 0 ? false:true;
}




//Button or Footswitch
byte preset1ButtonStateHandler() {
  bool b = muxValue[PRESETBUTTON1]||muxValue[PRESETSWITCH1];
  //return muxValue[3] == 0 ? false : true;
  return b == 0 ? false : true;
}

void preset1ClickHandler(Button2& btn) {
  if((iClockMode==CLOCKMODE_STANDALONE_A) || (iClockMode==CLOCKMODE_STANDALONE_B)){
    iNextPreset = NEXTPRESET_1;
  }
}

void preset1LongClickDetected(Button2& btn) {
    if((iClockMode==CLOCKMODE_STANDALONE_A) || (iClockMode==CLOCKMODE_STANDALONE_B)){
      fBPM_Preset1 = tapTempo.getBPM();
      EEPROM.update(MEMLOC_PRESET_1, byte(fBPM_Preset1));
      ledRed();
    }
}


void preset1ChangeHandler( Button2& btn ){
  if(iClockMode == CLOCKMODE_MIXXX){
    nudgeMinus( btn.isPressed() );
  }else if((iClockMode == CLOCKMODE_FOLLOW_24PPQN)||(iClockMode == CLOCKMODE_FOLLOW_48PPQN)||(iClockMode == CLOCKMODE_FOLLOW_72PPQN)||(iClockMode == CLOCKMODE_FOLLOW_96PPQN)){
    //if(!bNudgeActive){
      nudgeMinus( btn.isPressed() );
    //}
  }
}

void preset3ChangeHandler( Button2& btn ){
  if(iClockMode == CLOCKMODE_MIXXX){
    nudgePlus( btn.isPressed() );
  }else if((iClockMode == CLOCKMODE_FOLLOW_24PPQN)||(iClockMode == CLOCKMODE_FOLLOW_48PPQN)||(iClockMode == CLOCKMODE_FOLLOW_72PPQN)||(iClockMode == CLOCKMODE_FOLLOW_96PPQN)){
    //if(!bNudgeActive){
      nudgePlus( btn.isPressed() );
    //}
  }
}

byte preset2ButtonStateHandler() {
  bool b = muxValue[PRESETBUTTON2]||muxValue[PRESETSWITCH2];
  return b == 0 ? false : true;
}

void preset2ClickHandler(Button2& btn) {
  if((iClockMode==CLOCKMODE_STANDALONE_A) || (iClockMode==CLOCKMODE_STANDALONE_B)){
    iNextPreset = NEXTPRESET_2;
  }
}

void preset2LongClickDetected(Button2& btn) {
  if((iClockMode==CLOCKMODE_STANDALONE_A) || (iClockMode==CLOCKMODE_STANDALONE_B)){
    fBPM_Preset2 = tapTempo.getBPM();
    EEPROM.update(MEMLOC_PRESET_2, byte(fBPM_Preset2));
    ledRed();
  }else if(iClockMode == CLOCKMODE_MIXXX){
    //bool b = muxValue[5]||muxValue[8];
    //if(btn.isPressed()){
    //  stopPlaying();
    //}
  }
}

byte preset3ButtonStateHandler() {
  bool b = muxValue[PRESETBUTTON3]||muxValue[PRESETSWITCH3];
  return b == 0 ? false : true;
}

void preset3ClickHandler(Button2& btn) {
  if((iClockMode==CLOCKMODE_STANDALONE_A) || (iClockMode==CLOCKMODE_STANDALONE_B)){
    iNextPreset = NEXTPRESET_3;
  }
}

void preset3LongClickDetected(Button2& btn) {
  if((iClockMode==CLOCKMODE_STANDALONE_A) || (iClockMode==CLOCKMODE_STANDALONE_B)){
    fBPM_Preset3 = tapTempo.getBPM();
    EEPROM.update(MEMLOC_PRESET_3, byte(fBPM_Preset3));
    ledRed();
  }
}

void ledIndicateStart(){
  if(bIsPlaying){
    pixels.clear();
    for(int i=0;i<NUM_LEDS; i++){
      pixels.setPixelColor(i, pixels.Color(LED_OFF, LED_ON*.5, LED_OFF));
    }
    pixels.show();
  }
}

void setGlobalBPM(float f){
  //Serial.println("setGlobalBPM " + String(f));
  bNewBPM = true;
  if((iClockMode==CLOCKMODE_STANDALONE_A) || (iClockMode==CLOCKMODE_STANDALONE_B)){
    tapTempo.setBPM( f );
  }else if(iClockMode==CLOCKMODE_MIXXX){
    
  }else if((iClockMode == CLOCKMODE_FOLLOW_24PPQN)||(iClockMode == CLOCKMODE_FOLLOW_48PPQN)||(iClockMode == CLOCKMODE_FOLLOW_72PPQN)||(iClockMode == CLOCKMODE_FOLLOW_96PPQN)){
    tapTempo.setBPM( f );
  }
  uClock.setTempo( f );
}

void ledIndicateMeasure(int pMeasure){
  if(bIsPlaying){
    if(bQuantizeRestartWaiting){
      pixels.setPixelColor(pMeasure, pixels.Color(LED_ON, LED_OFF, LED_ON));
    }else{
      if(pMeasure==0){
        ledGreen();
      }else{
        pixels.clear();
        pixels.setPixelColor(pMeasure, pixels.Color(LED_OFF, LED_OFF, LED_ON));
      }
    }
    pixels.show();
  }
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
  val = EEPROM.read(MEMLOC_PRESET_1);
  if(val!=255){ // a.k.a. hier wurde schonmal etwas gespeichert
    fBPM_Preset1 = val;
  }

  val = EEPROM.read(MEMLOC_PRESET_2);
  if(val!=255){
    fBPM_Preset2 = val;
  }

  val = EEPROM.read(MEMLOC_PRESET_3);
  if(val!=255){
    fBPM_Preset3 = val;
  }
}

uint8_t getModeFromEeprom(){
  uint8_t val;
  val = EEPROM.read(MEMLOC_MODE);
  if(val!=255){ // a.k.a. hier wurde schonmal etwas gespeichert
    return val;
  }else{
    return CLOCKMODE_STANDALONE_A;
  }
}

uint8_t getQRSOffsetFromEeprom(){
  uint8_t val;
  val = EEPROM.read(MEMLOC_QRSOFFSET);
  if(val!=255){ // a.k.a. hier wurde schonmal etwas gespeichert
    return val;
  }else{
    return 1;//94; //Default, passt gut mit Ableton
  }
}

bool encoder0PinALast = false;
bool encoder0PinBLast = false;
uint8_t encoder0Pos = 128;
uint8_t encoder0PosOld = 128;

// Returns -1 / +1
int queryEncoder(){
  int iReturn = 0;
  if ((encoder0PinALast == false) && (muxValue[ENCODERPINA] == true)) {
     if (muxValue[ENCODERPINB] == false) {
       encoder0Pos--;
     } else {
       encoder0Pos++;
     }
   } 
   
   if ((encoder0PinALast == true) && (muxValue[ENCODERPINA] == false)) {
     if (muxValue[ENCODERPINB] == true) {
       encoder0Pos--;
     } else {
       encoder0Pos++;
     }
   }
   
   if (encoder0Pos != encoder0PosOld){
     if(encoder0Pos%2==0){
      if(encoder0Pos<encoder0PosOld){
        iReturn = -1;
      }else{
        iReturn = 1;
      }
     }
     encoder0PosOld = encoder0Pos;
   }
   encoder0PinALast = muxValue[ENCODERPINA];
  return iReturn;
}

void clearDisplay(){
  oled.clear();
}

void testDisplay(){
  oled.clear();
  oled.print("Hello world!");
}

void showBPM(float p){
  oled.setInvertMode( false );
  if((iClockMode==CLOCKMODE_STANDALONE_A) || (iClockMode==CLOCKMODE_STANDALONE_B)){
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

void showUpdateInfo(){
  oled.clear();
  oled.setFont(ZevvPeep8x16);
  oled.set2X();
  oled.println("ClockBox");
  oled.set1X();
  oled.println();//("  Version ");oled.println(VERSION);
  oled.println();
  oled.println("   UpdateMode");
}

void fillSysexBuffer(midiEventPacket_t pRX, uint8_t pSize){
  if(pRX.byte1 == 0xF0){
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
  }
}

void processSysexBuffer(){
  bool bBufferComplete = false;
  uint8_t x=0;
  for(int i=0; i<BUFFERSIZE; i++){
    if(sysexBuffer[i] != 0xFF){
      x++;
    }
    if(sysexBuffer[i] == 0xF7){
      bBufferComplete = true;
    }
  }
  if( (x > 2) && (bBufferComplete) ){
    for(int i=0; i<BUFFERSIZE; i++){
      if((sysexBuffer[i] == 0x7F) && (sysexBuffer[i+1] == 0x01) /*&& (sysexBuffer[i+2] == 0x11)*/){
        //BPM. Since we only have 1 clock instance there is no need to distinguish between decks
        String s = String(sysexBuffer[i+3], 16) + String(sysexBuffer[i+4], 16) + String(sysexBuffer[i+5], 16) + String(sysexBuffer[i+6], 16) + String(sysexBuffer[i+7], 16);
        float temp = s.toInt();
        temp /= 100.0;
        if(temp > 10.0 ){
          fBPM_Sysex = temp;
          fBPM_Cache = temp;
          setGlobalBPM( fBPM_Sysex );
          uClock.start();
        }
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
    //if((iClockMode == CLOCKMODE_FOLLOW_24PPQN)||(iClockMode == CLOCKMODE_FOLLOW_48PPQN)||(iClockMode == CLOCKMODE_FOLLOW_72PPQN)||(iClockMode == CLOCKMODE_FOLLOW_96PPQN)){
      fBPM_Sysex = fBPM_Cache;
    //}
    bNudgeActive=true;
    fBPM_Cache *= 1.075;
    setGlobalBPM( fBPM_Cache );

    //}
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
    fBPM_Sysex = fBPM_Cache;
    fBPM_Cache *= 0.925;
    setGlobalBPM( fBPM_Cache );
  }else{
    fBPM_Cache = fBPM_Sysex;
    setGlobalBPM( fBPM_Cache );
    bNudgeActive = false;
  }
}
