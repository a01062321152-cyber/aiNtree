/*
  =============================================
  아이엠 그루트 (I Am Groot) v3
  팀: 아두이노 오브 갤럭시 (14조)
  =============================================
  사용 부품:
  - 토양 습도 센서 (Moisture)         → A0
  - 조도 센서 (MH Sensor Series)      → A1 (아날로그 AO)
  - DHT11 온습도 센서                 → D7
  - LCD I2C (16x2)                    → SDA=A4, SCL=A5
  - LED 빨강                           → D9
  - LED 주황                           → D10
  - LED 초록                           → D11
  - 부저                               → D8
  - 초음파 센서 (HC-SR04)             → TRIG=D5, ECHO=D6
  =============================================
  [v3 변경사항]
  - HC-SR04 초음파 센서 추가 (TRIG=D5, ECHO=D6)
  - 감지 거리(GREET_DIST_CM) 이내 접근 시 환영 메시지 표시
  - 환영 중에는 식물 상태 표시를 잠시 중단
  - 환영 내용 시리얼 모니터 출력
  =============================================
  [초음파 센서 연결]
  HC-SR04:
    VCC  → 5V
    GND  → GND
    TRIG → D5
    ECHO → D6
  =============================================
  필요 라이브러리:
  - LiquidCrystal_I2C  (Frank de Brabander)
  - DHT sensor library (Adafruit)
  =============================================
*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

// ─── LCD 설정 ────────────────────────────────
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ─── DHT11 설정 ──────────────────────────────
#define DHT_PIN     7
#define DHT_TYPE    DHT11
DHT dht(DHT_PIN, DHT_TYPE);

// ─── 핀 설정 ─────────────────────────────────
#define SOIL_PIN      A0
#define LIGHT_PIN     A1
#define LED_RED        9
#define LED_ORANGE    10
#define LED_GREEN     11
#define BUZZER_PIN     8
#define TRIG_PIN       5   // 초음파 TRIG
#define ECHO_PIN       6   // 초음파 ECHO

// ─── 센서 임계값 ──────────────────────────────
#define SOIL_DRY      600
#define SOIL_WET      300
#define LIGHT_DARK    800
#define TEMP_HOT      28.0
#define TEMP_COLD     15.0
#define HUMID_DRY     30.0

// ─── 초음파 설정 ──────────────────────────────
#define GREET_DIST_CM   20    // 이 거리(cm) 이하로 접근하면 환영
#define GREET_COOLDOWN  8000  // 환영 후 재감지 대기 시간 (ms) — 너무 자주 울리지 않게
#define GREET_SHOW_MS   3000  // 환영 메시지 표시 시간 (ms)

// ─── 상태 정의 ───────────────────────────────
enum State {
  OVERWATER,
  THIRSTY,
  HOT,
  COLD,
  DARK,
  AIR_DRY,
  HAPPY
};

State currentState  = HAPPY;
State prevState     = HAPPY;

unsigned long lastCheck     = 0;
unsigned long lastGreetTime = 0;   // 마지막 환영 시각
bool          greeting      = false;
unsigned long greetStart    = 0;

const unsigned long CHECK_INTERVAL = 3000;

// ─── 커스텀 캐릭터 ───────────────────────────
byte happyFace[8]   = { B00000,B01010,B01010,B00000,B10001,B01110,B00000,B00000 };
byte sadFace[8]     = { B00000,B01010,B11011,B01010,B00000,B01110,B10001,B00000 };
byte hotFace[8]     = { B00100,B00100,B01010,B01010,B00000,B10001,B01110,B00000 };
byte coldFace[8]    = { B00000,B10101,B01010,B10101,B00000,B11111,B00000,B00000 };
byte darkFace[8]    = { B00000,B00000,B11011,B00000,B00000,B01110,B00000,B00000 };
byte dryFace[8]     = { B00000,B01010,B01010,B00000,B01110,B10001,B10001,B01110 };
byte shockedFace[8] = { B00000,B01010,B11011,B01010,B00000,B01110,B01010,B01110 };

// ─── 환영 문구 목록 (랜덤 출력) ──────────────
const char* greetLines[][2] = {
  { "  Hello! :)     ", " I am Groot~    " },
  { " Welcome!       ", " Nice to see u! " },
  { "  Hi there~     ", " I am Groot!    " },
  { " Groot is happy!", "  Come closer~  " },
  { "  Oh! A visitor!", " I am Groot :D  " },
};
const int GREET_COUNT = 5;

// ─────────────────────────────────────────────
void setup() {
  Serial.begin(9600);

  Serial.println();
  Serial.println(F("╔═════════════════════════════════════════╗"));
  Serial.println(F("║     I am Groot v3 - System Start        ║"));
  Serial.println(F("║     팀: 아두이노 오브 갤럭시 (14조)      ║"));
  Serial.println(F("║     HC-SR04 초음파 센서 추가 버전        ║"));
  Serial.println(F("╚═════════════════════════════════════════╝"));

  pinMode(LED_RED,    OUTPUT);
  pinMode(LED_ORANGE, OUTPUT);
  pinMode(LED_GREEN,  OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(TRIG_PIN,   OUTPUT);
  pinMode(ECHO_PIN,   INPUT);

  digitalWrite(LED_RED,    LOW);
  digitalWrite(LED_ORANGE, LOW);
  digitalWrite(LED_GREEN,  LOW);
  digitalWrite(TRIG_PIN,   LOW);

  lcd.init();
  lcd.backlight();
  lcd.createChar(0, happyFace);
  lcd.createChar(1, sadFace);
  lcd.createChar(2, hotFace);
  lcd.createChar(3, coldFace);
  lcd.createChar(4, darkFace);
  lcd.createChar(5, dryFace);
  lcd.createChar(6, shockedFace);

  dht.begin();
  randomSeed(analogRead(A3));  // 랜덤 시드 (미연결 핀 노이즈 활용)

  showBootScreen();
  delay(2000);

  Serial.println(F("초기화 완료. 측정 시작합니다."));
  Serial.print(F("※ 환영 감지 거리: "));
  Serial.print(GREET_DIST_CM);
  Serial.println(F("cm 이하"));
  Serial.println();
}

// ─────────────────────────────────────────────
void loop() {
  unsigned long now = millis();

  // ── 1) 초음파 거리 측정 (매 루프마다) ──────
  long dist = measureDistance();

  // 환영 조건: 거리 이내 + 쿨다운 완료 + 현재 환영 중 아님
  if (dist > 0
      && dist <= GREET_DIST_CM
      && !greeting
      && (now - lastGreetTime >= GREET_COOLDOWN)) {

    startGreeting(dist, now);
  }

  // 환영 표시 시간이 지나면 원래 화면으로 복귀
  if (greeting && (now - greetStart >= GREET_SHOW_MS)) {
    greeting = false;
    // 즉시 식물 상태 화면 복원
    updateLCD(currentState,
              dht.readTemperature(),
              dht.readHumidity());
  }

  // ── 2) 식물 상태 측정 (3초 간격, 환영 중에도 측정은 계속) ──
  if (now - lastCheck >= CHECK_INTERVAL) {
    lastCheck = now;

    int   soilValue  = analogRead(SOIL_PIN);
    int   lightValue = analogRead(LIGHT_PIN);
    float temp       = dht.readTemperature();
    float humid      = dht.readHumidity();

    if (isnan(temp) || isnan(humid)) {
      Serial.println(F("[!] DHT11 읽기 실패 - 다음 측정까지 대기"));
      return;
    }

    if      (soilValue <= SOIL_WET)   currentState = OVERWATER;
    else if (soilValue >= SOIL_DRY)   currentState = THIRSTY;
    else if (temp      >= TEMP_HOT)   currentState = HOT;
    else if (temp      <= TEMP_COLD)  currentState = COLD;
    else if (lightValue >= LIGHT_DARK) currentState = DARK;
    else if (humid     <= HUMID_DRY)  currentState = AIR_DRY;
    else                              currentState = HAPPY;

    printLog(soilValue, lightValue, temp, humid, dist);

    if (currentState != prevState) {
      printStateChange(prevState, currentState);
      if (currentState != HAPPY) playAlert(currentState);
      prevState = currentState;
    }

    // 환영 중이 아닐 때만 LCD 갱신
    if (!greeting) {
      updateLCD(currentState, temp, humid);
    }
    updateLED(currentState);
  }
}

// ─── 초음파 거리 측정 ─────────────────────────
// 반환값: 거리(cm). 측정 실패 시 -1 반환
long measureDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // 최대 대기 30ms (약 500cm) → 너무 멀면 -1 반환
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) return -1;

  long cm = duration / 58;  // 음속 변환 (29us/cm × 2)
  return cm;
}

// ─── 환영 시작 ───────────────────────────────
void startGreeting(long dist, unsigned long now) {
  greeting      = true;
  greetStart    = now;
  lastGreetTime = now;

  int idx = random(GREET_COUNT);  // 랜덤 문구 선택

  // LCD 환영 메시지
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(greetLines[idx][0]);
  lcd.setCursor(0, 1);
  lcd.print(greetLines[idx][1]);

  // 환영 비프음 (경쾌하게 2음)
  tone(BUZZER_PIN, 1047, 120); delay(160);  // C6
  tone(BUZZER_PIN, 1319, 150); delay(200);  // E6
  noTone(BUZZER_PIN);

  // 시리얼 출력
  Serial.println(F("╔═════════════════════════════════════════╗"));
  Serial.println(F("║            👋 방문자 감지!              ║"));
  Serial.println(F("╠═════════════════════════════════════════╣"));
  Serial.print(F("║  감지 거리 : "));
  Serial.print(dist);
  Serial.println(F(" cm                        ║"));
  Serial.print(F("║  메시지    : "));

  // 공백 제거 후 시리얼 출력
  String msg = String(greetLines[idx][0]);
  msg.trim();
  Serial.print(msg);
  Serial.print(F(" / "));
  msg = String(greetLines[idx][1]);
  msg.trim();
  Serial.println(msg);

  Serial.println(F("╚═════════════════════════════════════════╝"));
  Serial.println();
}

// ─── LCD 식물 상태 표시 ──────────────────────
void updateLCD(State state, float temp, float humid) {
  if (isnan(temp) || isnan(humid)) return;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print((int)temp);
  lcd.print("C H:");
  lcd.print((int)humid);
  lcd.print("%");

  lcd.setCursor(0, 1);
  switch (state) {
    case HAPPY:
      lcd.print(" I'm happy~ ");
      lcd.setCursor(15, 1); lcd.write(byte(0)); break;
    case THIRSTY:
      lcd.print(" Thirsty... ");
      lcd.setCursor(15, 1); lcd.write(byte(1)); break;
    case HOT:
      lcd.print(" Too hot!   ");
      lcd.setCursor(15, 1); lcd.write(byte(2)); break;
    case COLD:
      lcd.print(" Too cold!  ");
      lcd.setCursor(15, 1); lcd.write(byte(3)); break;
    case DARK:
      lcd.print(" Dark zzz.. ");
      lcd.setCursor(15, 1); lcd.write(byte(4)); break;
    case AIR_DRY:
      lcd.print(" Air dry... ");
      lcd.setCursor(15, 1); lcd.write(byte(5)); break;
    case OVERWATER:
      lcd.print(" Too wet!   ");
      lcd.setCursor(15, 1); lcd.write(byte(6)); break;
  }
}

// ─── LED 제어 ────────────────────────────────
void updateLED(State state) {
  digitalWrite(LED_RED,    LOW);
  digitalWrite(LED_ORANGE, LOW);
  digitalWrite(LED_GREEN,  LOW);

  switch (state) {
    case OVERWATER: case THIRSTY: case HOT: case COLD:
      digitalWrite(LED_RED,    HIGH); break;
    case DARK: case AIR_DRY:
      digitalWrite(LED_ORANGE, HIGH); break;
    case HAPPY:
      digitalWrite(LED_GREEN,  HIGH); break;
  }
}

// ─── 부저 알림음 ─────────────────────────────
void playAlert(State state) {
  switch (state) {
    case THIRSTY:
      tone(BUZZER_PIN, 880, 200); delay(350);
      tone(BUZZER_PIN, 880, 200); delay(350);
      break;
    case HOT:
      tone(BUZZER_PIN, 1500, 300); delay(450); break;
    case COLD:
      tone(BUZZER_PIN, 300,  400); delay(550); break;
    case DARK:
      tone(BUZZER_PIN, 500,  400); delay(550); break;
    case AIR_DRY:
      tone(BUZZER_PIN, 700,  300); delay(450); break;
    case OVERWATER:
      tone(BUZZER_PIN, 1200, 150); delay(280);
      tone(BUZZER_PIN, 1200, 150); delay(280);
      tone(BUZZER_PIN, 1200, 150); delay(280);
      break;
    default: break;
  }
  noTone(BUZZER_PIN);
}

// ─── 시리얼 로그 ─────────────────────────────
void printLog(int soil, int light, float temp, float humid, long dist) {
  unsigned long sec = millis() / 1000;

  Serial.println(F("┌─────────────────────────────────────────┐"));
  Serial.print(F("│ [I am Groot] 측정 "));
  Serial.print(sec); Serial.println(F("초"));
  Serial.println(F("├─────────────────────────────────────────┤"));

  Serial.print(F("│ 토양 습도  : ")); Serial.print(soil);
  Serial.print(F("\t[")); Serial.print(getSoilStatus(soil)); Serial.println(F("]"));

  Serial.print(F("│ 조도       : ")); Serial.print(light);
  Serial.print(F("\t[")); Serial.print(getLightStatus(light)); Serial.println(F("]"));

  Serial.print(F("│ 온도       : ")); Serial.print(temp, 1);
  Serial.print(F("C\t[")); Serial.print(getTempStatus(temp)); Serial.println(F("]"));

  Serial.print(F("│ 공기 습도  : ")); Serial.print(humid, 1);
  Serial.print(F("%\t[")); Serial.print(getHumidStatus(humid)); Serial.println(F("]"));

  Serial.print(F("│ 초음파 거리: "));
  if (dist < 0) Serial.println(F("측정 불가 (범위 초과)"));
  else { Serial.print(dist); Serial.println(F(" cm")); }

  Serial.println(F("├─────────────────────────────────────────┤"));
  Serial.print(F("│ 현재 상태  : ")); Serial.println(stateToString(currentState));
  Serial.print(F("│ LED 상태   : ")); Serial.println(getLedStatus(currentState));
  Serial.println(F("└─────────────────────────────────────────┘"));
  Serial.println();
}

void printStateChange(State from, State to) {
  Serial.println(F("*** 상태 변경 ***"));
  Serial.print(F("    ")); Serial.print(stateToString(from));
  Serial.print(F(" -> ")); Serial.println(stateToString(to));
  Serial.println();
}

// ─── 문자열 헬퍼 ─────────────────────────────
const char* stateToString(State s) {
  switch (s) {
    case HAPPY:     return "편안해요 ^_^";
    case THIRSTY:   return "목말라요 (물 부족)";
    case HOT:       return "더워요 (고온)";
    case COLD:      return "추워요 (저온)";
    case DARK:      return "어두워요 (빛 부족)";
    case AIR_DRY:   return "건조해요 (공기 건조)";
    case OVERWATER: return "물이 너무 많아요 (과습)";
  }
  return "알 수 없음";
}

const char* getLedStatus(State s) {
  switch (s) {
    case OVERWATER: case THIRSTY: case HOT: case COLD: return "빨강 (위험)";
    case DARK: case AIR_DRY:                           return "주황 (주의)";
    case HAPPY:                                        return "초록 (정상)";
  }
  return "알 수 없음";
}

const char* getSoilStatus(int v) {
  if (v <= SOIL_WET)  return "과습";
  if (v >= SOIL_DRY)  return "건조";
  return "적정";
}

const char* getLightStatus(int v) {
  if (v >= LIGHT_DARK) return "어두움";
  if (v <= 300)        return "매우 밝음";
  return "적정";
}

const char* getTempStatus(float v) {
  if (v >= TEMP_HOT)  return "높음";
  if (v <= TEMP_COLD) return "낮음";
  return "적정";
}

const char* getHumidStatus(float v) {
  if (v <= HUMID_DRY) return "건조";
  if (v >= 70.0)      return "매우 습함";
  return "적정";
}

// ─── 부팅 화면 ───────────────────────────────
void showBootScreen() {
  lcd.clear();
  lcd.setCursor(3, 0);
  lcd.print("I am Groot");
  lcd.setCursor(0, 1);
  lcd.print("  Loading...  ");
}
