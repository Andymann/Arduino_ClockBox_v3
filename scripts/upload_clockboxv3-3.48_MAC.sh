if [[ "$(uname)" != "Darwin" ]]; then
  echo "Ths script only works on macOS"
  return
else
  echo "Runninc update script for clockbox v3"
fi
DIRNAME=clockboxv3Uploader
mkdir $DIRNAME
cd $DIRNAME
curl -OL https://raw.githubusercontent.com/Andymann/Arduino_ClockBox_v3/refs/heads/main/firmware/clockboxV3-3.48.hex
curl -OL https://github.com/avrdudes/avrdude/releases/download/v8.1/avrdude_v8.1_macOS_64bit.tar.gz
tar -xvzf $(ls avrdude*)
stty -f $(ls /dev/tty.usbmodem*) 1200
sleep 1
PORT=$(ls /dev/tty.usbmodem*)
echo $PORT
./avrdude_macOS_64bit/bin/avrdude -p atmega32u4 -c avr109 -P $PORT -b 57600 -U flash:w:clockboxV3-3.47.hex:i
