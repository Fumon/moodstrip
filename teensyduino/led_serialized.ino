#include "LPD8806.h"
#include "SPI.h"

/*
  Pushes bytes to the SPI buffer from HID packets
*/

#define packetlength 64
#define nLEDs 92
LPD8806 strip = LPD8806(nLEDs);

int32_t blankcolor = strip.Color(0,0,0);

enum commandID {
  BLANK = 0x80,        // Blank the leds
  SETALLWHITE = 0x81,  // Set all LEDs white at the brightness level indicated by the following byte 0-127
  SETLED = 0x82,       // Set a specific LED (index as following byte) to specified color (next three bytes GRB order, 0-127
  SETBLOCK = 0x83,      // Set a block of pixels at offset (following byte) and size (next byte, # of bytes = size*3) Block begins at byte index 3
  SETFULL = 0x84
};

void respond() {
  Serial.print(0xE0);
}

void setup() {
  // Start up the LED strip
  strip.begin();
  strip.show();
  Serial.begin(115200);
}

uint8_t buffer[nLEDs*3];

void loop() {
  int n;
  byte w, offset, interrupted;
  uint16_t size;
  n = Serial.available();
  int32_t col;
  if (n > 0) {
    // First byte defines operation
    switch(static_cast<commandID>(Serial.read())) {
    case BLANK:
      col = blankcolor;
      goto SETSTRIP;
    case SETALLWHITE:
      w = Serial.read();
      col = strip.Color(w,w,w);
    SETSTRIP:
      for(int i; i< nLEDs; i++) {
        strip.setPixelColor(i, col);
      }
      respond();
      break;
    case SETLED:
      offset = Serial.read();
      col = (0x80 | Serial.read()) << 16
            | (0x80 | Serial.read() << 8)
            | (0x80 | Serial.read());
      strip.setPixelColor(offset, col);
      respond();
      break;
    case SETBLOCK:
      offset = Serial.read();
      size = Serial.read();
      interrupted = false;
      for(int i = 0; i < size; i++) {
        for(int j = 0; j < 3; j++){
          if(Serial.peek() & 0x80) {
            interrupted = true;
            break;
          }
          buffer[j] = Serial.read();
        }
        strip.setPixelColor(i, buffer[0], buffer[1], buffer[2]);
        if(interrupted) {
          break;
        }
      }
      if(interrupted) {
        break;
      }
      break;
    case SETFULL:
      Serial.readBytes(buffer, nLEDs*3);
      strip.copyBytes(buffer, 0, nLEDs*3);
    default:
      ;
    }
    strip.show();
  }
}
