#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ================= LCD =================
#define SDA_LCD 21
#define SCL_LCD 22
LiquidCrystal_I2C lcd(0x27, 20, 4);

// ================= TURBIDITY =================
#define TURBIDITY_PIN 35   // ADC1 (AMAN WiFi)

void setup() {
  Serial.begin(115200);

  // ADC ESP32 full range 0–3.3V
  analogSetPinAttenuation(TURBIDITY_PIN, ADC_11db);

  // LCD INIT
  Wire.begin(SDA_LCD, SCL_LCD);
  lcd.init();
  lcd.backlight();
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("TURBIDITY SENSOR");
  delay(1500);
}

void loop() {
  int adc = analogRead(TURBIDITY_PIN);
  float voltage = adc * (3.3 / 4095.0);

  // ===== HITUNG NTU (ESTIMASI) =====
  float ntu;
  if (voltage < 0.5) {
    ntu = 3000; // sangat keruh
  } else {
    ntu = -1120.4 * voltage * voltage + 5742.3 * voltage - 4352.9;
    if (ntu < 0) ntu = 0;
  }

  // ===== STATUS AIR =====
  String status;
  if (ntu < 50) {
    status = "Jernih";
  } else if (ntu < 200) {
    status = "Agak Keruh";
  } else {
    status = "Keruh";
  }

  // ===== SERIAL =====
  Serial.print("ADC: ");
  Serial.print(adc);
  Serial.print(" | V: ");
  Serial.print(voltage, 3);
  Serial.print(" | NTU: ");
  Serial.print(ntu, 1);
  Serial.print(" | ");
  Serial.println(status);

  // ===== LCD =====
  lcd.setCursor(0, 0);
  lcd.print("TURBIDITY SENSOR  ");

  lcd.setCursor(0, 1);
  lcd.print("ADC : ");
  lcd.print(adc);
  lcd.print("     ");

  lcd.setCursor(0, 2);
  lcd.print("NTU : ");
  lcd.print(ntu, 1);
  lcd.print("        ");

  lcd.setCursor(0, 3);
  lcd.print("Status: ");
  lcd.print(status);
  lcd.print("   ");

  delay(1000);
}
