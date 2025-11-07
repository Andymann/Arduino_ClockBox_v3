# Update Firmware   
## MAC
### Update to firmware v3.47  
Before updating please remove all other USB devices from your computer (keyboard and mouse can stay)  
Next, set the clockbox v3 to UpdateMode:  
Disconnect USB from the box, press and hold both buttons PLAY and STOP and connect USB. The top 4 LEDs are lit bright red and the display shows  
  
ClockBox
    
UpdateMode
  
  
You can now release the buttons and head over to your MAC. Open a new terminal window. Run the following command. It downloads all the used binaries into a folder called clockboxv3Uploader, searches for the clockbox and updates its firmware. If the update was successfull the box automatically resets and starts running the new firmware.  
  
    curl -L https://raw.githubusercontent.com/Andymann/Arduino_ClockBox_v3/refs/heads/main/upload_clockboxv3-3.47_MAC.sh | bash
  

