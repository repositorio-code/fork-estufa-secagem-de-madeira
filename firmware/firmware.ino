#include <WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"
#include <Wire.h>
#include <Adafruit_INA219.h>

// --- 1. CONFIGURAÇÕES DE REDE E MQTT ---
const char* ssid = "REDE";
const char* password = "SENHA_DA_REDE";

const char* mqtt_server = "IP_DA_MAQUINA";
const int mqtt_port = 1883;

const char* ID_DO_LOTE = "LOTE_TESTE_01";
const char* ID_DO_SENSOR = "ESTUFA_SENSOR_1";
const char* mqtt_topic_base = "Estufa";

// --- 2. CONFIGURAÇÕES DOS SENSORES ---
#define DHTPIN 33
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

Adafruit_INA219 ina219;

// --- 3. INTERVALO DE LEITURA ---
const long interval = 5000; // 5 segundos
unsigned long previousMillis = 0;

// --- 4. VARIÁVEIS DE LEITURA ---
float temperatura_c = 0.0;
float umidade_pct = 0.0;
float ph_valor = 7.0;
float bateria_pct = 0.0;

// --- 5. CONEXÃO MQTT ---
WiFiClient espClient;
PubSubClient client(espClient);

// --- FUNÇÕES DE CONEXÃO ---
void setup_wifi() {
    delay(10);
    Serial.println();
    Serial.print("Conectando-se a ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi conectado!");
    Serial.print("Endereço IP: ");
    Serial.println(WiFi.localIP());
}

void reconnect() {
    while (!client.connected()) {
        Serial.print("Tentando conexão MQTT...");
        if (client.connect(ID_DO_SENSOR)) {
            Serial.println("conectado");
        } else {
            Serial.print("falhou, rc=");
            Serial.print(client.state());
            Serial.println(" tentando novamente em 5 segundos");
            delay(5000);
        }
    }
}

// --- FUNÇÃO PARA MEDIR A BATERIA ---
float ler_bateria() {
    float busVoltage = ina219.getBusVoltage_V();
    float shuntVoltage = ina219.getShuntVoltage_mV();
    float batteryVoltage = busVoltage + (shuntVoltage / 1000.0);

    // Conversão simples: 8.4V = 100%, 6.0V = 0%
    float nivel = (batteryVoltage - 6.0) / (8.4 - 6.0) * 100.0;
    if (nivel > 100) nivel = 100;
    if (nivel < 0) nivel = 0;

    Serial.print("Tensão da bateria: ");
    Serial.print(batteryVoltage, 3);
    Serial.print(" V | Nível: ");
    Serial.print(nivel, 1);
    Serial.println(" %");

    return nivel;
}

// --- FUNÇÃO PARA LER SENSORES ---
void ler_sensores() {
    umidade_pct = dht.readHumidity();
    temperatura_c = dht.readTemperature();
    ph_valor = random(65, 75) / 10.0;

    if (isnan(umidade_pct) || isnan(temperatura_c)) {
        Serial.println("Falha ao ler o DHT11!");
        temperatura_c = 0.0;
        umidade_pct = 0.0;
        return;
    }

    bateria_pct = ler_bateria();

    Serial.print("T: ");
    Serial.print(temperatura_c);
    Serial.print("°C | U: ");
    Serial.print(umidade_pct);
    Serial.print("% | PH: ");
    Serial.print(ph_valor);
    Serial.print(" | Bateria: ");
    Serial.print(bateria_pct);
    Serial.println("%");
}

// --- FUNÇÃO PARA PUBLICAR NO MQTT ---
void publish_data() {
    if (temperatura_c == 0.0 && umidade_pct == 0.0) {
        Serial.println("Dados inválidos, não publicando.");
        return;
    }

    String payload = "{";
    payload += "\"lote_id\":\"" + String(ID_DO_LOTE) + "\",";
    payload += "\"temp_c\":" + String(temperatura_c) + ",";
    payload += "\"umidade_pct\":" + String(umidade_pct) + ",";
    payload += "\"ph_valor\":" + String(ph_valor) + ",";
    payload += "\"bateria_pct\":" + String(bateria_pct, 1) + ",";
    payload += "\"status\":\"OK\"";
    payload += "}";

    String topic = String(mqtt_topic_base) + "/" + String(ID_DO_LOTE) + "/sensor/" + String(ID_DO_SENSOR);

    Serial.print("Publicando no tópico: ");
    Serial.println(topic);
    Serial.println(payload);

    client.publish(topic.c_str(), payload.c_str());
}

// --- SETUP ---
void setup() {
    Serial.begin(115200);
    dht.begin();

    // Inicializa INA219
    if (!ina219.begin()) {
        Serial.println("Falha ao inicializar INA219! Verifique conexões.");
        while (1);
    }
    ina219.setCalibration_32V_2A();
    Serial.println("INA219 inicializado com sucesso!");

    setup_wifi();
    client.setServer(mqtt_server, mqtt_port);
}

// --- LOOP PRINCIPAL ---
void loop() {
    if (!client.connected()) {
        reconnect();
    }
    client.loop();

    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;
        ler_sensores();
        publish_data();
    }
}
