/*

    BREAKING CHANGE: use Hardware Serial

    Select between incoming clock from DIN or USB or to react on START and STOP only without sync via CLOCK
    button2 2.2.4

    Bei Clocks mit 2 Presetbuttons gibt es ein Problem beim Sync mit externer Clock und NudgePlus, bzw NudgeMinus

    DJ Mode

    ToDO: Documentation for incoming midiclock (in via usb, out via ?)
          why uclock AND taptempo? => uClock.tap() ist noch nicht implementiert

    
*/

#include <light_CD74HC4067.h>
#include "TapTempo.h"  // Note the quotation marks. Because it's in the same folder as the code itself. Get it, e.g. from https://github.com/Andymann/ArduinoTapTempo
#include <uClock.h>
#include <Button2.h>  // FORCE 2.2.4
#include <Adafruit_NeoPixel.h>
#include "MIDIUSB.h"
#include <EEPROM.h>
#include <Wire.h>

/*
    SELECT WHICH HARDWARE WILL BE USED
*/
//#define V3_PROTOBOARD
#define V3_PCB


/*
  Select which type of display will be used
*/

#define DISPLAY_128x64


#ifdef DISPLAY_128x64
#include "SSD1306Ascii.h"
#include "SSD1306AsciiWire.h"
// 0X3C+SA0 - 0x3C or 0x3D
#define DISPLAY_I2C_ADDRESS 0x3C
SSD1306AsciiWire i2cDisplay;
#endif


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



#define VERSION "3.37"
#define DEMUX_PIN A0

#define SYNC_TX_PIN A2

CD74HC4067 mux(6, 7, 8, 9);  // create a new CD74HC4067 object with its four select lines

#define NUM_LEDS 4
#define DATA_PIN 10
#define LED_ON 59  //60
#define LED_OFF 0
Adafruit_NeoPixel pixels(NUM_LEDS, DATA_PIN, NEO_GRB + NEO_KHZ800);

#define MIDI_CLOCK 0xF8
#define MIDI_START 0xFA
#define MIDI_STOP 0xFC

#define INTERNAL_PPQN 24  // needs to be 24 for CV/ Gate to work properly

uint8_t iQuantizeRestartOffset;  // Damit ein restart sehr genau ankommt; test per Ableton Metronom: 2 ist sweet spot, 24 sendet stop auf der '3'

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

uint8_t bpm_blink_timer = 1;
float fBPM_Cache = 94.0;
float fBPM_Sysex;
bool bQuantizeRestartWaiting = false;
uint8_t iMeasureCount = 0;

float fBPM_Preset1 = 80.0;
float fBPM_Preset2 = 100.0;
float fBPM_Preset3 = 120.0;

bool bIsPlaying = false;
bool bNudgeActive = false;

uint8_t DISPLAYUPDATE_NONE = 0;
uint8_t DISPLAYUPDATE_ALL = 1;
uint8_t DISPLAYUPDATE_BLINKER_ON = 2;
uint8_t DISPLAYUPDATE_BLINKER_OFF = 3;
uint8_t DISPLAYUPDATE_BPM = 4;
uint8_t iUpdateDisplayMode = DISPLAYUPDATE_NONE;

bool encoder0PinALast = false;
bool encoder0PinBLast = false;
uint8_t encoder0Pos = 128;
uint8_t encoder0PosOld = 128;


#define CLOCKMODE_STANDALONE_A 1  // Send Midi-Start on quantized restart
#define CLOCKMODE_STANDALONE_B 2  // Send Midi Stop-Start on quantized restart
#define CLOCKMODE_FOLLOW_24PPQN_DIN 3
#define CLOCKMODE_FOLLOW_24PPQN_USB 4
#define CLOCKMODE_FOLLOW_STARTSTOP_DIN 5
#define CLOCKMODE_FOLLOW_STARTSTOP_USB 6
#define MODECOUNT 6
uint8_t arrModes[] = { CLOCKMODE_STANDALONE_A, CLOCKMODE_STANDALONE_B, CLOCKMODE_FOLLOW_24PPQN_DIN, CLOCKMODE_FOLLOW_24PPQN_USB, CLOCKMODE_FOLLOW_STARTSTOP_DIN, CLOCKMODE_FOLLOW_STARTSTOP_USB /*, CLOCKMODE_PASSIVE_CLOCK_FILTER_DIN, CLOCKMODE_PASSIVE_CLOCK_FILTER_USB*/ };

// you can define whether clock-ticks ("0xF8") are sent continuously or only when the box is playing
// First option might improve syncing for, e.g., Ableton and other products that adopt to midi clock rather slowly
#define SENDCLOCK_ALWAYS 1
#define SENDCLOCK_WHENPLAYING 2

uint8_t iClockBehaviour = SENDCLOCK_ALWAYS;
uint8_t iClockMode;

bool muxValue[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };  // The value of the Buttons as read from the multiplexer

midiEventPacket_t midiPacket;

int iTickCounter = 0;
bool bFirmwareUpdateMode = false;
bool bDisplayBlinker = false;
bool bStaticContentDrawnOnce = false;

uint8_t iBeatCounter = 0;
uint8_t iBeatCounter_old = 255;

const float TARGET_BPM_NONE = .0;
float fTargetBPM = TARGET_BPM_NONE;
float fBPM_Diff = TARGET_BPM_NONE;

void setup() {
  iNextPreset = NEXTPRESET_NONE;
  Serial1.begin(31250);
  pixels.begin();
  ledInit();
  pinMode(DEMUX_PIN, INPUT);  // Set as input for reading through signal pin
  pinMode(SYNC_TX_PIN, OUTPUT);

  tapTempo.setTotalTapValues(4);

  btnHelper_TAP.begin(VIRTUAL_PIN);
  btnHelper_TAP.setButtonStateFunction(tapButtonStateHandler);
  btnHelper_TAP.setTapHandler(tapHandler);

  btnHelper_START.begin(VIRTUAL_PIN);
  btnHelper_START.setButtonStateFunction(startButtonStateHandler);
  btnHelper_START.setTapHandler(startHandler);

  btnHelper_STOP.begin(VIRTUAL_PIN);
  btnHelper_STOP.setButtonStateFunction(stopButtonStateHandler);
  btnHelper_STOP.setTapHandler(stopHandler);

  btnHelper_PRESET1.begin(VIRTUAL_PIN);
  btnHelper_PRESET1.setButtonStateFunction(preset1ButtonStateHandler);
  btnHelper_PRESET1.setTapHandler(preset1ClickHandler);

  btnHelper_PRESET2.begin(VIRTUAL_PIN);
  btnHelper_PRESET2.setButtonStateFunction(preset2ButtonStateHandler);
  btnHelper_PRESET2.setTapHandler(preset2ClickHandler);

  btnHelper_PRESET3.begin(VIRTUAL_PIN);
  btnHelper_PRESET3.setButtonStateFunction(preset3ButtonStateHandler);
  btnHelper_PRESET3.setTapHandler(preset3ClickHandler);

#ifdef DISPLAY_128x64
  Wire.begin();
  Wire.setClock(400000L);
  i2cDisplay.begin(&Adafruit128x64, DISPLAY_I2C_ADDRESS);
#endif


  getPresetsFromEeprom();
  iClockMode = getModeFromEeprom();
  iQuantizeRestartOffset = getQRSOffsetFromEeprom();

  readMux();
  if (muxValue[STOPBUTTON] == 1) {
    bFirmwareUpdateMode = true;
    uClock.stop();
    ledRed();
    showUpdateInfo();
    return;
  }

  if ((muxValue[STOPBUTTON] == 1) && (muxValue[TAPBUTTON] == 1)) {
    EEPROM.update(MEMLOC_MODE, byte(CLOCKMODE_STANDALONE_A));
    showModeResetInfo(2000);
  }

  showInfo(2000);
  clearDisplay();
  ledOff();

  initClock();
  processModeSwitch();

}  //setup

void initClock() {
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
  setGlobalBPM(fBPM_Cache);
}


void updateDisplay(bool pClearAll, bool pBLinkerOnOff, bool pClearBPM) {
#ifdef DISPLAY_128x64
  updateDisplay_128x64(pClearAll, pBLinkerOnOff, pClearBPM);
#endif

}

const uint8_t CONFIGCHANGE_NONE = 0;
const uint8_t CONFIGCHANGE_MODE = 1;
const uint8_t CONFIGCHANGE_QRS = 2;
uint8_t iConfigChangeMode = CONFIGCHANGE_NONE;



void loop() {

  if (bFirmwareUpdateMode) {
    // engaged in setup()
    // when uploading new stuff it might be
    // beneficial to not continuously shoot midi data (via USB)
    delay(100);
    return;
  }

  readMux();  // writes into muxValue[]

  tapTempo.update(false);
  btnHelper_TAP.loop();
  btnHelper_START.loop();
  btnHelper_STOP.loop();
  btnHelper_PRESET1.loop();
  btnHelper_PRESET2.loop();
  btnHelper_PRESET3.loop();

  if ((muxValue[ENCODERCLICK] == 1) && (muxValue[STARTBUTTON] == 1)) {
    if (iConfigChangeMode == CONFIGCHANGE_NONE) {  // preventing loops when 2 buttons are pressed, using only one boolean for every mode switch
      iConfigChangeMode = CONFIGCHANGE_MODE;
      switchMode();                            //{ // CLOCKMODE_STANDALONE_A, CLOCKMODE_STANDALONE_B, ... new mode in iClockMode and EEPROM
      processModeSwitch();                     //
      iUpdateDisplayMode = DISPLAYUPDATE_ALL;  //we can set this everywhere throughout the code.
    }
  } else if ((muxValue[ENCODERCLICK] == 1) && (muxValue[STOPBUTTON] == 1)) {
    if (iConfigChangeMode == CONFIGCHANGE_NONE) {
      iConfigChangeMode = CONFIGCHANGE_QRS;
      iUpdateDisplayMode = DISPLAYUPDATE_ALL;
    }
  }

  if (iConfigChangeMode != CONFIGCHANGE_NONE) {
    if ((muxValue[ENCODERCLICK] == 0) && (muxValue[STARTBUTTON] == 0) && (muxValue[STOPBUTTON] == 0)) {
      if (iConfigChangeMode == CONFIGCHANGE_QRS) {
        if (getQRSOffsetFromEeprom() != iQuantizeRestartOffset) {
          EEPROM.update(MEMLOC_QRSOFFSET, byte(iQuantizeRestartOffset));
          ledRed();
          delay(500);
          ledOff();
        }
      }
      iConfigChangeMode = CONFIGCHANGE_NONE;
      iUpdateDisplayMode = DISPLAYUPDATE_ALL;
    }
  }

  // regularly check for BPM updates
  if ((iClockMode == CLOCKMODE_STANDALONE_A) || (iClockMode == CLOCKMODE_STANDALONE_B)) {
    if (abs(tapTempo.getBPM() - fBPM_Cache) >= 1.0) {  // neue BPM
      fBPM_Cache = uint8_t(tapTempo.getBPM());         // Nur ganzzahlige Werte darstellen, Rundungsfehler ueberdecken
      uClock.setTempo(fBPM_Cache);
      iUpdateDisplayMode = DISPLAYUPDATE_ALL;
    }
  } else if (iClockMode == CLOCKMODE_FOLLOW_24PPQN_DIN) {
    if (abs(tapTempo.getBPM() - fBPM_Cache) >= 1.0) {  // neue BPM
      fBPM_Cache = uint8_t(tapTempo.getBPM());
      //uClock.setTempo( fBPM_Cache );  //Clock is deactivated in FOLLO_24PPQN-Modes
    }
    checkMidiDIN();
  } else if (iClockMode == CLOCKMODE_FOLLOW_24PPQN_USB) {
    if (abs(tapTempo.getBPM() - fBPM_Cache) >= 1.0) {  // neue BPM
      fBPM_Cache = uint8_t(tapTempo.getBPM());
      //uClock.setTempo( fBPM_Cache );
    }
    checkMidiUSB();
  } else if (iClockMode == CLOCKMODE_FOLLOW_STARTSTOP_DIN) {
    if (abs(tapTempo.getBPM() - fBPM_Cache) >= 1.0) {  // neue BPM
      fBPM_Cache = uint8_t(tapTempo.getBPM());
      uClock.setTempo(fBPM_Cache);
      iUpdateDisplayMode = DISPLAYUPDATE_ALL;
    }
    checkMidiDIN();
  } else if (iClockMode == CLOCKMODE_FOLLOW_STARTSTOP_USB) {
    if (abs(tapTempo.getBPM() - fBPM_Cache) >= 1.0) {  // neue BPM
      fBPM_Cache = uint8_t(tapTempo.getBPM());
      uClock.setTempo(fBPM_Cache);
      iUpdateDisplayMode = DISPLAYUPDATE_ALL;
    }
    checkMidiUSB();
  }


  int iEncoder = queryEncoder();

  if (iEncoder != 0) {
    if (iConfigChangeMode == CONFIGCHANGE_NONE) {
      if ((iClockMode == CLOCKMODE_STANDALONE_A) || (iClockMode == CLOCKMODE_STANDALONE_B) || (iClockMode == CLOCKMODE_FOLLOW_STARTSTOP_DIN) || (iClockMode == CLOCKMODE_FOLLOW_STARTSTOP_USB)) {
        if (muxValue[ENCODERCLICK] == 0) {
          // Encoder drehen
          fBPM_Cache += iEncoder;
        } else {
          // Encoder drücken und drehen
          fBPM_Cache += iEncoder * 5;
        }
        iUpdateDisplayMode = DISPLAYUPDATE_ALL;
        setGlobalBPM(fBPM_Cache);
      }
    } else if (iConfigChangeMode == CONFIGCHANGE_QRS) {
      if (iEncoder < 0) {
        if (iQuantizeRestartOffset > 1) {
          iQuantizeRestartOffset += iEncoder;
          iUpdateDisplayMode = DISPLAYUPDATE_ALL;
        }
      } else if (iEncoder > 0) {
        if (iQuantizeRestartOffset < INTERNAL_PPQN - 0) {
          iQuantizeRestartOffset += iEncoder;
          iUpdateDisplayMode = DISPLAYUPDATE_ALL;
        }
      }
    }
  }

  if (iNextPreset != NEXTPRESET_NONE) {
    selectPreset(iNextPreset);
    iNextPreset = NEXTPRESET_NONE;
  }

  if(iBeatCounter != iBeatCounter_old){
    nextBeat();
    iBeatCounter_old = iBeatCounter;
  }


  //We only update the display max once per loop to save on resources
  if (iUpdateDisplayMode != DISPLAYUPDATE_NONE) {
    if (iUpdateDisplayMode == DISPLAYUPDATE_ALL) {
      bStaticContentDrawnOnce = false;
      updateDisplay(true, false, false);
    } else if (iUpdateDisplayMode == DISPLAYUPDATE_BLINKER_ON) {
      updateDisplay(false, true, false);
    } else if (iUpdateDisplayMode == DISPLAYUPDATE_BLINKER_OFF) {
      updateDisplay(false, false, false);
    } else if (iUpdateDisplayMode == DISPLAYUPDATE_BPM) {
      updateDisplay(false, false, true);
    }

    iUpdateDisplayMode = DISPLAYUPDATE_NONE;
  }
}

// every quarter note. Keep resources low over here because it might silently slow down the clock
void nextBeat(){
  //Tempo-ease
  if(fTargetBPM != TARGET_BPM_NONE){
    if(abs(uClock.getTempo() - fTargetBPM)>3.){
      setGlobalBPM( uClock.getTempo()  + fBPM_Diff/16.);
    }else{
      setGlobalBPM( fTargetBPM );
      fTargetBPM = TARGET_BPM_NONE;
    }
  }
}

// Standalone, follow ...
void switchMode() {
  for (int i = 0; i < MODECOUNT; i++) {
    if (iClockMode == arrModes[i]) {
      iClockMode = arrModes[(i + 1) % MODECOUNT];
      iTickCounter = 0;
      EEPROM.update(MEMLOC_MODE, byte(iClockMode));
      break;
    }
  }
}


void processModeSwitch() {
  if ((iClockMode == CLOCKMODE_STANDALONE_A) || (iClockMode == CLOCKMODE_STANDALONE_B)) {
    if (iClockBehaviour == SENDCLOCK_ALWAYS) {
      uClock.start();
    }

  } else if (iClockMode == CLOCKMODE_FOLLOW_24PPQN_DIN) {
    stopPlaying(true);
    uClock.stop();
    iUpdateDisplayMode = DISPLAYUPDATE_ALL;

  } else if (iClockMode == CLOCKMODE_FOLLOW_24PPQN_USB) {
    stopPlaying(true);
    uClock.stop();
    iUpdateDisplayMode = DISPLAYUPDATE_ALL;

  } else if ((iClockMode == CLOCKMODE_FOLLOW_STARTSTOP_DIN) || (iClockMode == CLOCKMODE_FOLLOW_STARTSTOP_USB)) {
    if (iClockBehaviour == SENDCLOCK_ALWAYS) {
      uClock.start();
    }
  }
}

void checkMidiUSB() {
  if (iConfigChangeMode != CONFIGCHANGE_NONE) {
    return;
  }
  midiPacket = MidiUSB.read();
  if (midiPacket.header != 0) {
    if ((midiPacket.header & 0x0F) != 0x0F) {
      return;
    }
  } else {
    return;
  }
  if (iClockMode == CLOCKMODE_FOLLOW_24PPQN_USB) {
    // no Processing at first. just passthru of incoming clock data to make everything hyper-tight
    if ((midiPacket.byte1 == MIDI_CLOCK) || (midiPacket.byte1 == MIDI_START) || (midiPacket.byte1 == MIDI_STOP)) {
      Serial1.write(midiPacket.byte1);
    }

    if (midiPacket.byte1 == MIDI_START) {
      bIsPlaying = true;
      iTickCounter = 0;
      return;
    }

    if (midiPacket.byte1 == MIDI_STOP) {
      bIsPlaying = false;
      return;
    }

    if (midiPacket.byte1 == MIDI_CLOCK) {

      if ((iTickCounter == 0) || (iTickCounter == 24) || (iTickCounter == 48) || (iTickCounter == 72)) {
        iUpdateDisplayMode = DISPLAYUPDATE_BLINKER_ON;
      }

      if ((iTickCounter == 8) || (iTickCounter == 32) || (iTickCounter == 56) || (iTickCounter == 80)) {
        iUpdateDisplayMode = DISPLAYUPDATE_BLINKER_OFF;
      }

      if (iTickCounter > INTERNAL_PPQN*4 -2) {
        iTickCounter = 0;
      } else {
        iTickCounter++;
      }
      handleLED(iTickCounter);
    }

  } else if (iClockMode == CLOCKMODE_FOLLOW_STARTSTOP_USB) {

    if (midiPacket.byte1 == MIDI_START) {
      startPlaying(true);
      return;
    }

    if (midiPacket.byte1 == MIDI_STOP) {
      stopPlaying(true);
      return;
    }
  }
}


// uClock does NOT run in this mode. Clock data are passed thru, LED pattern is handled by counting and processing impulses.
// no tapTempo.update() since we do no show the BPM because processing and displaying potentially costs already sparse resources.
void checkMidiDIN() {
  if (iConfigChangeMode != CONFIGCHANGE_NONE) {
    return;
  }

  if (iClockMode == CLOCKMODE_FOLLOW_24PPQN_DIN) {
    if (Serial1.available() > 0) {
      midiPacket.byte1 = Serial1.read();
      //Serial1.flush();
    } else {
      return;
    }
    // no Processing at first. just passthru of incoming clock data to make everything hyper-tight
    if ((midiPacket.byte1 == MIDI_CLOCK) || (midiPacket.byte1 == MIDI_START) || (midiPacket.byte1 == MIDI_STOP)) {
      midiEventPacket_t p = { 0x0F, midiPacket.byte1, 0, 0 };
      MidiUSB.sendMIDI(p);
      MidiUSB.flush();
      Serial1.write(midiPacket.byte1);
    }

    if (midiPacket.byte1 == MIDI_START) {
      bIsPlaying = true;
      iTickCounter = 0;
      return;
    }

    if (midiPacket.byte1 == MIDI_STOP) {
      bIsPlaying = false;
      return;
    }

   

    if (midiPacket.byte1 == MIDI_CLOCK) {

      if ((iTickCounter == 0) || (iTickCounter == 24) || (iTickCounter == 48) || (iTickCounter == 72)) {
        iUpdateDisplayMode = DISPLAYUPDATE_BLINKER_ON;
      }

      if ((iTickCounter == 8) || (iTickCounter == 32) || (iTickCounter == 56) || (iTickCounter == 80)) {
        iUpdateDisplayMode = DISPLAYUPDATE_BLINKER_OFF;
      }

      if (iTickCounter > INTERNAL_PPQN*4 -2) {
        iTickCounter = 0;
      } else {
        iTickCounter++;
      }
      handleLED(iTickCounter);
    }


  } else if (iClockMode == CLOCKMODE_FOLLOW_STARTSTOP_DIN) {
    if (Serial1.available() > 0) {
      midiPacket.byte1 = Serial1.read();
      //Serial1.flush();
    } else {
      return;
    }
    if (midiPacket.byte1 == MIDI_START) {
      // Do not actively send MIDI_START since this is already passed thru, just start the internal clock for displaying LED
      // This will also reset the clock to 0 if already running
      startPlaying(true);
      return;
    }

    if (midiPacket.byte1 == MIDI_STOP) {
      // Do not actively send MIDI_START since this is already passed thru, just start the internal clock for displaying LED
      // This will also reset the clock to 0 if already running
      stopPlaying(true);
      return;
    }
  }
}

void selectPreset(uint8_t pPresetID) {
  if( !bIsPlaying ){
    if (pPresetID == NEXTPRESET_1) {
      fBPM_Cache = uint8_t(fBPM_Preset1);
      setGlobalBPM(fBPM_Preset1);
    } else if (pPresetID == NEXTPRESET_2) {
      fBPM_Cache = uint8_t(fBPM_Preset2);
      setGlobalBPM(fBPM_Preset2);
    } else if (pPresetID == NEXTPRESET_3) {
      fBPM_Cache = uint8_t(fBPM_Preset3);
      setGlobalBPM(fBPM_Preset3);
    }
    iUpdateDisplayMode = DISPLAYUPDATE_ALL;
  }else{
    // Tempo-ease, Tempo-fade
    if (pPresetID == NEXTPRESET_1) {
      fTargetBPM = fBPM_Preset1;
    } else if (pPresetID == NEXTPRESET_2) {
      fTargetBPM = fBPM_Preset2;
    } else if (pPresetID == NEXTPRESET_3) {
      fTargetBPM = fBPM_Preset3;
    }
    fBPM_Diff = fTargetBPM - uClock.getTempo();
    iBeatCounter = 0;
  }
}


void readMux() {
  // loop through channels 0 - 15
  for (byte i = 0; i < 16; i++) {
    mux.channel(i);
    int val = digitalRead(DEMUX_PIN);  // Read analog value
    muxValue[i] = val;
  }
}

// The callback function wich will be called by Clock each Pulse of clock resolution.
void ClockOutPPQN(uint32_t tick) {
  // Send MIDI_CLOCK to external gears in standalone mode. Other modes to a passthru of incoming data and
  // only need the clock for displaying stuff
  if ((iClockMode == CLOCKMODE_STANDALONE_A) || (iClockMode == CLOCKMODE_STANDALONE_B) || (iClockMode == CLOCKMODE_FOLLOW_STARTSTOP_DIN) || (iClockMode == CLOCKMODE_FOLLOW_STARTSTOP_USB)) {
    sendMidiClock();
  }
  handleClockTick(tick);
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

uint8_t countTick(uint8_t pMultiplier) {
  if (iTickCounter >= INTERNAL_PPQN * pMultiplier - 1) {
    iTickCounter = 0;
  } else {
    iTickCounter++;
  }
  return iTickCounter;
}

void sendMidiClock() {
  midiEventPacket_t p = { 0x0F, MIDI_CLOCK, 0, 0 };
  if ((iClockMode == CLOCKMODE_STANDALONE_A) || (iClockMode == CLOCKMODE_STANDALONE_B)) {
    MidiUSB.sendMIDI(p);
    MidiUSB.flush();
    Serial1.write(MIDI_CLOCK);
  } else if (iClockMode == CLOCKMODE_FOLLOW_24PPQN_USB) {
    Serial1.write(MIDI_CLOCK);
  } else if (iClockMode == CLOCKMODE_FOLLOW_STARTSTOP_DIN) {
    MidiUSB.sendMIDI(p);
    MidiUSB.flush();
    Serial1.write(MIDI_CLOCK);
  } else if (iClockMode == CLOCKMODE_FOLLOW_STARTSTOP_USB) {
    Serial1.write(MIDI_CLOCK);
  }
}

void sendMidiStart() {
  midiEventPacket_t p = { 0x0F, MIDI_START, 0, 0 };
  if ((iClockMode == CLOCKMODE_STANDALONE_A) || (iClockMode == CLOCKMODE_STANDALONE_B)) {
    MidiUSB.sendMIDI(p);
    MidiUSB.flush();
    Serial1.write(MIDI_START);
  } else if (iClockMode == CLOCKMODE_FOLLOW_24PPQN_DIN) {
    MidiUSB.sendMIDI(p);
    MidiUSB.flush();
    Serial1.write(MIDI_START);
  } else if (iClockMode == CLOCKMODE_FOLLOW_24PPQN_USB) {
    Serial1.write(MIDI_START);
  } else if (iClockMode == CLOCKMODE_FOLLOW_STARTSTOP_DIN) {
    MidiUSB.sendMIDI(p);
    MidiUSB.flush();
    Serial1.write(MIDI_START);
  } else if (iClockMode == CLOCKMODE_FOLLOW_STARTSTOP_USB) {
    Serial1.write(MIDI_START);
  }
}

void sendMidiStop() {
  midiEventPacket_t p = { 0x0F, MIDI_STOP, 0, 0 };
  if ((iClockMode == CLOCKMODE_STANDALONE_A) || (iClockMode == CLOCKMODE_STANDALONE_B)) {
    MidiUSB.sendMIDI(p);
    MidiUSB.flush();
    Serial1.write(MIDI_STOP);
  } else if (iClockMode == CLOCKMODE_FOLLOW_24PPQN_DIN) {
    MidiUSB.sendMIDI(p);
    MidiUSB.flush();
    Serial1.write(MIDI_STOP);
  } else if (iClockMode == CLOCKMODE_FOLLOW_24PPQN_USB) {
    Serial1.write(MIDI_STOP);
  } else if (iClockMode == CLOCKMODE_FOLLOW_STARTSTOP_DIN) {
    MidiUSB.sendMIDI(p);
    MidiUSB.flush();
    Serial1.write(MIDI_STOP);
  } else if (iClockMode == CLOCKMODE_FOLLOW_STARTSTOP_USB) {
    Serial1.write(MIDI_STOP);
  }
}

void handleClockTick(uint32_t tick) {
  if (bQuantizeRestartWaiting == true) {
    if ((tick % (INTERNAL_PPQN * 4) == (INTERNAL_PPQN*4 - iQuantizeRestartOffset))) {
      if (iClockMode == CLOCKMODE_STANDALONE_B) {
        sendMidiStop();
      }
    }

    //send midi Start on the one
    if (tick % (INTERNAL_PPQN * 4)==0){
      if ((iClockMode == CLOCKMODE_STANDALONE_A) || (iClockMode == CLOCKMODE_STANDALONE_B) || (iClockMode == CLOCKMODE_FOLLOW_STARTSTOP_DIN) || (iClockMode == CLOCKMODE_FOLLOW_STARTSTOP_USB)) {
        sendMidiStart();
        bQuantizeRestartWaiting = false;
      }
    }
    
  }

  //CV Gate out
  if (!(tick % 6)) {
    digitalWrite(SYNC_TX_PIN, true);
    if(iBeatCounter<200){
      iBeatCounter++;
    }else{
      iBeatCounter=0;
    }
  }

  if (!((tick - 1) % 6)) {
    digitalWrite(SYNC_TX_PIN, false);
  }

  handleLED(tick);
}

void handleLED(uint32_t tick){
  // at the beginning or on every downbeat
  if ((tick == 1) || (!(tick % (INTERNAL_PPQN * 4)))) {
    bpm_blink_timer = 12;
    iMeasureCount = 0;
    ledIndicateMeasure(iMeasureCount);
    iUpdateDisplayMode = DISPLAYUPDATE_BLINKER_ON;
    iMeasureCount++;

  } else if (!(tick % (INTERNAL_PPQN))) {  // each quarter note //led on
    bpm_blink_timer = 8;
    ledIndicateMeasure(iMeasureCount);
    iUpdateDisplayMode = DISPLAYUPDATE_BLINKER_ON;
    iMeasureCount++;

  } else if (!(tick % bpm_blink_timer)) {  // get led off
    if (!bQuantizeRestartWaiting) {
      ledOff();
      iUpdateDisplayMode = DISPLAYUPDATE_BLINKER_OFF;
    }
  }
}


void tapHandler(Button2& btn) {
  if ((iClockMode == CLOCKMODE_STANDALONE_A) || (iClockMode == CLOCKMODE_STANDALONE_B) || (iClockMode == CLOCKMODE_FOLLOW_STARTSTOP_DIN) || (iClockMode == CLOCKMODE_FOLLOW_STARTSTOP_USB)) {
    tapTempo.update(true);
  }
}

byte tapButtonStateHandler() {
  return muxValue[TAPBUTTON] == 0 ? false : true;
}


void startHandler(Button2& btn) {
  if (muxValue[ENCODERCLICK] == 0) {
    if ((iClockMode == CLOCKMODE_STANDALONE_A) || (iClockMode == CLOCKMODE_STANDALONE_B) || (iClockMode == CLOCKMODE_FOLLOW_STARTSTOP_DIN) || (iClockMode == CLOCKMODE_FOLLOW_STARTSTOP_USB)) {
      startPlaying(true);
    }
  }
}

void restartHandler(Button2& btn) {
}

void startPlaying(bool pSendMidi) {

  if ((!bIsPlaying) && (iConfigChangeMode == CONFIGCHANGE_NONE)) {
    if (pSendMidi) {
      sendMidiStart();
    }
    uClock.start();  //if already running this causes a clock reset (-> LED handling, tick,)
  } else {
    bQuantizeRestartWaiting = true;
  }
  bIsPlaying = true;
}

byte startButtonStateHandler() {
  return muxValue[STARTBUTTON] == 0 ? false : true;
}


void stopHandler(Button2& btn) {
  if (muxValue[ENCODERCLICK] == 0) {
    if ((iClockMode == CLOCKMODE_STANDALONE_A) || (iClockMode == CLOCKMODE_STANDALONE_B) || (iClockMode == CLOCKMODE_FOLLOW_STARTSTOP_DIN) || (iClockMode == CLOCKMODE_FOLLOW_STARTSTOP_USB)) {
      stopPlaying(true);
    }
  }
}

void stopPlaying(bool pSendMidi) {
  bIsPlaying = false;
  bQuantizeRestartWaiting = false;
  if (pSendMidi) {
    sendMidiStop();
  }
  ledOff();
  if (iClockBehaviour == SENDCLOCK_WHENPLAYING) {
    uClock.stop();
  }
}

byte stopButtonStateHandler() {
  return muxValue[STOPBUTTON] == 0 ? false : true;
}


//Button or Footswitch
byte preset1ButtonStateHandler() {
  bool b = muxValue[PRESETBUTTON1] || muxValue[PRESETSWITCH1];
  //return muxValue[3] == 0 ? false : true;
  return b == 0 ? false : true;
}

void preset1ClickHandler(Button2& btn) {
  if ((iClockMode == CLOCKMODE_STANDALONE_A) || (iClockMode == CLOCKMODE_STANDALONE_B) || (iClockMode == CLOCKMODE_FOLLOW_STARTSTOP_DIN) || (iClockMode == CLOCKMODE_FOLLOW_STARTSTOP_USB)) {
    if (muxValue[ENCODERCLICK] == 1) {
      fBPM_Preset1 = tapTempo.getBPM();
      if (EEPROM.read(MEMLOC_PRESET_1) != byte(fBPM_Preset1)) {
        EEPROM.update(MEMLOC_PRESET_1, byte(fBPM_Preset1));
        ledRed();
        if (!bIsPlaying) {
          delay(500);
          ledOff();
        }
      }
    } else {
      iNextPreset = NEXTPRESET_1;
    }
  }
}



byte preset2ButtonStateHandler() {
  bool b = muxValue[PRESETBUTTON2] || muxValue[PRESETSWITCH2];
  return b == 0 ? false : true;
}


void preset2ClickHandler(Button2& btn) {
  if ((iClockMode == CLOCKMODE_STANDALONE_A) || (iClockMode == CLOCKMODE_STANDALONE_B) || (iClockMode == CLOCKMODE_FOLLOW_STARTSTOP_DIN) || (iClockMode == CLOCKMODE_FOLLOW_STARTSTOP_USB)) {
    if (muxValue[ENCODERCLICK] == 1) {
      fBPM_Preset2 = tapTempo.getBPM();
      if (EEPROM.read(MEMLOC_PRESET_2) != byte(fBPM_Preset2)) {
        EEPROM.update(MEMLOC_PRESET_2, byte(fBPM_Preset2));
        ledRed();
        if (!bIsPlaying) {
          delay(500);
          ledOff();
        }
      }
    } else {
      iNextPreset = NEXTPRESET_2;
    }
  }
}


byte preset3ButtonStateHandler() {
  bool b = muxValue[PRESETBUTTON3] || muxValue[PRESETSWITCH3];
  return b == 0 ? false : true;
}

void preset3ClickHandler(Button2& btn) {
  if ((iClockMode == CLOCKMODE_STANDALONE_A) || (iClockMode == CLOCKMODE_STANDALONE_B) || (iClockMode == CLOCKMODE_FOLLOW_STARTSTOP_DIN)) {
    if (muxValue[ENCODERCLICK] == 1) {
      fBPM_Preset3 = tapTempo.getBPM();
      if (EEPROM.read(MEMLOC_PRESET_3) != byte(fBPM_Preset3)) {
        EEPROM.update(MEMLOC_PRESET_3, byte(fBPM_Preset3));
        ledRed();
        if (!bIsPlaying) {
          delay(500);
          ledOff();
        }
      }
    } else {
      iNextPreset = NEXTPRESET_3;
    }
  }
}


void setGlobalBPM(float f) {
  if ((iClockMode == CLOCKMODE_STANDALONE_A) || (iClockMode == CLOCKMODE_STANDALONE_B)) {
    tapTempo.setBPM(f);
  } else if ((iClockMode == CLOCKMODE_FOLLOW_24PPQN_DIN) || (iClockMode == CLOCKMODE_FOLLOW_24PPQN_USB)) {
    //tapTempo.setBPM(f);
  } else if ((iClockMode == CLOCKMODE_FOLLOW_STARTSTOP_DIN) || (iClockMode == CLOCKMODE_FOLLOW_STARTSTOP_USB)) {
    tapTempo.setBPM(f);
  }
  uClock.setTempo(f);
}

void ledIndicateMeasure(int pMeasure) {
  if (bIsPlaying) {
    if (bQuantizeRestartWaiting) {
      pixels.setPixelColor(pMeasure, pixels.Color(LED_ON, LED_OFF, LED_ON));
    } else {
      if (pMeasure == 0) {
        ledGreen();
      } else {
        pixels.clear();
        pixels.setPixelColor(pMeasure, pixels.Color(LED_OFF, LED_OFF, LED_ON));
      }
    }
    pixels.show();
  }
}

void ledGreen() {
  pixels.clear();
  for (int i = 0; i < NUM_LEDS; i++) {
    pixels.setPixelColor(i, pixels.Color(LED_OFF, LED_ON, LED_OFF));
  }
  pixels.show();
}

void ledOra() {
  pixels.clear();
  for (int i = 0; i < NUM_LEDS; i++) {
    pixels.setPixelColor(i, pixels.Color(7, 4, LED_OFF));
  }
  pixels.show();
}

void ledOra(uint8_t pPixel) {
  pixels.clear();
  for (uint8_t i = 0; i < pPixel; i++) {
    pixels.setPixelColor(i, pixels.Color(7, 4, LED_OFF));
  }
  pixels.show();
}

void ledOff() {
  pixels.clear();
  pixels.show();
}

void ledRed() {
  pixels.clear();
  for (int i = 0; i < NUM_LEDS; i++) {
    pixels.setPixelColor(i, pixels.Color(LED_ON, LED_OFF, LED_OFF));
  }
  pixels.show();
}

void ledPurple() {
  pixels.clear();
  for (int i = 0; i < NUM_LEDS; i++) {
    pixels.setPixelColor(i, pixels.Color(LED_ON, LED_OFF, LED_ON));
  }
  pixels.show();
}

void ledPurple(uint8_t pVal) {
  pixels.clear();
  //for (int i = 0; i < NUM_LEDS; i++) {
  pixels.setPixelColor(0, pixels.Color(LED_ON * (pVal / INTERNAL_PPQN) / 4, LED_OFF, LED_ON * (pVal / INTERNAL_PPQN) / 4));
  //}
  pixels.show();
}

void ledInit() {
  pixels.clear();
  for (int i = 0; i < NUM_LEDS; i++) {
    pixels.setPixelColor(i, pixels.Color(LED_OFF, LED_ON, LED_ON));
  }
  pixels.show();
}

void getPresetsFromEeprom() {
  uint8_t val;
  val = EEPROM.read(MEMLOC_PRESET_1);
  if (val != 255) {  // a.k.a. hier wurde schonmal etwas gespeichert
    fBPM_Preset1 = val;
  } else {
    fBPM_Preset1 = 80;
  }

  val = EEPROM.read(MEMLOC_PRESET_2);
  if (val != 255) {
    fBPM_Preset2 = val;
  }

  val = EEPROM.read(MEMLOC_PRESET_3);
  if (val != 255) {
    fBPM_Preset3 = val;
  }
}

uint8_t getModeFromEeprom() {
  uint8_t val;
  val = EEPROM.read(MEMLOC_MODE);
  if (val != 255) {  // a.k.a. hier wurde schonmal etwas gespeichert
    return val;
  } else {
    return CLOCKMODE_STANDALONE_A;
  }
}

uint8_t getQRSOffsetFromEeprom() {
  uint8_t val;
  val = EEPROM.read(MEMLOC_QRSOFFSET);
  if (val != 255) {  // a.k.a. hier wurde schonmal etwas gespeichert
    return val;
  } else {
    return 1;
  }
}

// Returns -1 / +1
int queryEncoder() {
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

  if (encoder0Pos != encoder0PosOld) {
    if (encoder0Pos % 2 == 0) {
      if (encoder0Pos < encoder0PosOld) {
        iReturn = -1;
      } else {
        iReturn = 1;
      }
    }
    encoder0PosOld = encoder0Pos;
  }
  encoder0PinALast = muxValue[ENCODERPINA];
  return iReturn;
}

void clearDisplay() {
#ifdef DISPLAY_128x64
  i2cDisplay.clear();
  bStaticContentDrawnOnce = false;
#endif

}

void showInfo(int pWaitMS) {
#ifdef DISPLAY_128x64
  showInfo_128x64(pWaitMS);
#endif

}


void showUpdateInfo() {
#ifdef DISPLAY_128x64
  showUpdateInfo_128x64();
#endif

}


void nudgePlus(bool pOnOff) {
  if (pOnOff) {
    //if((iClockMode == CLOCKMODE_FOLLOW_24PPQN)||(iClockMode == CLOCKMODE_FOLLOW_48PPQN)||(iClockMode == CLOCKMODE_FOLLOW_72PPQN)||(iClockMode == CLOCKMODE_FOLLOW_96PPQN)){
    fBPM_Sysex = fBPM_Cache;
    //}
    bNudgeActive = true;
    fBPM_Cache *= 1.075;
    setGlobalBPM(fBPM_Cache);

    //}
  } else {
    bNudgeActive = false;
    fBPM_Cache = fBPM_Sysex;
    setGlobalBPM(fBPM_Cache);
  }
}

void nudgeMinus(bool pOnOff) {
  if (pOnOff) {
    //fBPM_NudgeCache = tapTempo.getBPM(); //uClock.getTempo();
    bNudgeActive = true;
    fBPM_Sysex = fBPM_Cache;
    fBPM_Cache *= 0.925;
    setGlobalBPM(fBPM_Cache);
  } else {
    fBPM_Cache = fBPM_Sysex;
    setGlobalBPM(fBPM_Cache);
    bNudgeActive = false;
  }
}


void showModeResetInfo(int pMS) {
#ifdef DISPLAY_128x64
  showModeResetInfo_128x64(pMS);
#endif

}


#ifdef DISPLAY_128x64

void showInfo_128x64(int pWaitMS) {
  i2cDisplay.setFont(ZevvPeep8x16);
  i2cDisplay.clear();
  i2cDisplay.set2X();
  i2cDisplay.println("ClockBox");
  i2cDisplay.set1X();
  i2cDisplay.print("  Version ");
  i2cDisplay.println(VERSION);
  i2cDisplay.println();
  i2cDisplay.println(" Andyland.info");
  delay(pWaitMS);
}

void updateDisplay_128x64(bool pClearAll, bool pBLinkerOnOff, bool pClearBPM) {
  if (pClearAll == true) {
    clearDisplay();
  }
  if ((iConfigChangeMode == CONFIGCHANGE_NONE) || (iConfigChangeMode == CONFIGCHANGE_MODE)) {
    if (iClockMode == CLOCKMODE_STANDALONE_A) {
      // BPM
      i2cDisplay.setInvertMode(false);
      i2cDisplay.setFont(Verdana_digits_24);
      i2cDisplay.set2X();
      i2cDisplay.setRow(0);
      if (fBPM_Cache < 100) {
        i2cDisplay.setCol(30);
      } else {
        i2cDisplay.setCol(15);
      }
      i2cDisplay.print(String(fBPM_Cache));

      i2cDisplay.setFont(ZevvPeep8x16);
      i2cDisplay.set1X();
      i2cDisplay.setCol(1);
      i2cDisplay.setRow(6);
      i2cDisplay.print("QRS Start");
      i2cDisplay.setCol(112);
      i2cDisplay.setInvertMode(pBLinkerOnOff);
      i2cDisplay.setRow(0);
      i2cDisplay.print("  ");
    } else if (iClockMode == CLOCKMODE_STANDALONE_B) {
      // BPM
      i2cDisplay.setInvertMode(false);
      i2cDisplay.setFont(Verdana_digits_24);
      i2cDisplay.set2X();
      i2cDisplay.setRow(0);
      if (fBPM_Cache < 100) {
        i2cDisplay.setCol(30);
      } else {
        i2cDisplay.setCol(15);
      }
      i2cDisplay.print(String(fBPM_Cache));

      i2cDisplay.setFont(ZevvPeep8x16);
      i2cDisplay.set1X();
      i2cDisplay.setCol(1);
      i2cDisplay.setRow(6);
      i2cDisplay.print("QRS Stop Start");
      i2cDisplay.setCol(112);
      i2cDisplay.setInvertMode(pBLinkerOnOff);
      i2cDisplay.setRow(0);
      i2cDisplay.print("  ");
    } else if (iClockMode == CLOCKMODE_FOLLOW_24PPQN_DIN) {
      // BPM wird nicht gezeigt, kostet zuviel Resourcen
      if (bStaticContentDrawnOnce == false) {
        // das hier muss nur einmal gezeichnet werden
        i2cDisplay.setInvertMode(false);
        i2cDisplay.setFont(ZevvPeep8x16);
        i2cDisplay.set1X();
        i2cDisplay.setCol(1);
        i2cDisplay.setRow(1);
        i2cDisplay.print("Follow Clock");
        i2cDisplay.setCol(1);
        i2cDisplay.setRow(3);
        i2cDisplay.print("from DIN Midi");
        i2cDisplay.setCol(1);
        i2cDisplay.setRow(6);
        i2cDisplay.print("ext.Clk DIN 24");
        bStaticContentDrawnOnce = true;
      }
      i2cDisplay.setRow(0);
      i2cDisplay.setCol(112);
      i2cDisplay.setInvertMode(pBLinkerOnOff);
      i2cDisplay.print("  ");

    } else if (iClockMode == CLOCKMODE_FOLLOW_24PPQN_USB) {
      if (bStaticContentDrawnOnce == false) {
        // das hier muss nur einmal gezeichnet werden
        i2cDisplay.setInvertMode(false);
        i2cDisplay.setFont(ZevvPeep8x16);
        i2cDisplay.set1X();
        i2cDisplay.setCol(1);
        i2cDisplay.setRow(1);
        i2cDisplay.print("Follow Clock");
        i2cDisplay.setCol(1);
        i2cDisplay.setRow(3);
        i2cDisplay.print("from USB Midi");
        i2cDisplay.setCol(1);
        i2cDisplay.setRow(6);
        i2cDisplay.print("ext.Clk USB 24");
        bStaticContentDrawnOnce = true;
      }
      i2cDisplay.setRow(0);
      i2cDisplay.setCol(112);
      i2cDisplay.setInvertMode(pBLinkerOnOff);
      i2cDisplay.print("  ");
    } else if (iClockMode == CLOCKMODE_FOLLOW_STARTSTOP_DIN) {
      // BPM
      i2cDisplay.setInvertMode(false);
      i2cDisplay.setFont(Verdana_digits_24);
      i2cDisplay.set2X();
      i2cDisplay.setRow(0);
      if (fBPM_Cache < 100) {
        i2cDisplay.setCol(30);
      } else {
        i2cDisplay.setCol(15);
      }
      i2cDisplay.print(String(fBPM_Cache));

      i2cDisplay.setFont(ZevvPeep8x16);
      i2cDisplay.set1X();
      i2cDisplay.setCol(1);
      i2cDisplay.setRow(6);
      i2cDisplay.print("StartStop DIN");
      i2cDisplay.setCol(112);
      i2cDisplay.setInvertMode(pBLinkerOnOff);
      i2cDisplay.setRow(0);
      i2cDisplay.print("  ");
    } else if (iClockMode == CLOCKMODE_FOLLOW_STARTSTOP_USB) {
      // BPM
      i2cDisplay.setInvertMode(false);
      i2cDisplay.setFont(Verdana_digits_24);
      i2cDisplay.set2X();
      i2cDisplay.setRow(0);
      if (fBPM_Cache < 100) {
        i2cDisplay.setCol(30);
      } else {
        i2cDisplay.setCol(15);
      }
      i2cDisplay.print(String(fBPM_Cache));
      i2cDisplay.setInvertMode(false);
      i2cDisplay.setFont(ZevvPeep8x16);
      i2cDisplay.set1X();
      i2cDisplay.setCol(1);
      i2cDisplay.setRow(6);
      i2cDisplay.print("StartStop USB");
      i2cDisplay.setCol(112);
      i2cDisplay.setInvertMode(pBLinkerOnOff);
      i2cDisplay.setRow(0);
      i2cDisplay.print("  ");
    }
  } else if (iConfigChangeMode == CONFIGCHANGE_QRS) {
    if (bStaticContentDrawnOnce == false) {
      i2cDisplay.setFont(ZevvPeep8x16);
      i2cDisplay.setInvertMode(false);
      i2cDisplay.set1X();
      i2cDisplay.setRow(0);
      i2cDisplay.print("QRS Offset(PPQN):");
      i2cDisplay.setRow(3);
      i2cDisplay.set2X();
      bStaticContentDrawnOnce = true;
    }
    
    if (iQuantizeRestartOffset < 100) {
      i2cDisplay.setCol(30);
    } else {
      i2cDisplay.setCol(15);
    }
    i2cDisplay.print(String(iQuantizeRestartOffset));
  }
}



void showUpdateInfo_128x64() {
  i2cDisplay.clear();
  i2cDisplay.setFont(ZevvPeep8x16);
  i2cDisplay.set2X();
  i2cDisplay.println("ClockBox");
  i2cDisplay.set1X();
  i2cDisplay.println();
  i2cDisplay.println();
  i2cDisplay.println("   UpdateMode");
}

void showModeResetInfo_128x64(int pMS) {
  i2cDisplay.setFont(ZevvPeep8x16);
  i2cDisplay.clear();
  i2cDisplay.set2X();
  i2cDisplay.println("ClockBox");
  i2cDisplay.set1X();
  i2cDisplay.println();
  i2cDisplay.println("ClockMode reset");
  delay(pMS);
}
#endif
