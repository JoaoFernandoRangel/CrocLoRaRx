#include <Arduino.h>
#include <LoRa.h>
#include <NTPClient.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <Update.h>
#include <WiFi.h>

#include "cJSON.h"
#include "conf.h"
#include "myFS.h"

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
    xTaskCreatePinnedToCore(LoRaTask, "LoRaTask", 10000, NULL, 1, NULL, 1);  // Executa no núcleo APP (Core 1)
    // xTaskCreatePinnedToCore(ThingsBoardTask, "ThingsBoardTask", 10000, NULL, 1, NULL, 0);  // Executa no núcleo PRO (Core 0)
    Serial.println("Finish Setup");
}
void loop() { vTaskSuspend(NULL); }
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
                Serial.print("Msg processada: ");
                Serial.println(handleLoRaMessage(LoRaData));
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void ThingsBoardTask(void *pvParameters) {
    connectToWifi();
    _timeClient.begin();
    _timeClient.update();
    manageMQTT();
    while (1) {
        manageWiFi();
        if (!client.connected()) {
            manageMQTT();
        } else {
            client.loop();
            _timeClient.update();
            if (millis() - t[0] > retornaSegundo(30)) {
            }
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
    WiFi.mode(WIFI_MODE_STA);
    std::string ssid_str, pwd_str;
    // Itera sobre as redes para garantir que os dados estejam corretos
    for (int i = 0; i < networkCount; i++) {
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

String handleLoRaMessage(String LoRaInMessage) {
    // Criar um objeto JSON com cJSON
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        Serial.println("Erro ao criar o objeto JSON");
    }
    // Separar a mensagem em pares `id_valor` usando o delimitador `;`
    int start = 0, end;
    while ((end = LoRaInMessage.indexOf(';', start)) != -1) {
        String pair = LoRaInMessage.substring(start, end);
        // Dividir o par `id_valor` em chave e valor
        int separatorIndex = pair.indexOf('_');
        if (separatorIndex != -1) {
            String key = pair.substring(0, separatorIndex);
            String value = pair.substring(separatorIndex + 1);
            // Converter para float e limitar a 2 casas decimais
            float rawValue = value.toFloat();
            float roundedValue = round(rawValue * 100) / 100.0;

            // Adicionar ao JSON como par chave-valor
            cJSON_AddNumberToObject(root, key.c_str(), roundedValue);
        }
        start = end + 1;  // Avançar para o próximo par
    }
    // Serializar o objeto JSON
    char *jsonString = cJSON_PrintUnformatted(root);
    if (jsonString == NULL) {
        Serial.println("Erro ao serializar o JSON");
        cJSON_Delete(root);
    }
    String ret = String(jsonString);
    // Liberar memória
    cJSON_free(jsonString);
    cJSON_Delete(root);
    return ret;
}

bool publishToMqtt(String toGo) { return client.publish("v1/devices/me/telemetry", toGo.c_str(), strlen(toGo.c_str())); }