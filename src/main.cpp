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



const uint8_t FEND = 0xC0;
const uint8_t FESC = 0xDB;
const uint8_t TFEND = 0xDC;
const uint8_t TFESC = 0xDD;
const uint8_t COMMAND = 0x00;



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
        56,    // Bit Rate
        44,      // Frequency Deviation
        166.7,    // RX Bandwidth
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
    // state = radio.setFrequency(435);
    // state |= radio.setBitRate(1);
    // state |= radio.setFrequencyDeviation(1);
    // state |= radio.setRxBandwidth(25);
    // state |= radio.setCurrentLimit(100);
    // state |= radio.setCrcFiltering(false);
    // state |= radio.setOutputPower(2);
    // uint8_t syncWord[] = {0x01, 0x23, 0x45, 0x67,
    //                       0x89, 0xAB, 0xCD, 0xEF};
    // state |= radio.setSyncWord(syncWord, 8);
    // if (state != RADIOLIB_ERR_NONE)
    // {
    //     Serial.print(F("[DEBUG] Unable to set configuration, code "));
    //     Serial.println(state);
    //     while (true)
    //     {
    //         delay(10);
    //     }
    // }

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
            Serial.write(FEND);
            Serial.write(COMMAND);
            for (int i = 0; i < totalLen; i++){
                uint8_t current_byte = rxMessageBuf[i];
                switch (current_byte){
                    case FEND:
                        Serial.write(FESC);
                        Serial.write(TFEND);
                        break;
                    case FESC:
                        Serial.write(FESC);
                        Serial.write(TFESC);
                    default:
                        Serial.write(current_byte);
                }
            }
            Serial.print(F("[PACKET RX]"));
            Serial.write(rxMessageBuf, totalLen);
            Serial.print('\n');
            resetRxBuffers();
        }
    }
}

bool FESCread = false;
bool FENDread = false;
bool C0read = false;
bool reading_packet = false;

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


        // KISS Framing
        // https://en.wikipedia.org/wiki/KISS_(amateur_radio_protocol)
        switch (sIn){
            case FEND:
                if(!reading_packet){
                    FENDread = true;
                }else{
                    reading_packet = false;
                    FENDread = false;
                    sendPackets(serialInBuf, serialInBufLen);
                    serialInBufLen = 0;
                }
                break;
            case COMMAND:
                if(FENDread){
                    FENDread = false;
                    reading_packet = true;
                    serialInBufLen = 0;
                }
                break;
            case FESC:
                FESCread = true;
                break;
            case TFEND:
                if (FESCread){
                    FESCread = false;
                    // write FEND
                    serialInBuf[serialInBufLen++] = FEND;
                } else{
                    //write tfend
                    serialInBuf[serialInBufLen++] = TFEND;
                }
                break;
            case TFESC:
                if (FESCread){
                    FESCread = false;
                    // write FESC
                    serialInBuf[serialInBufLen++] = FESC;
                } else{
                    //write tfesc
                    serialInBuf[serialInBufLen++] = TFESC;
                }
                break;
            default:
                if (FESCread){
                    FESCread = false; // THIS SHOULD NOT HAPPEN
                }
                if(reading_packet){
                    serialInBuf[serialInBufLen++] = sIn;
                }
                FENDread = false; // THIS SHOULD NOT MATTER
        }

        // Serial message is base64 encoded, so there is some overhead
        if (serialInBufLen > MAX_MESSAGE_SIZE)
        {
            //Packet was too long or a terrible error happened, start over
            serialInBufLen = 0;
            bool FESCread = false;
            bool FENDread = false;
            bool C0read = false;
            bool reading_packet = false;

        }
    }
}