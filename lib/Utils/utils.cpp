#include "Arduino.h"
#include "cJSON.h"
#include "./conf.h"
#include "myFS.h"






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

String addTimeToJSON(String toGo, String timeStamp) {
    // Parse o JSON existente
    cJSON *root = cJSON_Parse(toGo.c_str());
    if (root == NULL) {
        Serial.println("Erro ao analisar o JSON existente.");
        return "";
    }

    // Criar um novo JSON para adicionar o timeStamp
    cJSON *newRoot = cJSON_CreateObject();
    if (newRoot == NULL) {
        Serial.println("Erro ao criar o novo JSON.");
        cJSON_Delete(root);
        return "";
    }

    // Adicionar o timeStamp como o primeiro campo
    cJSON_AddStringToObject(newRoot, "timeStamp", timeStamp.c_str());

    // Mesclar o JSON existente no novo JSON
    cJSON *child = NULL;
    cJSON_ArrayForEach(child, root) {
        cJSON_AddItemToObject(newRoot, child->string, cJSON_Duplicate(child, 1));
    }

    // Serializar o JSON resultante
    char *jsonString = cJSON_PrintUnformatted(newRoot);
    if (jsonString == NULL) {
        Serial.println("Erro ao serializar o novo JSON.");
        cJSON_Delete(root);
        cJSON_Delete(newRoot);
        return "";
    }

    // Converter para String
    String result = String(jsonString);

    // Liberar memória
    cJSON_free(jsonString);
    cJSON_Delete(root);
    cJSON_Delete(newRoot);

    return result;
}
