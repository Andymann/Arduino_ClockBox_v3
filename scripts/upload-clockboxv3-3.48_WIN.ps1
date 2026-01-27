# PowerShell script for uploading ClockBox v3 firmware on Windows 11
$avrdudeUrl = "https://github.com/avrdudes/avrdude/releases/download/v8.1/avrdude-v8.1-windows-x64.zip"
$avrdudeZip = "avrdude_Windows_64bit.zip"
$firmwareUrl = "https://raw.githubusercontent.com/Andymann/Arduino_ClockBox_v3/refs/heads/main/firmware/clockboxV3-3.48.hex"

if ($PSVersionTable.PSVersion.Major -lt 5) {
    Write-Host "This script requires PowerShell 5.0 or later"
    exit 1
}

Write-Host "Running update script for clockbox v3"
$DIRNAME = "clockboxv3Uploader"

# Create directory if it doesn't exist
if (Test-Path $DIRNAME) {
    Remove-Item -Path $DIRNAME -Recurse -Force
}
New-Item -ItemType Directory -Path $DIRNAME | Out-Null
Set-Location $DIRNAME

# Download firmware hex file
Write-Host "Downloading firmware..."
Invoke-WebRequest -Uri $firmwareUrl -OutFile "clockboxV3-3.48.hex"

# Download avrdude for Windows
Write-Host "Downloading avrdude..."
Invoke-WebRequest -Uri $avrdudeUrl -OutFile $avrdudeZip

# Extract avrdude
Write-Host "Extracting avrdude..."
Expand-Archive -Path $avrdudeZip -DestinationPath . -Force

# Find COM port (look for USB serial devices, typically COM3, COM4, etc.)
Write-Host "Searching for USB serial ports..."
$serialPorts = Get-WmiObject -Class Win32_SerialPort | Where-Object { 
    $_.Description -like "*USB*" -or 
    $_.Description -like "*Serial*" -or 
    $_.Description -like "*COM*" 
} | Select-Object DeviceID, Description, Name

if (-not $serialPorts -or $serialPorts.Count -eq 0) {
    Write-Host "No USB serial port found. Please ensure your device is connected."
    exit 1
}

# Display available ports
Write-Host "`nAvailable USB Serial Ports:"
Write-Host "============================"
for ($i = 0; $i -lt $serialPorts.Count; $i++) {
    $port = $serialPorts[$i]
    Write-Host "[$($i + 1)] $($port.DeviceID) - $($port.Description)"
}

# Prompt user to select a port
Write-Host ""
do {
    $selection = Read-Host "Select port number (1-$($serialPorts.Count))"
    $selectedIndex = [int]$selection - 1
    
    if ($selectedIndex -ge 0 -and $selectedIndex -lt $serialPorts.Count) {
        $PORT = $serialPorts[$selectedIndex].DeviceID
        Write-Host "Selected port: $PORT ($($serialPorts[$selectedIndex].Description))"
        break
    } else {
        Write-Host "Invalid selection. Please enter a number between 1 and $($serialPorts.Count)."
    }
} while ($true)

# Set serial port to 1200 baud to trigger bootloader mode
Write-Host "Setting port to 1200 baud to trigger bootloader mode..."
try {
    # Use mode command to set baud rate
    $process = Start-Process -FilePath "mode" -ArgumentList "$PORT BAUD=1200" -NoNewWindow -Wait -PassThru
    Start-Sleep -Seconds 1
} catch {
    Write-Host "Warning: Could not set baud rate. Continuing anyway..."
}

# Find avrdude executable
$avrdudeExe = Get-ChildItem -Path . -Filter "avrdude.exe" -Recurse | Select-Object -First 1

if (-not $avrdudeExe) {
    Write-Host "Error: avrdude.exe not found after extraction"
    exit 1
}

Write-Host "Flashing firmware..."
# Run avrdude to flash the firmware
& $avrdudeExe.FullName -p atmega32u4 -c avr109 -P $PORT -b 57600 -U flash:w:clockboxV3-3.48.hex:i

if ($LASTEXITCODE -eq 0) {
    Write-Host "Firmware upload completed successfully!"
} else {
    Write-Host "Firmware upload failed with exit code: $LASTEXITCODE"
    exit $LASTEXITCODE
}
