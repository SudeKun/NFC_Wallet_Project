# NFC Project

This project implements NFC (Near Field Communication) functionality using Arduino-compatible boards with PN532 NFC modules. It includes libraries for NDEF (NFC Data Exchange Format) message handling and PN532 communication protocols.

## Features

- NDEF message creation, reading, and writing
- Support for MIFARE Classic and Ultralight tags
- Peer-to-peer (P2P) communication
- Tag emulation
- Multiple communication interfaces (HSU, I2C, SPI)

## Hardware Requirements

- Arduino-compatible board (e.g., Arduino Uno, ESP32)
- PN532 NFC module
- NFC tags/cards for testing

## Software Requirements

- PlatformIO IDE or VS Code with PlatformIO extension
- Arduino framework

## Installation

1. Clone or download this repository
2. Open the project in PlatformIO
3. Build and upload the desired example sketch

## Usage

### Examples

The `lib/NDEF/examples/` and `lib/PN532/examples/` directories contain various example sketches:

- `ReadTag.ino`: Read NDEF messages from NFC tags
- `WriteTag.ino`: Write NDEF messages to NFC tags
- `P2P_Send.ino` / `P2P_Receive.ino`: Peer-to-peer communication
- `emulate_tag_ndef.ino`: Tag emulation

### Library Usage

Include the necessary headers in your Arduino sketch:

```cpp
#include <NfcAdapter.h>
#include <NdefMessage.h>
```

Initialize the NFC adapter and perform operations as shown in the examples.

### Main Application

The main application in `src/main.cpp` provides NFC card cloning, reading, and emulation functionality for ESP32.

1. Connect the PN532 module to the ESP32:
   - TX -> GPIO 16 (RX2)
   - RX -> GPIO 17 (TX2)
   - Set DIP switches to OFF - OFF for HSU mode

2. Open the project in PlatformIO and build/upload the sketch.

3. Open the serial monitor (115200 baud) to interact with the device.

4. The application supports:
   - Reading MIFARE Classic cards
   - Cloning card data
   - Emulating tags with NDEF messages
   - Peer-to-peer communication via SNEP

Follow the serial prompts to perform operations.

## Project Structure

- `src/`: Main source code
- `lib/NDEF/`: NDEF library for message handling
- `lib/PN532/`: PN532 driver library
- `include/`: Additional includes
- `test/`: Test sketches

## Contributing

Contributions are welcome. Please submit issues and pull requests on the project repository.

## License

This project uses libraries with their respective licenses. Check individual library READMEs for details.