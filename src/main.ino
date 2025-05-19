#include <WiFi.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>

/////////////// Parametros editaveis /////////////////
// Mudar client Id aqui para ser um valor único
// Ver mensagens MQTT: https://www.hivemq.com/demos/websocket-client/ -> connect -> subscriptions add new topic subscription -> cisterna/# -> subscribe
const char *clientId = "ESP32Client7898456";

// Credenciais do Wi-Fi e broker MQTT
const char *ssid = "";
const char *password = "";
const char *mqttServerAddr = "broker.hivemq.com";
#define mqttServerPort 1883

// Medidas do recipiente em uso
#define CM_TO_BOTTOM 29.5 // Centímetros do sensor ultrassonico até a base do recipiente
#define LITERS_PER_CM 0.1 // 10cm * 10cm * 1cm= 100cm³ = 0.1 L

// Parametros de qualidade desejados
#define temperaturaMaxima 30.0
#define temperaturaMinima 20.0
#define nivelMinimo 6.0
#define pHMaximo 8.0
#define pHMinimo 6.0
#define TDSMaximo 2.5 // Não encontrado sensor de solidos dissolvidos para venda
#define turbidezMaxima 20

#define closeIfAltered true // Defina true para fechar ou false para abrir quando qualquer medida realizada estiver fora dos parametros acima.

////////////////////////////
////////////////////////////
////////////////////////////

// Definição dos pinos
#define VALVE_OUT 25 // HIGH para passar agua
#define TEMPERATURE_1WIRE 14
#define ULTRASOUND_ECHO_IN 26
#define ULTRASOUND_TRIG_OUT 27
#define PH_IN 35
// #define TDS_IN 39           // VN pin // Sensor TDS não encontrado para compra
#define TURBIDITY_SENSOR 36 // VP pin

// Wi-Fi e MQTT
WiFiClient espClient;
PubSubClient client(espClient);
#define MAX_MQTT_CONNECT_ATTEMPTS 3

// 1 Wire Protocol
OneWire oneWire(TEMPERATURE_1WIRE);
DallasTemperature sensors(&oneWire);

void connectMQTT()
{
    if (client.connected())
    {
        client.loop(); // Mantém o MQTT ativo
        return;
    }

    client.setServer(mqttServerAddr, mqttServerPort);
    int attempt = 0;

    while (!client.connected() && attempt < MAX_MQTT_CONNECT_ATTEMPTS)
    {
        attempt++;
        Serial.print("Conectando ao broker MQTT...");
        if (client.connect(clientId, "", ""))
        {
            Serial.println(" conectado!");
            client.loop(); // Mantém o MQTT ativo
        }
        else
        {
            Serial.print(" falhou, rc=");
            Serial.print(client.state());
            Serial.print(", attempt=");
            Serial.print(attempt);
            Serial.println(" tentando novamente em 2 segundos");
            delay(2000);
        }
    }
}

void connectWifi()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        return;
    }

    Serial.print("Conectando-se ao Wi-Fi");

    WiFi.begin(ssid, password);
    unsigned long startAttemptTime = millis();
    const unsigned long timeout = 10000;

    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < timeout)
    {
        delay(100);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("\nWi-Fi conectado!");
    }
    else
    {
        Serial.println("\nFalha ao conectar ao Wi-Fi.");
    }
}

void setup()
{
    Serial.begin(115200);

    // Configuração dos pinos
    pinMode(VALVE_OUT, OUTPUT);
    sensors.begin();
    pinMode(ULTRASOUND_ECHO_IN, INPUT);
    pinMode(ULTRASOUND_TRIG_OUT, OUTPUT);
    pinMode(PH_IN, INPUT);
    // pinMode(TDS_IN, INPUT);
    pinMode(TURBIDITY_SENSOR, INPUT);
}

void mqttLoop()
{
    // Verifica/Conecta WiFi
    connectWifi();
    // Verifica/Conecta MQTT
    connectMQTT();
}

float getTemperature()
{
    sensors.requestTemperatures();
    float temperatura = sensors.getTempCByIndex(0);
    return temperatura;
}

float getDistance()
{
    // Sensor ultrassônico distancia
    digitalWrite(ULTRASOUND_TRIG_OUT, LOW);
    delayMicroseconds(5);
    digitalWrite(ULTRASOUND_TRIG_OUT, HIGH);
    delayMicroseconds(10);
    digitalWrite(ULTRASOUND_TRIG_OUT, LOW);
    long duration = pulseIn(ULTRASOUND_ECHO_IN, HIGH);
    float distanceCm = duration * 0.0343 / 2.0;
    return distanceCm;
}

float getPH()
{
    int phIn = analogRead(PH_IN);
    // float phV = 3.3 * phIn /  4095.0; // Para calibração
    // ph 10 = 1.2V (1489 in)
    // ph 7 = 1.5V (1861 in) (calibrado no potenciometro metade de 3V porque o sensor tem saida 0-5 e está com divisor de voltagem pra 3v)
    // ph 4 = 1.8V (2233 in)
    int pHMapped = map(phIn, 1489, 2233, 100, 40);
    float pHValue = (float)pHMapped / 10;

    return pHValue;
}

// float getTDS() {
//     // Total dissolved solids
//     int tdsIn = analogRead(TDS_IN);
//     int tdsMapped = map(tdsIn, 0, 4095, 0, 330);
//     float tdsVoltagem = (float)tdsMapped / 100;

//     // Maior voltagem = mais sólidos dissolvidos.
//     // Na vida real, será necessário um algoritmo para medir a voltagem ao longo de um período de tempo e calcular o ppm.
//     // Porém, não encontramos este sensor a venda então usaremos por enquanto apenas o valor da voltagem simbolicamente.
//     // Referencia: https://randomnerdtutorials.com/esp32-tds-water-quality-sensor/

//     return tdsVoltagem;
// }

int getTurbidity()
{
    // Turbidez - NTU Nephelometric Turbidity Unit: MENOR voltagem = Maior NTU = mais turva.
    int turbidityIn = analogRead(TURBIDITY_SENSOR);
    int turbidityPercent = map(turbidityIn, 0, 1500, 100, 0);

    // Na vida real é necessária uma calibragem e calculo correto do NTU. Aqui, para o protótipo usamos um simples mapa linear de 100% turva a 0% turva.
    // https://www.electroniclinic.com/turbidity-sensor-with-arduino-for-water-quality-monitoring-turbidity-meter/
    return turbidityPercent < 0 ? 0 : turbidityPercent;
}

bool shouldCloseValve(float temperatura, float filledCm, float pHValue, int turbidityPercent)
{
    bool temperaturaAlta = (temperatura > temperaturaMaxima);
    bool temperaturaBaixa = (temperatura < temperaturaMinima);

    bool nivelBaixo = (filledCm < nivelMinimo);

    bool phAlto = (pHValue > pHMaximo);
    bool phBaixo = (pHValue < pHMinimo);

    // bool tdsAlto = (tdsVoltagem > TDSMaximo);

    bool turbidezAlta = (turbidityPercent > turbidezMaxima);

    // bool fechaValvula = temperaturaAlta || temperaturaBaixa || nivelBaixo || phAlto || phBaixo || tdsAlto || turbidezAlta; // TDS faltando
    bool alterado = temperaturaAlta || temperaturaBaixa || nivelBaixo || phAlto || phBaixo || turbidezAlta;

    bool fechaValvula = closeIfAltered ? alterado : !alterado;
    return fechaValvula;
}

void loop()
{
    mqttLoop();

    // Ler sensores
    float temperatura = getTemperature();

    float distanceCm = getDistance();
    float filledCm = CM_TO_BOTTOM - distanceCm;
    float litros = filledCm * LITERS_PER_CM;

    float pHValue = getPH();
    int turbidityPercent = getTurbidity();

    // Critérios de anomalia:
    bool fechaValvula = shouldCloseValve(temperatura, filledCm, pHValue, turbidityPercent);
    String valvulaTexto = fechaValvula ? "FECHADA" : "ABERTA";

    // Controle da válvula
    if (fechaValvula)
    {
        digitalWrite(VALVE_OUT, LOW);
    }
    else
    {
        digitalWrite(VALVE_OUT, HIGH);
    }

    // Resultados
    String report = "Temperatura: " + String(temperatura, 1) + "°C | Nivel: " + String(filledCm, 1) + "cm | Litros: " + String(litros, 1) + "L | pH: " + String(pHValue, 1) + " | Turbidez: " + String(turbidityPercent) + "% | Valvula: " + valvulaTexto;

    // Exibir valores no Serial
    Serial.println(report);

    // Enviar dados via MQTT
    client.publish("cisterna/report", report.c_str());

    client.publish("cisterna/temperatura", String(temperatura).c_str());
    client.publish("cisterna/nivel", String(filledCm).c_str());
    client.publish("cisterna/litros", String(litros).c_str());
    client.publish("cisterna/ph", String(pHValue).c_str());
    client.publish("cisterna/turbidez", String(turbidityPercent).c_str());
    client.publish("cisterna/valvula", valvulaTexto.c_str());

    delay(3000);
}
