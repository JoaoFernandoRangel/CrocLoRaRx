#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
// define the pins used by the transceiver module
#define ss 5
#define rst 14
#define dio0 2

int counter = 0;

void setup()
{
  // initialize Serial Monitor
  Serial.begin(115200);
  Serial.println("LoRa Sender");

  // setup LoRa transceiver module
  LoRa.setPins(ss, rst, dio0);
  while (!LoRa.begin(433E6))
  {
    Serial.println(".");
    delay(500);
  }
  // Change sync word (0xF3) to match the receiver
  // The sync word assures you don't get LoRa messages from other LoRa transceivers
  // ranges from 0-0xFF
  // LoRa.setSyncWord(0xF3);
  Serial.println("LoRa Initializing OK!");
}

String LoRaData;
void loop()
{
  // try to parse packet
  int packetSize = LoRa.parsePacket();
  if (packetSize)
  {
    // read packet
    while (LoRa.available())
    {
      LoRaData = LoRa.readString();
      Serial.print("Número recebido: ");
      Serial.println(LoRaData);
    }
    if ((LoRaData.toInt() % 2) == 1)
    {
      Serial.println("Número ímpar");
    }
    else
    {
      Serial.println("Número par");
    }
    LoRaData = "";
  }
}