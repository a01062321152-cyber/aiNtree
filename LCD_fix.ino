#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
  Serial.begin(9600);
  Wire.begin();

  // ─── 1. I2C 주소 스캔 ───────────────────────
  Serial.println(F("=== I2C 주소 스캔 ==="));
  bool found = false;
  for (byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print(F("✔ 발견! 주소: 0x"));
      Serial.println(addr, HEX);
      found = true;
    }
  }
  if (!found) Serial.println(F("✘ I2C 장치 없음 → 배선 확인"));
  Serial.println();

  // ─── 2. LCD 초기화 ──────────────────────────
  Serial.println(F("=== LCD 초기화 ==="));
  lcd.begin(16, 2);
  lcd.backlight();
  Serial.println(F("초기화 완료"));
  Serial.println();

  // ─── 3. 1행 테스트 ──────────────────────────
  Serial.println(F("=== 1행 테스트 ==="));
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("1234567890123456");  // 16칸 꽉 채우기
  Serial.println(F("1행: '1234567890123456' 출력"));
  delay(2000);

  // ─── 4. 2행 테스트 ──────────────────────────
  Serial.println(F("=== 2행 테스트 ==="));
  lcd.setCursor(0, 1);
  lcd.print("ABCDEFGHIJKLMNOP");  // 16칸 꽉 채우기
  Serial.println(F("2행: 'ABCDEFGHIJKLMNOP' 출력"));
  delay(2000);

  // ─── 5. 실제 출력 형식 테스트 ───────────────
  Serial.println(F("=== 실제 출력 테스트 ==="));
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("T:24C H:55%");
  lcd.setCursor(0, 1);
  lcd.print(" I'm happy~");
  Serial.println(F("실제 형식 출력 완료"));
  delay(2000);

  // ─── 6. 백라이트 점멸 테스트 ────────────────
  Serial.println(F("=== 백라이트 점멸 테스트 ==="));
  for (int i = 0; i < 3; i++) {
    lcd.noBacklight();
    Serial.println(F("백라이트 OFF"));
    delay(500);
    lcd.backlight();
    Serial.println(F("백라이트 ON"));
    delay(500);
  }

  Serial.println();
  Serial.println(F("=== 검사 완료 ==="));
  Serial.println(F("각 단계에서 LCD에 뭐가 보였는지 알려주세요!"));
}

void loop() {}