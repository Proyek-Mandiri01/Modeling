// ================= LCD I2C =================
#define SDA_LCD 21
#define SCL_LCD 22

// ================= PIN SENSOR =================
#define TEMP_PIN 23
#define PH_PIN 34
#define TURBIDITY_PIN 35

// Ultrasonic 1
#define TRIG_U1 27
#define ECHO_U1 26

// Ultrasonic 2
#define TRIG_U2 33
#define ECHO_U2 32

// ================= RELAY (ESP32 GPIO) =================
#define RELAY1 18   // IN1 -> D18
#define RELAY2 19   // IN2 -> D19
#define RELAY3 5    // IN3 -> D5

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
#include <PubSubClient.h>

// ================= LCD 20x4 =================
LiquidCrystal_I2C lcd(0x27, 20, 4);

// ================= Temperature =================
OneWire oneWire(TEMP_PIN);
DallasTemperature sensors(&oneWire);

// ================= TIMER =================
unsigned long lastUpdate = 0;
const unsigned long lcdInterval = 500;

// ================= WiFi =================
const char* ssid = "Marone";
const char* password = "bebekangsa";

// ================= MQTT =================
const char* mqtt_server = "103.151.63.80";
const int mqtt_port = 1860;

WiFiClient espClient;
PubSubClient client(espClient);

// ================= ULTRASONIC =================
long bacaUltrasonic(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 30000);
  if (duration == 0) return 0;

  return duration * 0.034 / 2; // cm
}

// ================= RELAY FUNCTION =================
void relayOn(int pin, const char* name) {
  digitalWrite(pin, LOW); // Active LOW
  Serial.println(String(name) + " ON");
}

void relayOff(int pin, const char* name) {
  digitalWrite(pin, HIGH);
  Serial.println(String(name) + " OFF");
}

// ================= MQTT CALLBACK =================
void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.print("Topic: ");
  Serial.print(topic);
  Serial.print(" | Pesan: ");
  Serial.println(message);

  if (String(topic) == "esp32/relay1") {
    message == "ON" ? relayOn(RELAY1, "Relay 1") : relayOff(RELAY1, "Relay 1");
  }
  else if (String(topic) == "esp32/relay2") {
    message == "ON" ? relayOn(RELAY2, "Relay 2") : relayOff(RELAY2, "Relay 2");
  }
  else if (String(topic) == "esp32/relay3") {
    message == "ON" ? relayOn(RELAY3, "Relay 3") : relayOff(RELAY3, "Relay 3");
  }
}

// ================= WiFi CONNECT =================
void setup_wifi() {
  Serial.print("Menghubungkan ke WiFi ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Terhubung");
  Serial.print("IP ESP32: ");
  Serial.println(WiFi.localIP());
}

// ================= MQTT RECONNECT =================
void reconnect() {
  while (!client.connected()) {
    Serial.print("Menghubungkan MQTT...");
    if (client.connect("ESP32-Aquarium")) {
      Serial.println(" TERHUBUNG");

      client.subscribe("esp32/relay1");
      client.subscribe("esp32/relay2");
      client.subscribe("esp32/relay3");
    } else {
      Serial.print("Gagal rc=");
      Serial.print(client.state());
      Serial.println(" ulangi 5 detik");
      delay(5000);
    }
  }
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);

  pinMode(TRIG_U1, OUTPUT);
  pinMode(ECHO_U1, INPUT);
  pinMode(TRIG_U2, OUTPUT);
  pinMode(ECHO_U2, INPUT);

  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  pinMode(RELAY3, OUTPUT);

  // Matikan semua relay saat start
  relayOff(RELAY1, "Relay 1");
  relayOff(RELAY2, "Relay 2");
  relayOff(RELAY3, "Relay 3");

  Wire.begin(SDA_LCD, SCL_LCD);
  lcd.init();
  lcd.backlight();
  lcd.clear();

  sensors.begin();

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

// ================= LOOP =================
void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // ===== BACA SENSOR =====
  int phRaw = analogRead(PH_PIN);
  int turbidity = analogRead(TURBIDITY_PIN);

  sensors.requestTemperatures();
  float suhu = sensors.getTempCByIndex(0);

  long u1 = bacaUltrasonic(TRIG_U1, ECHO_U1);
  long u2 = bacaUltrasonic(TRIG_U2, ECHO_U2);

  // ===== UPDATE LCD =====
  unsigned long now = millis();
  if (now - lastUpdate >= lcdInterval) {
    lastUpdate = now;

    lcd.setCursor(0, 0);
    lcd.print("pH   : ");
    lcd.print(phRaw);
    lcd.print("       ");

    lcd.setCursor(0, 1);
    lcd.print("Turb : ");
    lcd.print(turbidity);
    lcd.print("       ");

    lcd.setCursor(0, 2);
    lcd.print("Suhu : ");
    lcd.print(suhu, 1);
    lcd.print(" C     ");

    lcd.setCursor(0, 3);
    lcd.print("                    ");

    lcd.setCursor(0, 3);
    lcd.print("U1:");
    lcd.print(u1);
    lcd.print("cm");

    lcd.setCursor(12, 3);
    lcd.print("U2:");
    lcd.print(u2);
    lcd.print("cm");
  }
}
