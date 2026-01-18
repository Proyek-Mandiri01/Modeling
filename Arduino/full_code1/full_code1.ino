#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
#include <PubSubClient.h>

/* ========= PIN ========= */
#define SDA_LCD 21
#define SCL_LCD 22

#define PH_PIN 34
#define TURBIDITY_PIN 35
#define TEMP_PIN 23

#define TRIG_U1 27
#define ECHO_U1 26

#define RELAY_PUMP   18   // Pompa buang air
#define RELAY_VALVE  19   // Solenoid isi air

/* ========= LEVEL AIR (CM) ========= */
#define AIR_PENUH_CM   4
#define AIR_SURUT_CM  15

/* ========= LCD ========= */
LiquidCrystal_I2C lcd(0x27, 20, 4);

/* ========= TEMP ========= */
OneWire oneWire(TEMP_PIN);
DallasTemperature tempSensor(&oneWire);

/* ========= WIFI ========= */
const char* ssid = "Marone";
const char* password = "bebekangsa";

/* ========= MQTT ========= */
const char* mqtt_server = "103.151.63.80";
const int mqtt_port = 1899;

const char* topic_cmd    = "esp32/aquarium/cmd";
const char* topic_status = "esp32/aquarium/status";

WiFiClient espClient;
PubSubClient client(espClient);

/* ========= STATE ========= */
bool pumpOn = false;
bool valveOn = false;
bool autoMode = true;

String phStatus, tempStatus, turbStatus;

/* ========= SYSTEM STATE ========= */
enum State { IDLE, DRAIN, FILL, DONE };
State systemState = IDLE;

/* ========= TIMER ========= */
unsigned long lcdTimer = 0;
bool lcdMode = false;

/* ========= ULTRASONIC ========= */
long bacaUltrasonic(int trig, int echo) {
  digitalWrite(trig, LOW); delayMicroseconds(2);
  digitalWrite(trig, HIGH); delayMicroseconds(10);
  digitalWrite(trig, LOW);

  long dur = pulseIn(echo, HIGH, 30000);
  if (dur == 0) return -1;   // error
  return dur * 0.034 / 2;
}

/* ========= RELAY ========= */
void pumpON()  { digitalWrite(RELAY_PUMP, LOW);  pumpOn = true; }
void pumpOFF() { digitalWrite(RELAY_PUMP, HIGH); pumpOn = false; }

void valveON()  { digitalWrite(RELAY_VALVE, LOW);  valveOn = true; }
void valveOFF() { digitalWrite(RELAY_VALVE, HIGH); valveOn = false; }

/* ========= MQTT CALLBACK ========= */
void callback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (int i = 0; i < length; i++) msg += (char)payload[i];

  if (String(topic) == topic_cmd) {
    if (msg == "PUMP_ON") pumpON();
    if (msg == "PUMP_OFF") pumpOFF();
    if (msg == "VALVE_ON") valveON();
    if (msg == "VALVE_OFF") valveOFF();
    if (msg == "AUTO_ON") autoMode = true;
    if (msg == "AUTO_OFF") autoMode = false;
  }
}

/* ========= WIFI ========= */
void setupWiFi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);
}

/* ========= MQTT ========= */
void reconnectMQTT() {
  while (!client.connected()) {
    client.connect("ESP32-AQUARIUM");
    delay(500);
  }
  client.subscribe(topic_cmd);
}

/* ========= SETUP ========= */
void setup() {
  Serial.begin(115200);

  pinMode(TRIG_U1, OUTPUT);
  pinMode(ECHO_U1, INPUT);

  pinMode(RELAY_PUMP, OUTPUT);
  pinMode(RELAY_VALVE, OUTPUT);

  pumpOFF();
  valveOFF();

  Wire.begin(SDA_LCD, SCL_LCD);
  lcd.init();
  lcd.backlight();

  tempSensor.begin();

  setupWiFi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

/* ========= LOOP ========= */
void loop() {
  if (!client.connected()) reconnectMQTT();
  client.loop();

  /* ===== SENSOR ===== */
  int phRaw = analogRead(PH_PIN);
  int turb  = analogRead(TURBIDITY_PIN);

  tempSensor.requestTemperatures();
  float suhu = tempSensor.getTempCByIndex(0);

  long levelAir = bacaUltrasonic(TRIG_U1, ECHO_U1);
  if (levelAir < 0) return;   // sensor error

  /* ===== STATUS SENSOR ===== */
  phStatus   = (phRaw < 2000) ? "RENDAH" :
               (phRaw > 3000) ? "TINGGI" : "NORMAL";

  tempStatus = (suhu < 18) ? "DINGIN" :
               (suhu > 26) ? "PANAS" : "NORMAL";

  turbStatus = (turb > 1500) ? "KERUH" : "NORMAL";

  /* ===== STATE MACHINE (TURBIDITY ONLY) ===== */
  if (autoMode) {
    switch (systemState) {

      case IDLE:
        if (turbStatus == "KERUH") {
          Serial.println(">> AIR KERUH - MULAI KURAS");
          pumpON();
          valveOFF();
          systemState = DRAIN;
        }
        break;

      case DRAIN:
        if (levelAir >= AIR_SURUT_CM) {
          pumpOFF();
          Serial.println(">> AIR SURUT - ISI AIR");
          valveON();
          systemState = FILL;
        }
        break;

      case FILL:
        if (levelAir <= AIR_PENUH_CM) {
          valveOFF();
          Serial.println(">> AIR PENUH - SELESAI");
          systemState = DONE;
        }
        break;

      case DONE:
        systemState = IDLE;
        break;
    }
  }

  /* ===== LCD ===== */
  if (millis() - lcdTimer >= 5000) {
    lcdTimer = millis();
    lcdMode = !lcdMode;
    lcd.clear();
  }

  if (!lcdMode) {
    lcd.setCursor(0,0);
    lcd.print("pH:");
    lcd.print(phRaw);
    lcd.print(" Tur:");
    lcd.print(turb);

    lcd.setCursor(0,1);
    lcd.print("Temp:");
    lcd.print(suhu,1);
    lcd.print("C");

    lcd.setCursor(0,2);
    lcd.print("Level:");
    lcd.print(levelAir);
    lcd.print("cm");

    lcd.setCursor(0,3);
    lcd.print("Pump:");
    lcd.print(pumpOn?"ON ":"OFF");
    lcd.print("Valve:");
    lcd.print(valveOn?"ON":"OFF");
  } 
  else {
    lcd.setCursor(0,0);
    lcd.print("STATUS SENSOR");

    lcd.setCursor(0,1);
    lcd.print("PH   : ");
    lcd.print(phStatus);

    lcd.setCursor(0,2);
    lcd.print("SUHU : ");
    lcd.print(tempStatus);

    lcd.setCursor(0,3);
    lcd.print("TURB : ");
    lcd.print(turbStatus);
  }

  /* ===== MQTT PAYLOAD ===== */
  String payload = "{";
  payload += "\"ph_status\":\"" + phStatus + "\",";
  payload += "\"temp_status\":\"" + tempStatus + "\",";
  payload += "\"turb_status\":\"" + turbStatus + "\",";
  payload += "\"water_level_cm\":" + String(levelAir) + ",";
  payload += "\"state\":\"" +
    String(systemState==IDLE?"IDLE":
           systemState==DRAIN?"DRAIN":
           systemState==FILL?"FILL":"DONE") + "\"}";
  
  client.publish(topic_status, payload.c_str());

  Serial.println(payload);
  delay(1000);
}
