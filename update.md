# Firmware update.   
To find out more about the latest firmware and its features, please have a look at the [changelog](https://github.com/Andymann/Arduino_ClockBox_v3/blob/main/changelog.md).  
  
You can update the clockbox's firmware yourself. Either build it from scratch using the .ino file from the repository or select and flash a prebuilt firmware. Please note that this procedure is only available for clockboxes past the 'early-prototype' stage. Don't worry: If you own a device that does not look like hand-soldered, hand-glued and all-too-hacky you are good to go. There is no way to destroy the device with the wrong firmware. Worst case: A few buttons do not work as expected and the encoder might behave incorrectly. If this happened to you just slect the correct .hex-file and try again.  
However, in order to successfully update a prebuilt firmware you first need to set the clockbox v3 into update mode.  
  
## Set clockbox v3 into update mode  
Please remove all all other USB devices, especially Arduinos, from your computer (keyboard and mouse can stay). Disconnect the clockbox v3, press and hold both buttons PLAY and STOP and connect the clockbox v3 to your computer via USB . The top 4 LEDs are now lit bright red and the display shows  
  
<img src="images/updatemode.jpg" width="200">  
  
  
  
You can now release the buttons and move over to your computer.

For all operating systems:
Open a new terminal window and run the command below for the firmware version that you want the box to be updated to. The script downloads all the necessary binaries into a folder called clockboxv3Uploader, searches for the clockbox and updates its firmware. If the update was successfull the box automatically resets and starts running the new firmware. 

## Instructions for computers running MacOS:  
### Firmware v3.48
    curl -L https://raw.githubusercontent.com/Andymann/Arduino_ClockBox_v3/refs/heads/main/scripts/upload_clockboxv3-3.48_MAC.sh | bash

