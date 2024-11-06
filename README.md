# Arduino_ClockBox_v3<br>
<br>
Various revisions of hardware are reflected in different branches to accomodate for different
pinlayouts etc.

## Operational Modes  
The ClockBox v3 supports different modes of operation. The selected mode is shown in the lower half of the Display. The mode can be selected by pressing down the rotary encoder and hitting the green ("start") button. The selected mode is saved automatically and will be restored at next power-on.  
### Mode: QRS Start ("QuantizedReStart - Start")
This mode is intended for running the ClockBox as the single source of Midi-clock-data. It sends out clock pulses according to the value shown in the display. When you are doing a quantized restart - i.e. hitting the PLAY-button when the clock is already running it will send out another CLOCK START impulse on the next first downbeat.
  
### Mode: QRS Stop-Start ("QuantizedReStart - Stop Start")
This mode is intended for running the ClockBox as the single source of Midi-clock-data. It sends out clock pulses according to the value shown in the display. When you are doing a quantized restart - i.e. hitting the PLAY-button when the clock is already running it will send out a CLOCK STOP impulse followed by a CLOCK START impulse on the next first downbeat. This allows for devices like the SP404 Mk2 do be pulled into a running session without having to stop the clock manually.
  
### Mode: ext.Clk DIN 24
This mode puts the clock into follower-mode. it waits for clock-impulses (24 PPQN) on the TRS input on the back side of the box to automatically adopt to the incoming bpm. Incoming clock pulses are indicated by the 4th LED lighting up accordingly. Clock Data will be sent out via all midi-out ports as well as USB. The box will start sending out midi data when a MIDI START is received. the box will stop on an incoming MIDI STOP pulse.
  
### Mode: ext.Clk USB 24
This mode puts the clock into follower-mode. it waits for clock-impulses (24 PPQN) on the USB input of the box. When clock impulses are received the bpm settings will automatically adopt to the incoming bpm. Incoming clock pulses are indicated by the 4th LED lighting up accordingly. Clock Data will be sent out via all midi-out ports but NOT via USB. The box will start sending out midi data when a MIDI START is received. the box will stop on an incoming MIDI STOP pulse.
  
### Mode: ext StartStop
This mode puts the clock into follower-mode. It ONLY reacts to incoming MIDI START or MIDI STOP messages. The BPM of the ClockBox is not affected by incoming pulses and has to set manually.
  

## Rewiring to Hardware Serial  
Boards of revision 202401001 are layed out to use SoftwareSerial. However, early stages of developments revealed that using the built-in hardware serial interface of the µController leads to better results, especially in conjunction with incoming clock pulses.  
  

### Cutting the existing traces
First cut the trace that provides midi input. If done correctly then there should be NO connection between pin 6 of the optocoupler 6N138 (labeled 'U3') and pin D16 (which is pin number 14) on the Arduino.
![Cutting the trace for Midi IN .](https://github.com/Andymann/Arduino_ClockBox_v3/blob/main/images/image_1.jpg?raw=true)  
  
Secondly, cut the trace that provides midi output as shown in the picture. If done correctly then there should be NO connection between pin 2 of U2 ( 74LS241 )and pin A3 (pin number 20) of the on the Arduino. Note that on the IC labeled 'U2' the pins 2, 4, 6, 8, 11, 13, 15 and 17 are all bridged.
![Cutting the trace for Midi OUT .](https://github.com/Andymann/Arduino_ClockBox_v3/blob/main/images/image_2.jpg?raw=true)  
  
Now connect pin 1 (Tx) of the Arduino to pin 2 of IC labeled 'U2' (you can also connect pin 1 to pin 4, 6, 8, 11, 15 or 17 of U2, see above).  
Connect pin 2 (Rx) of the Arduino to pin 6 of the optocoupler 6N138
![Rewiring 74LS241.](https://github.com/Andymann/Arduino_ClockBox_v3/blob/main/images/image_4.jpg?raw=true)
  
![Rewiring 6N138.](https://github.com/Andymann/Arduino_ClockBox_v3/blob/main/images/image_5.jpg?raw=true)