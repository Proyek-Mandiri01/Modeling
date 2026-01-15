#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
#include <PubSubClient.h>

/* ========== PIN ========== */
#define SDA_LCD 21
#define SCL_LCD 22

#define TEMP_PIN 23
#define PH_PIN 34
#define TURBIDITY_PIN 35

#define TRIG_U1 27
#define ECHO_U1 26
#define TRIG_U2 33
#define ECHO_U2 32

#define RELAY_PUMP 18   // ACTIVE LOW

/* ========== LCD ========== */
LiquidCrystal_I2C lcd(0x27, 20, 4);

/* ========== TEMP ========== */
OneWire oneWire(TEMP_PIN);
DallasTemperature tempSensor(&oneWire);

/* ========== WIFI ========== */
const char* ssid = "Marone";
const char* password = "bebekangsa";

/* ========== MQTT ========== */
const char* mqtt_server = "103.151.63.80";
const int mqtt_port = 1860;

const char* topic_cmd    = "esp32/aquarium/cmd";
const char* topic_status = "esp32/aquarium/status";

WiFiClient espClient;
PubSubClient client(espClient);

/* ========== STATE ========== */
bool pumpOn = false;
String statusNotif = "NORMAL";

/* ========== ULTRASONIC ========== */
long bacaUltrasonic(int trig, int echo) {
  digitalWrite(trig, LOW); delayMicroseconds(2);
  digitalWrite(trig, HIGH); delayMicroseconds(10);
  digitalWrite(trig, LOW);
  long dur = pulseIn(echo, HIGH, 30000);
  if (dur == 0) return 0;
  return dur * 0.034 / 2;
}

/* ========== RELAY ========== */
void pumpON() {
  digitalWrite(RELAY_PUMP, LOW);
  pumpOn = true;
}
void pumpOFF() {
  digitalWrite(RELAY_PUMP, HIGH);
  pumpOn = false;
}

/* ========== MQTT CALLBACK ========== */
void callback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (int i = 0; i < length; i++) msg += (char)payload[i];

  if (String(topic) == topic_cmd) {
    if (msg.indexOf("ON") >= 0) pumpON();
    if (msg.indexOf("OFF") >= 0) pumpOFF();
  }
}

/* ========== WIFI ========== */
void setupWiFi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);
}

/* ========== MQTT ========== */
void reconnectMQTT() {
  while (!client.connected()) {
    client.connect("ESP32-AQUARIUM");
    delay(500);
  }
  client.subscribe(topic_cmd);
}

/* ========== SETUP ========== */
void setup() {
  Serial.begin(115200);

  pinMode(TRIG_U1, OUTPUT);
  pinMode(ECHO_U1, INPUT);
  pinMode(TRIG_U2, OUTPUT);
  pinMode(ECHO_U2, INPUT);

  pinMode(RELAY_PUMP, OUTPUT);
  pumpOFF();

  Wire.begin(SDA_LCD, SCL_LCD);
  lcd.init();
  lcd.backlight();

  tempSensor.begin();

  setupWiFi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

/* ========== LOOP ========== */
void loop() {
  if (!client.connected()) reconnectMQTT();
  client.loop();

  int phRaw = analogRead(PH_PIN);
  int turb  = analogRead(TURBIDITY_PIN);

  tempSensor.requestTemperatures();
  float suhu = tempSensor.getTempCByIndex(0);

  long u1 = bacaUltrasonic(TRIG_U1, ECHO_U1);
  long u2 = bacaUltrasonic(TRIG_U2, ECHO_U2);

  /* ===== NOTIF ===== */
  if (phRaw < 2000) statusNotif = "PH RENDAH";
  else if (phRaw > 3000) statusNotif = "PH TINGGI";
  else if (suhu < 18) statusNotif = "AIR DINGIN";
  else if (suhu > 26) statusNotif = "AIR PANAS";
  else if (turb > 1500) statusNotif = "AIR KERUH";
  else statusNotif = "NORMAL";

  /* ===== LCD ===== */
  lcd.setCursor(0,0);
  lcd.print("pH:");
  lcd.print(phRaw);
  lcd.setCursor(10,0);
  lcd.print("Tur:");
  lcd.print(turb);
  lcd.print("  ");

  lcd.setCursor(0,1);
  lcd.print("Temp:");
  lcd.print(suhu,1);
  lcd.print(" C     ");

  lcd.setCursor(0,2);
  lcd.print("U1:");
  lcd.print(u1);
  lcd.print("cm  U2:");
  lcd.print(u2);
  lcd.print("cm ");

  lcd.setCursor(0,3);
  lcd.print(statusNotif);
  lcd.print("       ");

  /* ===== SERIAL ===== */
  Serial.println("==== AQUARIUM ====");
  Serial.println("pH ADC : " + String(phRaw));
  Serial.println("Turb   : " + String(turb));
  Serial.println("Temp   : " + String(suhu));
  Serial.println("U1: " + String(u1) + " | U2: " + String(u2));
  Serial.println("POMPA  : " + String(pumpOn ? "ON" : "OFF"));
  Serial.println(statusNotif);

  /* ===== MQTT STATUS ===== */
  String payload = "{";
  payload += "\"ph\":" + String(phRaw) + ",";
  payload += "\"turbidity\":" + String(turb) + ",";
  payload += "\"temperature\":" + String(suhu,1) + ",";
  payload += "\"u1\":" + String(u1) + ",";
  payload += "\"u2\":" + String(u2) + ",";
  payload += "\"pump\":\"" + String(pumpOn ? "ON" : "OFF") + "\",";
  payload += "\"status\":\"" + statusNotif + "\"}";
  client.publish(topic_status, payload.c_str());

  delay(1000);
}
