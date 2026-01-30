# ClockBox v3 manual
<img src="images/ClockBoxV3/yellow_front_1.png" width="400"> 

## Intro  
Welcome to the wonderful world of tempo-syncing devices via midi. If you came here you probably tried to sync your DAW with a drumcomputer, a synthesizer or something else and reealized that this topic DOES have it quirks.  
[https://socialmidi.com/RL](https://socialmidi.com/)
## Features
6 Midi outputs  
1 Midi input 
1 Midi IN/ OUT via USB  
CV/Gate output  
Tempo presets  
Quantized Restart (QRS)  
open source
## Hardware  
### Top view  
Name buttons, etc  
<img src="images/ClockBoxV3/yellow_top_1.png" width="400">

### Front view  
cv gate, Mid IN  
<img src="images/ClockBoxV3/yellow_front_1.png" width="400">  

### Rear view  
6 Midi OUT  
<img src="images/ClockBoxV3/yellow_back_1.png" width="400">  

### Back view
The back side of the device provides additional information about the in- and outputs.  
<img src="images/ClockBoxV3/yellow_bottom_1.png" width="400">  

## Basic usage  
The ClockBox v3 can run standalone or connected to a computer. Just apply power via USB and you are good to go. When powered on the ClockBox v3 will show the currently installed firmware version for 2 seconds.
Initially, the device is set to mode  "QRS Stop Start". This way it serves as ready-to-go tempo master. Just turn it on, connect your devices and hit PLAY.
Diagram. Example: Ableton- midi config
### Set the tempo
The tempo can be set by tapping the TAP-button, dialing in the tempo with the encoder and by selecting previously saved values via preset buttons. Twisting the encoder will increase the value by +/-1. Pressing down on the encoder while turning it will change the tempo by +/-5. Short-clicking one of the preset buttons immediately recalls the previously stored tempo-value, pressing and holding the button for more than 2 seconds will make the ClockBox v3 fade to the new tempo.
### Save a tempo-preset
To save a preset press and hold the encoder and click one of the preset-buttons. The LEDs on top will flash red to show that the new value has successfully been saved.
### QRS: Quantized Restart
Just hit PLAY while the clock is running to fix everything.  
  

Quantized Restart allows for devices being seamlessly re-synced to the running clock without the need for stopping and re-starting everything. With multiple devices being connected to one common clock source there is always the chance of individual devices facing issues with tempodrift or other problems. When you realize thatone or more devices are not playing in sync anymore, simply hit PLAY while the clock is running. The LEDs on top will change their colour and all devices will automatically be re-synced on the next "1".
## Advanced usage
### Modes
With firmware version 3.49 the ClockBox v3 has 4 different modes which allow the device to perfectly fit into your setup:  
-QRS Stop Start  
-QRS Start   
-Follow Clock from DIN Midi  
-Follow Clock from USB  
The selected mode is shown in the lower part of the display.
### Changing the mode  
To change the current mode press and hold the encoder and click the PLAY button. The name of the new mode will be shown in the display. Repeat these steps to cycle through all available modes. The current mode is automatically saved and will be recalled when the ClockBox v3 is powered on the next time.
#### QRS Stop Start  
The ClockBox v3 serves as a tempo master when this mode is selected. This is the no-worry go-to-Mode. When you are running multiple devices and you experience one ore more of them not playing in sync anmore, you can seamlessly re-sync all devices by clicking th PLAY-button while the clock is running. This engages QRS mode. The ClockBox v3 will send a MIDI STOP signal, followed by a MIDI START signal on the next "1". This ensures all devices are autmatically re-synched again without you having to worry about timing, etc.
Example, 
##### QRS Stop Start finetuning
You can finetune the time between the MIDI STOP and MIDI START signal. To do so, press and hold the encoder and click the STOP-button. The display will show "QRS offset (PPQN)". twisting the encoder will allow you to set this value between 1 and 24. Releasing the encoder will store the selected value. When QRS offset is set to 24 the ClockBox v3 will send a MIDI STOP signal on the 4th beat and will send MIDI START on the next "1". The value dpends on the slowest of the devices that you are running but a value of 2 proved to be sufficient for most devices.
#### QRS Start
The ClockBox v3 serves as a tempo master when this mode is selected. When you engage a Quantized Restart by clicking PLAY while the clock is running, the device will only send a MIDI START signal on the next '1'.
#### Follow clock from DIN Midi  
The ClockBox v3 serves as a tempo follower. It receives and processes incoming tempo data from Midi IN. Incoming clock data are sent to the 6 Midi OUT on the back of the device and to USB.
#### Follow clock from Usb Midi  
The ClockBox v3 serves as a tempo follower. It receives and processes incoming tempo data from Midi USB.
Diagram, clock chain, QRS, 24 PPWN
## cv/gate 
You can control synthesizers, Eurorack modules, etc.
Diagram
### Clock muliplier  
Button combo, example
## Reset the device  
## Firmware update
Why, when, how?
### Update mode  
In order to update the firmware, the ClockBox v3 needs to be out into UpdateMode. This is done by powering off the device, holding PLAY and STOP buttons and attaching it to your computer. The top 4 LEDs are now lit red and the display shows that the device is in UpdateMode.

## FCC  
## Version  