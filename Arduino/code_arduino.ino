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

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// ================= LCD 20x4 =================
LiquidCrystal_I2C lcd(0x27, 20, 4);

// ================= Temperature =================
OneWire oneWire(TEMP_PIN);
DallasTemperature sensors(&oneWire);

// ================= TIMER =================
unsigned long lastUpdate = 0;
const unsigned long lcdInterval = 500;

// ================= ULTRASONIC =================
long bacaUltrasonic(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 30000);
  if (duration == 0) return 0;

  return duration * 0.034 / 2; // HASIL DALAM CM
}

void setup() {
  Serial.begin(115200);

  pinMode(TRIG_U1, OUTPUT);
  pinMode(ECHO_U1, INPUT);
  pinMode(TRIG_U2, OUTPUT);
  pinMode(ECHO_U2, INPUT);

  Wire.begin(SDA_LCD, SCL_LCD);
  lcd.init();
  lcd.backlight();
  lcd.clear();

  sensors.begin();
}

void loop() {
  // ===== BACA SENSOR =====
  int phRaw = analogRead(PH_PIN);
  int turbidity = analogRead(TURBIDITY_PIN);

  sensors.requestTemperatures();
  float suhu = sensors.getTempCByIndex(0);

  long u1 = bacaUltrasonic(TRIG_U1, ECHO_U1); // cm
  long u2 = bacaUltrasonic(TRIG_U2, ECHO_U2); // cm

  // ===== UPDATE LCD =====
  unsigned long now = millis();
  if (now - lastUpdate >= lcdInterval) {
    lastUpdate = now;

    // Baris 1
    lcd.setCursor(0, 0);
    lcd.print("pH   : ");
    lcd.print(phRaw);
    lcd.print("       ");

    // Baris 2
    lcd.setCursor(0, 1);
    lcd.print("Turb : ");
    lcd.print(turbidity);
    lcd.print("       ");

    // Baris 3
    lcd.setCursor(0, 2);
    lcd.print("Suhu : ");
    lcd.print(suhu, 1);
    lcd.print(" C     ");

    // ===== BARIS 4 (DIBERSIHKAN DULU!) =====
    lcd.setCursor(0, 3);
    lcd.print("                    "); // 20 spasi (CLEAR BARIS)

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
