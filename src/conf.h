#define Bomba 4

#define BombaCaju "C14W6ZGOQKxuKucdCFMj"
#define BombaGalinheiro "nvyrYVfkx4D99FG7fSIz"
#define BombaJardim "jojb3psqbghbwcw1todo"
#define LoRaGateway "eNQFayREloG8uS2Y3IMQ"

#if Bomba == 4
#define BombaTarget LoRaGateway

#elif Bomba == 3
#define BombaTarget BombaJardim
#elif Bomba == 2
#define BombaTarget BombaGalinheiro
#elif Bomba == 1
#define BombaTarget BombaCaju
#endif


// define the pins used by the transceiver module
#define ss 5
#define rst 14
#define dio0 2
#define DebugLoRa 0

#define retornaSegundo(x) (1000 * (x))
#define retornaMinuto(x) (60 * 1000 * (x))
#define retornaHora(x) (60 * 60 * 1000 * (x))
#define RelePin 27
#define WiFi_LED 33
#define MAX_ATTEMPTS 10
// Arquivos de configuração
#define RedeData "/wifiData.JSON"