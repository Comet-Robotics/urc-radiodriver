
#include "Arduino.h"

// G0 : D6
// RST : D7
/*
  RadioLib SX127x Receive with Interrupts Example

  This example listens for LoRa transmissions and tries to
  receive them. Once a packet is received, an interrupt is
  triggered. To successfully receive data, the following
  settings have to be the same on both transmitter
  and receiver:
  - carrier frequency
  - bandwidth
  - spreading factor
  - coding rate
  - sync word

  Other modules from SX127x/RFM9x family can also be used.

  For default module settings, see the wiki page
  https://github.com/jgromes/RadioLib/wiki/Default-configuration#sx127xrfm9x---lora-modem

  For full API reference, see the GitHub Pages
  https://jgromes.github.io/RadioLib/
*/

// include the library
#include <RadioLib.h>

// SX1278 has the following connections:
// NSS pin:   10
// DIO0 pin:  2
// RESET pin: 9
// DIO1 pin:  3
SX1278 radio = new Module(8, 6, 7, 3);

// or detect the pinout automatically using RadioBoards
// https://github.com/radiolib-org/RadioBoards
/*
#define RADIO_BOARD_AUTO
#include <RadioBoards.h>
Radio radio = new RadioModule();
*/

// flag to indicate that a packet was received
volatile bool busy = false;

// this function is called when a complete packet
// is received by the module
// IMPORTANT: this function MUST be 'void' type
//            and MUST NOT have any arguments!
#if defined(ESP8266) || defined(ESP32)
ICACHE_RAM_ATTR
#endif
void packetHandler(void)
{
    if (busy){ return; }

    uint8_t outBuf[64];
    int state = radio.readData(outBuf, 64);
    Serial.print("[PACKET RX]: ");
    Serial.write(outBuf, 64);
    Serial.print('\n');
}

void setup()
{
    Serial.begin(115200);

    // initialize SX1278 with default settings
    Serial.print(F("[SX1278] Initializing ... "));
    int state = radio.beginFSK();
    if (state == RADIOLIB_ERR_NONE)
    {
        Serial.println(F("success!"));
    }
    else
    {
        Serial.print(F("failed, code "));
        Serial.println(state);
        while (true)
        {
            delay(10);
        }
    }

    // Configure settings
    state = radio.setFrequency(435);
    //   /Serial.println(state);
    // Legal limit, 56 kbaud and 100 kHz bandwidth
    state |= radio.setBitRate(56);
    state |= radio.setFrequencyDeviation(44);
    state |= radio.setRxBandwidth(250.0);
    state |= radio.setCurrentLimit(100);
    state |= radio.setDataShaping(RADIOLIB_SHAPING_0_5);
    uint8_t syncWord[] = {0x01, 0x23, 0x45, 0x67,
                          0x89, 0xAB, 0xCD, 0xEF};
    state = radio.setSyncWord(syncWord, 8);
    if (state != RADIOLIB_ERR_NONE)
    {
        Serial.print(F("Unable to set configuration, code "));
        Serial.println(state);
        while (true)
        {
            delay(10);
        }
    }

    // set the function that will be called
    // when new packet is received
    radio.setPacketReceivedAction(packetHandler);

    // start listening
    Serial.print(F("[SX1278] Starting to listen ... "));
    state = radio.packetMode();
    state |= radio.startReceive();
    if (state == RADIOLIB_ERR_NONE)
    {
        Serial.println(F("success!"));
    }
    else
    {
        Serial.print(F("failed, code "));
        Serial.println(state);
        while (true)
        {
            delay(10);
        }
    }


    
}



uint8_t serialInBuf[64];
size_t serialInBufLen = 0;

void loop()
{

    if (Serial.available()){
        int sIn = Serial.read();
        serialInBuf[serialInBufLen++] = sIn;
        if (serialInBufLen == 64 || sIn == '\n'){
            busy = true;
            int state = radio.transmit(serialInBuf, serialInBufLen);
            Serial.println("[DEBUG] Sent Packet");
            serialInBufLen = 0;
            busy = false;
            radio.startReceive();

        }
    }

}