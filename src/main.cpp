
#include "Arduino.h"
#include <RadioLib.h>

// SX1278 has the following connections:
SX1278 radio = new Module(8, 6, 7, 3);

// flag to indicate that a packet was received
volatile bool rx_flag = false;

// this function is called when a complete packet
// is received by the module
// IMPORTANT: this function MUST be 'void' type
//            and MUST NOT have any arguments!
#if defined(ESP8266) || defined(ESP32)
ICACHE_RAM_ATTR
#endif
void packetHandler(void)
{
    rx_flag = true;
    // Serial.println("[DEBUG] Handler called");
    // if (busy)
    // {
    //     return;
    // }

    // uint8_t outBuf[64];
    // int state = radio.readData(outBuf, 64);
    // if (state == RADIOLIB_ERR_CRC_MISMATCH)
    // {
    //     Serial.println("[DEBUG] Received malformed packet");
    //     return;
    // }
    // Serial.print("[PACKET RX]");
    // Serial.write(outBuf, 64);
    // Serial.print('\n');
}

void setup()
{
    Serial.begin(115200);

    // initialize SX1278 with default settings
    Serial.print(F("[DEBUG] Initializing ... "));
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
    // Legal limit, 56 kbaud and 100 kHz bandwidth
    state |= radio.setBitRate(1);
    state |= radio.setFrequencyDeviation(0.2);
    state |= radio.setRxBandwidth(250.0);
    state |= radio.setCurrentLimit(100);
    state |= radio.setGain(0);
    state |= radio.setOutputPower(15);
    state |= radio.setDataShaping(RADIOLIB_SHAPING_0_5);
    uint8_t syncWord[] = {0x01, 0x23, 0x45, 0x67,
                          0x89, 0xAB, 0xCD, 0xEF};
    state = radio.setSyncWord(syncWord, 8);
    if (state != RADIOLIB_ERR_NONE)
    {
        Serial.print(F("[DEBUG] Unable to set configuration, code "));
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
    Serial.print(F("[DEBUG] Starting to listen ... "));
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

    if (Serial.available())
    {
        // Transmit every 64 bytes, or on a newline char
        int sIn = Serial.read();
        if (sIn != -1)
        {
            serialInBuf[serialInBufLen++] = sIn;
            Serial.print((char)sIn);
            if (serialInBufLen == 64 || (char)sIn == '\n')
            {
                int state = radio.transmit(serialInBuf, serialInBufLen);
                Serial.print("[DEBUG] Sent Packet, state ");
                Serial.print(state);
                Serial.print('\n');
                serialInBufLen = 0;
                radio.startReceive();
            }
        }
    }
    if (rx_flag)
    {
        Serial.println("[DEBUG] Handler called");
        uint8_t outBuf[64];
        int state = radio.readData(outBuf, 64);
        if (state == RADIOLIB_ERR_CRC_MISMATCH)
        {
            Serial.println("[DEBUG] Received malformed packet");
            
        } else{
            Serial.print("[PACKET RX]");
            Serial.write(outBuf, 64);
            Serial.print('\n');
        }
        rx_flag = false;
    }
}