# Arduino_ClockBox_v3<br>
<br>
Various revisions of hardware are reflected in different branches to accomodate for different
pinlayouts etc.

## Rewiring to Hardware Serial  
Boards of revision 202401001 are layed out to use SoftwareSerial. However, early stages of developments revealed that using the built-in hardware serial interface of the µController leads to better results, especially in conjunction with incoming clock pulses.  
  

### Cutting the existing traces
First cut the trace that provides midi input. If done correctly then there should be NO connection between pin 6 of the optocoupler 6N138 (labeled 'U3') and pin D16 (which is pin number 14) on the Arduino.
![Cutting the trace for Midi IN .](https://github.com/Andymann/Arduino_ClockBox_v3/blob/main/images/image_1.jpg?raw=true)  
  
Secondly, cut the trace that provides midi output as shown in the picture. If done correctly then there should be NO connection between pin 2 of U2 ( 74LS241 )and pin A3 (pin number 20) of the on the Arduino. Note that on the IC labeled 'U2' the pins 2, 4, 6, 8, 11, 13, 15 and 17 are all bridged.
![Cutting the trace for Midi OUT .](https://github.com/Andymann/Arduino_ClockBox_v3/blob/main/images/image_2.jpg?raw=true)  
  
Now connect pin 1 (Tx) of the Arduino to pin 2 of IC labeled 'U2' (you can also connect pin 1 to pin 4, 6, 8, 11, 15 or 17 of U2, see above).  
Connect pin 2 (Rx) of the Arduino to pin 6 of the optocoupler 6N138
![Rewiring .](https://github.com/Andymann/Arduino_ClockBox_v3/blob/main/images/image_4.jpg?raw=true)