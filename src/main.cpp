#include <Arduino.h>
#include <LoRa.h>
#include <NTPClient.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <Update.h>
#include <WiFi.h>

#include "cJSON.h"
#include "conf.h"
#include "esp_task_wdt.h"
#include "myFS.h"
#include "utils.h"

WiFiClient espClient;
PubSubClient client(espClient);
WiFiUDP _ntpUDP;
NTPClient _timeClient(_ntpUDP, "pool.ntp.org", -10800);

// Configurações do ThingsBoard
const char *ssid;
const char *password;
const char *mqtt_server = "demo.thingsboard.io";

int counter = 0;
unsigned long t[5];
String LoRaData;

void LoRaTask(void *pvParameters);
void ThingsBoardTask(void *pvParameters);
bool connectToWifi();
void manageWiFi();
void manageMQTT();
void reconnectMQTT();
String handleLoRaMessage(String LoRaInMessage);
bool publishToMqtt(String toGo);

void setup() {
    // initialize Serial Monitor
    Serial.begin(115200);
    Serial.println("Init Setup");
    if (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)) {
        Serial.println("LittleFS Mount Failed");
        return;
    }
    // xTaskCreatePinnedToCore(LoRaTask, "LoRaTask", 10000, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(ThingsBoardTask, "ThingsBoardTask", 10000, NULL, 1, NULL, APP_CPU_NUM);  // Executa no núcleo PRO (Core 1)
    Serial.println("Finish Setup");
}
void loop() { vTaskSuspend(NULL); }
bool recMsgFlag = false;
bool loRaTaskStarted = false;
void LoRaTask(void *pvParameters) {
    // setup LoRa transceiver module
    LoRa.setPins(ss, rst, dio0);
    while (!LoRa.begin(433E6)) {
        Serial.println(".");
        delay(500);
    }
    // Change sync word (0xF3) to match the receiver
    // The sync word assures you don't get LoRa messages from other LoRa transceivers
    // ranges from 0-0xFF
    LoRa.setSyncWord(0xF3);
    Serial.println("LoRa Initializing OK!");
    while (1) {
        // try to parse packet
        int packetSize = LoRa.parsePacket();
        if (packetSize) {
            while (LoRa.available()) {
                LoRaData = LoRa.readString();
                Serial.print("Msg recebido: ");
                Serial.println(LoRaData);
                recMsgFlag = true;
#if DebugLoRa
                Serial.print("Msg processada: ");
                Serial.println(handleLoRaMessage(LoRaData));
#endif
            }
        }
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void ThingsBoardTask(void *pvParameters) {
    connectToWifi();
    _timeClient.begin();
    _timeClient.update();
    manageMQTT();
    while (1) {
        String LoRaMsg, toGo;
        esp_task_wdt_reset();
        manageWiFi();
        if (!client.connected()) {
            manageMQTT();
        } else {
            client.loop();
            _timeClient.update();
            // if (millis() - t[0] > retornaSegundo(30)) {
            //     LoRaMsg = handleLoRaMessage(LoRaData);
            //     Serial.print("LoRaMsg: ");
            //     Serial.println(LoRaMsg);
            //     toGo = addTimeToJSON(LoRaMsg, _timeClient.getFormattedTime());
            //     Serial.print("toGo: ");
            //     Serial.println(toGo);
            //     if (publishToMqtt(toGo)) {
            //         Serial.printf("[SUCESSO]\nMensagem: %s\n", toGo);
            //     } else {
            //         Serial.printf("[FALHOU]\nMensagem: %s\n", toGo);
            //     }
            //     recMsgFlag = false;
            //     t[0] = millis();
            // }
            if (recMsgFlag) {
                LoRaMsg = handleLoRaMessage(LoRaData);
                toGo = addTimeToJSON(LoRaMsg, _timeClient.getFormattedTime());
#if DebugLoRa
                Serial.print("LoRaMsg: ");
                Serial.println(LoRaMsg);
                Serial.print("toGo: ");
                Serial.println(toGo);
#endif
                if (publishToMqtt(toGo)) {
                    Serial.print("[SUCESSO]");
                    Serial.print("toGo: ");
                    Serial.println(toGo);
                } else {
                    Serial.print("[FALHOU]");
                    Serial.print("toGo: ");
                    Serial.println(toGo);
                }
                recMsgFlag = false;
            }
        }
        // Verificar se LoRaTask deve ser iniciado
        if (!loRaTaskStarted) {
            xTaskCreatePinnedToCore(LoRaTask, "LoRaTask", 10000, NULL, 1, NULL, PRO_CPU_NUM);  // Executa no núcleo PRO (Core 0)
            loRaTaskStarted = true;                                                            // Setar a flag após iniciar a tarefa
        }
    }
}

void manageWiFi() {
    if (WiFi.status() != WL_CONNECTED) {
        connectToWifi();
        digitalWrite(WiFi_LED, LOW);  // Acende LED WiFi indicando desconexão
    } else {
        digitalWrite(WiFi_LED, HIGH);  // Acende LED WiFi indicando conexão
    }
}
void manageMQTT() {
    client.setServer(mqtt_server, 1883);
    // client.setCallback(callback);
    reconnectMQTT();
}
void reconnectMQTT() {
    int contadorMQTT = 0;
    while (!client.connected() && contadorMQTT < 15) {
        esp_task_wdt_reset();
        Serial.print("Tentando conectar ao MQTT...");
        if (client.connect("ESP32Client", BombaTarget, NULL)) {
            Serial.println("Conectado");
            client.subscribe("v1/devices/me/rpc/request/+");
            contadorMQTT = 0;
        } else {
            Serial.print("falhou, rc=");
            Serial.print(client.state());
            Serial.println(" tente novamente em 5 segundos");
            delay(5000);
            esp_task_wdt_reset();
            contadorMQTT++;
        }
    }
}

bool connectToWifi() {
    int maxAttemptsPerNetwork = MAX_ATTEMPTS;
    bool notConnected = true;
    String jsonString = readFile(LittleFS, RedeData, false);
    delay(2000);
    Serial.println("jsonString:");
    Serial.println(jsonString);
    cJSON *json = cJSON_Parse(jsonString.c_str());
    if (json == NULL) {
        Serial.println("Erro ao analisar JSON.");
    }
    // Verifica se o campo 'networks' existe e é um array
    cJSON *networks = cJSON_GetObjectItemCaseSensitive(json, "networks");
    // if (!cJSON_IsArray(networks)) {
    //     Serial.println("Campo 'networks' não encontrado ou não é um array.");
    //     cJSON_Delete(json);
    // }
    // Obtém o tamanho do array
    int networkCount = cJSON_GetArraySize(networks);
    Serial.print("networkCount: ");
    Serial.println(networkCount);
    // WiFi.mode(WIFI_MODE_STA);
    std::string ssid_str, pwd_str;
    // Itera sobre as redes para garantir que os dados estejam corretos
    for (int i = 0; i < networkCount; i++) {
        esp_task_wdt_reset();
        int contador = 0;
        cJSON *network = cJSON_GetArrayItem(networks, i);
        if (cJSON_IsObject(network)) {
            cJSON *ssid = cJSON_GetObjectItemCaseSensitive(network, "SSID");
            cJSON *pwd = cJSON_GetObjectItemCaseSensitive(network, "PWD");

            if (cJSON_IsString(ssid) && cJSON_IsString(pwd)) {
                ssid_str = ssid->valuestring;  // Atribui o valor do JSON
                pwd_str = pwd->valuestring;    // Atribui o valor do JSON

                Serial.print("SSID: ");
                Serial.println(ssid_str.c_str());
                Serial.print("PWD: ");
                Serial.println(pwd_str.c_str());

                WiFi.begin(ssid_str.c_str(), pwd_str.c_str());
                unsigned long startTime = millis();
                while (WiFi.status() != WL_CONNECTED && contador <= maxAttemptsPerNetwork) {
                    Serial.print(".");
                    contador++;
                    vTaskDelay(pdMS_TO_TICKS(500));
                    esp_task_wdt_reset();
                }
                if (WiFi.status() == WL_CONNECTED) {
                    Serial.println();
                    Serial.println("WiFi connected");
                    Serial.print("IP address: ");
                    Serial.println(WiFi.localIP());
                    notConnected = false;
                    cJSON_Delete(json);
                    return true;  // Conectado com sucesso
                } else {
                    Serial.println();
                    Serial.println("Não foi possível conectar à WiFi, tentando próxima rede.");
                    notConnected = true;
                }
            }
        }
    }
    cJSON_Delete(json);
    return false;  // Isso nunca será executado devido ao ESP.restart()
}

bool publishToMqtt(String toGo) { return client.publish("v1/devices/me/telemetry", toGo.c_str()); }