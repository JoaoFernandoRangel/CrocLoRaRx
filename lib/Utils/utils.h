#include "Arduino.h"
#include "cJSON.h"
#include "conf.h"
#include "myFS.h"



String handleLoRaMessage(String LoRaInMessage);
String addTimeToJSON(String toGo, String timeStamp);