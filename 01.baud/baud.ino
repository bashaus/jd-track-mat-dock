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

// Byte size
#define ARRAY_SIZE 20

// Checksums
int tx_checksum;
int rx_checksum;

// Playing flag
boolean isPlaying = false;

void setup() {
  // Connect
  
  delay(3000);
  
//  Serial.begin(9600);
//  rfid_baud(0x01);
  
  Serial.begin(19200);
  rfid_baud(0x00);
}

void loop() {
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

boolean rfid_baud(int baud) {
  byte in[1] = { baud };
  byte out[ARRAY_SIZE];

  rfid_tx(RFID_BAUD, in, 1);
  if (rfid_rx(out) <= 0) {
    return false;
  }
}
