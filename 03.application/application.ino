
#include <SPI.h>
#include <SdFat.h>
#include <SdFatUtil.h>
#include <SFEMP3Shield.h>

// RFID Config
#define TIMEOUT                1000 // Timeout

// RFID Codes
#define RFID_RESET             0x80 // Resets the Module 
#define RFID_FIRMWARE          0x81 // Firmware Reads the Firmware Revision of the Module 
#define RFID_TAG_SEEK          0x82 // Seek for Tag Continuously -- checks for presence of a tag 
#define RFID_TAG_SELECT        0x83 // Selects a Tag 
#define RFID_AUTHENCATE_BLOCK  0x85 // Authenticates the selected Block 
#define RFID_READ_BLOCK        0x86 // Reads from the specified Block 
#define RFID_READ_VALUE        0x87 // Read Value Reads from a Value Block 
#define RFID_WRITE_BLOCK       0x89 // Writes the data to the specified block 
#define RFID_WRITE_BALUE       0x8A // Formats and Writes a Value block 
#define RFID_WRITE_MIFARE      0x8B // Writes 4 byte data to Mifare Ultralight block 
#define RFID_WRITE_EEPROM_KEY  0x8C // Write Key Writes the Key to the EEPROM of the MFRC530 
#define RFID_INCREMENT         0x8D // Increment Increments a value block 
#define RFID_DECREMENT         0x8E // Decrement Decrements a value block 
#define RFID_ANTENNA_POWER     0x90 // Antenna Power Switches ON or OFF the RF field 
#define RFID_READ_PORT         0x91 // Reads from the Input port 
#define RFID_WRITE_PORT        0x92 // Write Port Writes to the Output port 
#define RFID_HALT              0x93 // Halt Halts the PICC 
#define RFID_BAUD              0x94 // Set Baud Rate Sets the new baud rate 
#define RFID_SLEEP             0x96 // Puts SM130 in sleep mode

#define TAG_SEEK_ATTEMPTS      2

/**
 * SM130 - MOSI
 *   HEADER     1 byte    Indicates the beginning of a frame (always 0xFF)
 *   RESERVED   1 byte    Reserved for future use: not implemented (always 0x00)
 *   LENGTH     1 byte    Length of the payload (includes COMMAND and the DATA bytes)
 *   COMMAND    1 byte    What operation to perform   
 *   DATA       N bytes   Parameters
 *   CHECKSUM   1 byte    Calculated by adding all the bytes in the packet except the Header byte
 *
 * SM130 - MISO
 *   HEADER     1 byte    Indicates the beginning of a frame (always 0xFF)
 *   RESERVED   1 byte    Reserved for future use: not implemented (always 0x00)
 *   LENGTH     1 byte    Length of the payload (includes COMMAND and the DATA bytes)
 *   COMMAND    1 byte    What operation was performed
 *   RESPONSE   N bytes   Response data
 *   CHECKSUM   1 byte    Calculated by adding all the bytes in the packet except the Header byte
 */

// Pins
#define PIN_BUTTON 14
#define PIN_RESET  5

// Byte size
#define ARRAY_SIZE 20

// Checksums
int tx_checksum;
int rx_checksum;

// Store the last reset
unsigned long rfid_last_reset = 0;

// Reset once per hour (3600000 = 1000 * 60 * 60)
#define RFID_RESET_INTERVAL 3600000

// MP3
SdFat sd;
SFEMP3Shield MP3player;

// Playing flag
boolean isPlaying = false;

void setup() {
  // Connect
  Serial.begin(9600);

  // Get version number
  rfid_firmware();
  
  pinMode(PIN_BUTTON, INPUT);
  pinMode(PIN_RESET , OUTPUT);
  
  digitalWrite(PIN_RESET, LOW);
  
  // Initialize the SD Card
  if(!sd.begin(SD_SEL, SPI_HALF_SPEED)) {
    sd.initErrorHalt();
  }
  
  // Change directory to /
  if(!sd.chdir("/")) {
    sd.errorHalt("sd.chdir");
  }

  // Initialize the MP3 Player Shield
  MP3player.begin();
  MP3player.setVolume(23, 23);
}

void loop() {
  if (button_pressed()) {
    start_playing();
  } else {
    stop_playing();
  }
  
  check_rfid_reset();
}

void check_rfid_reset() {
  // If the millis() buffer overflows
  if (millis() < rfid_last_reset) {
    rfid_hardware_reset();
    return;
  }
  
  // If it's time to reset
  if (millis() > rfid_last_reset + RFID_RESET_INTERVAL) {
    rfid_hardware_reset();
    return;
  }
}

boolean button_pressed() {
  return digitalRead(PIN_BUTTON) == HIGH;
}

void start_playing() {
  // Do nothing if we're already playing a file
  if (isPlaying) {
    return;
  }
  
  // Attempt to select the tag a maximum of 2 times
  // If anything more than that, reset the board and re-run the application 
  byte tag_data[ARRAY_SIZE];
  
  if (rfid_tag_seek(tag_data) == 0) {
    return;
  }
  
  // Read page data
  int page = 0x07; // in production: 0x11
  byte data[ARRAY_SIZE];
  int page_length = rfid_page_data(page, data);
  if (page_length < 0x12) {
    return;
  }
  
  // Build the Filename
  String filename = "";
  
  // Skip the first char as it's the command
  // Skip the second char as it's the block number
  // Maximum of 8 chars
  for (int i=2; i < min(8 + 2, page_length); ++i) { // in production: for (int i=4; i < min(8 + 2, page_length); ++i) { 
    // Only allow A-Z and a-z and 0-9 and hypen (-) and underscore(_)
    if (!(data[i] >= 0x30 && data[i] <= 0x39)) { // 0-9
      if (!(data[i] >= 0x41 && data[i] <= 0x5A)) { // A-Z
        if (!(data[i] >= 0x61 && data[i] <= 0x7A)) { // a-z
          if (!(data[i] == 0x2D || data[i] == 0x5F)) { // hypen (-) and underscore(_)
            break;
          }
        }
      }
    }
    
    filename += String((char) data[i]);
  }
  
  filename += ".mp3";
  
  char filenameChars[filename.length()+1];
  filename.toCharArray(filenameChars, filename.length()+1);
  
  // Play the song
  MP3player.playMP3(filenameChars, 0);
  
  // Flag the song as playing
  isPlaying = true;
}

void stop_playing() {
  if (!isPlaying) {
    return;
  }
  
  // Stop playing
  MP3player.stopTrack();
  
  // Flag off
  isPlaying = false;
}

void rfid_tx(int command, byte data[], int data_size = 0) {
  rfid_rx_clear();
  
  delay(100);

  tx_checksum = 0;

  // HEADER     1 byte    Indicates the beginning of a frame (always 0xFF)
  rfid_tx(0xFF);

  // RESERVED   1 byte    Reserved for future use: not implemented (always 0x00)
  rfid_tx(0x00);

  // LENGTH     1 byte    Length of the payload (includes COMMAND and the DATA bytes)
  rfid_tx_and_checksum(data_size + 1);

  // COMMAND    1 byte    What operation to perform   
  rfid_tx_and_checksum(command); // CMD

  // DATA       N bytes   Parameters
  for (int i=0; i < data_size; ++i) {
    rfid_tx_and_checksum(data[i]); // DATA[0..N]
  }

  // CHECKSUM   1 byte    Calculated by adding all the bytes in the packet except the Header byte
  rfid_tx(tx_checksum);
}

void rfid_tx(byte data) {
  Serial.write(data);
}

void rfid_tx_and_checksum(byte data) {
  rfid_tx(data);
  tx_checksum += data;
}

/**
 * @return int -- number of bytes returned
 */
void rfid_rx_clear() {
  while (Serial.available()) {
    Serial.read();
  }
}
 
int rfid_rx(byte response[]) {
  delay(100);

  // HEADER     1 byte    Indicates the beginning of a frame (always 0xFF)
  do {
    byte first;

    if (!rfid_rx_byte(first)) {
      return 0;
    }

    if (first == 0xFF) { /* STX */
      break; // Start processing
    }

    // Otherwise keep looping until something of repute comes along
  } 
  while (true);

  // Reset RX checksum
  rx_checksum = 0;

  // RESERVED   1 byte    Reserved for future use: not implemented (always 0x00)
  byte reserved;
  rfid_rx_byte(reserved); // discard byte

  // LENGTH     1 byte    Length of the payload (includes COMMAND and the DATA bytes)
  byte data_length;
  rfid_rx_and_checksum(data_length);

  // COMMAND    1 byte    What operation was performed
  rfid_rx_and_checksum(response[0]);

  // RESPONSE   N bytes   Response data
  for (int i=1; i < data_length; ++i) {
    rfid_rx_and_checksum(response[i]);
  }

  // CHECKSUM   1 byte    Calculated by adding all the bytes in the packet except the Header byte
  byte expected_checksum;
  rfid_rx_byte(expected_checksum);

  // Mod the received checksum
  rx_checksum = rx_checksum % 0x100;

  // Checksum
  if (expected_checksum != rx_checksum) {
    return 0;
  }

  return data_length;
}

boolean rfid_rx_byte(byte &out) {
  int start = millis();
  while (!Serial.available()) {
    if (millis() > start + TIMEOUT) {
      return false;
    }
  }

  out = Serial.read();
  return true;
}

boolean rfid_rx_and_checksum(byte &out) {
  if (!rfid_rx_byte(out)) {
    return false;
  }

  rx_checksum += out;
  return true;
}

/* */

int rfid_hardware_reset() {
  digitalWrite(PIN_RESET, HIGH);
  delay(100);
  digitalWrite(PIN_RESET, LOW);
  delay(1000);
  
  rfid_last_reset = millis();
}

int rfid_firmware() {
  byte in[0] = {
  };
  byte out[ARRAY_SIZE];

  rfid_tx(RFID_FIRMWARE, in, 0);
  return rfid_rx(out);
}

int rfid_tag_seek(byte out[]) {
  byte in[0] = {};

  int attempts = 0;
  boolean passed = false;
  do {
    rfid_tx(RFID_TAG_SEEK, in, 0);
    
    while (true) {
      int rx_length = rfid_rx(out);
  
      switch (rx_length) {
        case 0: // error
          return 0;
    
        case 2: // not ready
          switch (out[1]) {
            case 0x4C: // L
              // 4C (L): Command in progress.
              break;
      
            case 0x55: // U
              // 55 (U): Command in progress but RF Field is OFF.
              return 0;
      
            default:
              break;
          }
    
          continue;
    
        case 6: // found card
        case 9: // found card
          return rx_length;
    
        default: // unknown response
          return 0;
      }
    }
    
    ++attempts;
  } while (attempts <= TAG_SEEK_ATTEMPTS);
  
  return 0;
}

/**
 * FF -- STX
 * 0  -- RESERVED
 * 12 -- LENGTH
 * 86 -- CMD
 * 0  -- FIRST BLOCK
 * 04 5E 93 41 
 * 62 B6 28 80 
 * 7C 48 00 00 
 * E1 10 12 00
 * 55 -- CHECKSUM
 */

int rfid_page_data(int page, byte out[]) {
  byte in[1] = { page };
  rfid_tx(RFID_READ_BLOCK, in, 1);
  return rfid_rx(out);
}
