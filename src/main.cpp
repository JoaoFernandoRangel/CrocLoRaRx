#include <Arduino.h>
#include <LoRa.h>
#include <SPI.h>
// define the pins used by the transceiver module
#define ss 5
#define rst 14
#define dio0 2

int counter = 0;
String LoRaData;

void LoRaTask(void *pvParameters);
void ThingsBoardTask(void *pvParameters);
void setup() {
    // initialize Serial Monitor
    Serial.begin(115200);

    Serial.println("Init Setup");

    // setup LoRa transceiver module
    LoRa.setPins(ss, rst, dio0);
    while (!LoRa.begin(433E6)) {
        Serial.println(".");
        delay(500);
    }
    // Change sync word (0xF3) to match the receiver
    // The sync word assures you don't get LoRa messages from other LoRa transceivers
    // ranges from 0-0xFF
    // LoRa.setSyncWord(0xF3);
    Serial.println("LoRa Initializing OK!");
    xTaskCreatePinnedToCore(LoRaTask, "LoRaTask", 10000, NULL, 1, NULL, 1);    // Executa no núcleo APP (Core 1)
    xTaskCreatePinnedToCore(ThingsBoardTask, "ThingsBoardTask", 10000, NULL, 1, NULL, 0);  // Executa no núcleo PRO (Core 0)
    Serial.println("Finish Setup");
}
void loop() { vTaskSuspend(NULL); }
void LoRaTask(void *pvParameters) {
    while (1) {
        // try to parse packet
        int packetSize = LoRa.parsePacket();
        if (packetSize) {
            // read packet
            while (LoRa.available()) {
                LoRaData = LoRa.readString();
                Serial.print("Número recebido: ");
                Serial.println(LoRaData);
            }
            if ((LoRaData.toInt() % 2) == 1) {
                Serial.println("Número ímpar");
            } else {
                Serial.println("Número par");
            }
            LoRaData = "";
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void ThingsBoardTask(void *pvParameters) {}
