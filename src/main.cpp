#include "Arduino.h"
#include <RadioLib.h>
// SX1278 has the following connections:
SX1278 radio = new Module(8, 6, 7, 3);

// flag to indicate that a packet was received
volatile bool rx_flag = false;
volatile bool txing = false;

// Packet structure (3 bytes overhead):
// [1 byte sequence number][1 byte total packets][1 byte current packet size][payload...]
#define MAX_PACKET_SIZE 63
#define PACKET_OVERHEAD 3
#define MAX_PAYLOAD_SIZE (MAX_PACKET_SIZE - PACKET_OVERHEAD)
#define MAX_MESSAGE_SIZE 1024

uint8_t serialInBuf[MAX_MESSAGE_SIZE];
size_t serialInBufLen = 0;
uint8_t outBuf[MAX_PACKET_SIZE];

// Receive buffer management
uint8_t rxMessageBuf[MAX_MESSAGE_SIZE];
uint8_t rxPacketTracker[16]; // Bitmap to track received packets
uint8_t currentMessageTotalPackets = 0;
uint8_t receivedPacketsCount = 0;

void resetRxBuffers() {
    memset(rxPacketTracker, 0, sizeof(rxPacketTracker));
    currentMessageTotalPackets = 0;
    receivedPacketsCount = 0;
}

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

void sendPackets(uint8_t* data, size_t len) {
    uint8_t totalPackets = (len + MAX_PAYLOAD_SIZE - 1) / MAX_PAYLOAD_SIZE;
    uint8_t packet[MAX_PACKET_SIZE];
    
    for (uint8_t seq = 0; seq < totalPackets; seq++) {
        size_t payloadSize = min(MAX_PAYLOAD_SIZE, len - (seq * MAX_PAYLOAD_SIZE));
        
        packet[0] = seq;  // Sequence number
        packet[1] = totalPackets;  // Total packets
        packet[2] = payloadSize;  // Current packet size
        
        memcpy(packet + PACKET_OVERHEAD, 
               data + (seq * MAX_PAYLOAD_SIZE), 
               payloadSize);
        
        txing = true;
        int state = radio.transmit(packet, payloadSize + PACKET_OVERHEAD);
        Serial.print(F("[DEBUG] Sent Packet "));
        Serial.print(seq + 1);
        Serial.print(F("/"));
        Serial.println(totalPackets);
        
        delay(10); 
    }
    txing = false;
    radio.startReceive();
}

void handleReceivedPacket(uint8_t* data, size_t len) {
    if (len < PACKET_OVERHEAD) return;
    
    uint8_t seq = data[0];
    uint8_t totalPackets = data[1];
    uint8_t packetSize = data[2];
    


    // If this is the first packet of a new message or we need to restart
    if (currentMessageTotalPackets == 0 || totalPackets != currentMessageTotalPackets) {
        Serial.println(F("[DEBUG] Starting new message"));
        resetRxBuffers();
        currentMessageTotalPackets = totalPackets;
    }
    Serial.print(F("[DEBUG] Received Packet "));
    Serial.print(seq + 1);
    Serial.print(F("/"));
    Serial.println(totalPackets);
    if (!(rxPacketTracker[seq/8] & (1 << (seq%8)))) {
        // New packet
        memcpy(rxMessageBuf + (seq * MAX_PAYLOAD_SIZE), 
               data + PACKET_OVERHEAD, 
               packetSize);
        rxPacketTracker[seq/8] |= (1 << (seq%8));
        receivedPacketsCount++;
        
      
        
        if (receivedPacketsCount == totalPackets) {
            // Message complete
            size_t totalLen = (totalPackets - 1) * MAX_PAYLOAD_SIZE + packetSize;
            Serial.print(F("[PACKET RX]"));
            Serial.write(rxMessageBuf, totalLen);
            Serial.print('\n');
            resetRxBuffers();
        }
    }
}

void loop()
{
    if (rx_flag)
    {
        int state = radio.readData(outBuf, MAX_PACKET_SIZE);
        if (state == RADIOLIB_ERR_NONE)
        {
            handleReceivedPacket(outBuf, MAX_PACKET_SIZE);
        }
        rx_flag = false;
    }

    int sIn = Serial.read();
    if (sIn != -1)
    {
        serialInBuf[serialInBufLen++] = sIn;
        
        if (serialInBufLen >= MAX_MESSAGE_SIZE || (char)sIn == '\n')
        {
            sendPackets(serialInBuf, serialInBufLen);
            serialInBufLen = 0;
        }
    }
}