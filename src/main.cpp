
#include "Arduino.h"
#include <RadioLib.h>
// SX1278 has the following connections:
SX1278 radio = new Module(8, 6, 7, 3);

// flag to indicate that a packet was received
volatile bool rx_flag = false;
volatile bool txing = false;

// this function is called when a complete packet
// is received by the module
// IMPORTANT: this function MUST be 'void' type
//            and MUST NOT have any arguments!
#if defined(ESP8266) || defined(ESP32)
ICACHE_RAM_ATTR
#endif
void packetHandler(void)
{
    if (!txing)
    {
        rx_flag = true;
    }
}

void setup()
{
    Serial.begin(115200);

    // initialize SX1278 with default settings
    Serial.print(F("[DEBUG] Initializing ... "));
    // Only seems to actually work when using the full constructor
    int state = radio.beginFSK(
        435,    // Frequency
        4.8,    // Bit Rate
        5,      // Frequency Deviation
        125,    // RX Bandwidth
        15,     // TX Power
        16U,    // Preamble length
        false); // Use OOK instead of FSK

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
    state |= radio.setBitRate(1);
    state |= radio.setFrequencyDeviation(1);
    state |= radio.setRxBandwidth(25);
    state |= radio.setCurrentLimit(100);
    state |= radio.setCrcFiltering(false);
    state |= radio.setOutputPower(2);
    // state |= radio.setDataShaping(RADIOLIB_SHAPING_0_5);
    // state |= radio.variablePacketLengthMode(5);
    uint8_t syncWord[] = {0x01, 0x23, 0x45, 0x67,
                          0x89, 0xAB, 0xCD, 0xEF};
    state |= radio.setSyncWord(syncWord, 8);
    if (state != RADIOLIB_ERR_NONE)
    {
        Serial.print(F("[DEBUG] Unable to set configuration, code "));
        Serial.println(state);
        while (true)
        {
            delay(10);
        }
    }

    // start listening
    radio.setPacketReceivedAction(packetHandler);
    Serial.print(F("[DEBUG] Starting to listen ... "));
    state = radio.packetMode();
    // state |= radio.disableAddressFiltering();
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
uint8_t outBuf[64];

void loop()
{
    if (rx_flag)
    {
        Serial.println("[DEBUG] Handler called");
        int state = radio.readData(outBuf, 63);
        if (state != RADIOLIB_ERR_NONE)
        {
            Serial.print("[DEBUG] RX ERROR, state: ");
            Serial.print(state);
            Serial.print('\n');
        }
        else
        {
            Serial.print("[PACKET RX]");
            Serial.write(outBuf, 63);
            Serial.print('\n');
        }
        rx_flag = false;
    }

    // Transmit every 64 bytes, or on a newline char
    int sIn = Serial.read();
    if (sIn != -1)
    {
        serialInBuf[serialInBufLen++] = sIn;
        // /Serial.print((char)sIn);
        if (serialInBufLen == 63 || (char)sIn == '\n')
        {
            txing = true;
            int state = radio.transmit(serialInBuf, serialInBufLen);
            Serial.print("[DEBUG] Sent Packet, state ");
            Serial.print(state);
            Serial.print('\n');
            serialInBufLen = 0;
            txing = false;
            radio.startReceive();
        }
    }
}