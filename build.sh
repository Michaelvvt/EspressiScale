#!/bin/bash

echo "Building EspressiScale..."

# Path to PlatformIO
PIO_PATH="$HOME/.platformio/penv/bin/platformio"

# Check if PlatformIO exists
if [ ! -f "$PIO_PATH" ]; then
    echo "Error: PlatformIO not found at $PIO_PATH"
    exit 1
fi

echo "Using PlatformIO at $PIO_PATH"

# Run PlatformIO build
echo "Starting compilation..."
"$PIO_PATH" run

# Check build status
if [ $? -eq 0 ]; then
    echo "Build successful!"
    echo "Firmware location: .pio/build/lilygo-t-touchbar/firmware.bin"
else
    echo "Build failed!"
    exit 1
fi 