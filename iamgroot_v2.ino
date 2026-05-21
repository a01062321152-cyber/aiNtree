/*
  =============================================
  아이엠 그루트 (I Am Groot)
  팀: 아두이노 오브 갤럭시 (14조)
  =============================================
  사용 부품:
  - 토양 습도 센서 (Moisture)         → A0
  - 조도 센서 (MH Sensor Series)      → A1 (아날로그 출력 AO)
  - DHT11 온습도 센서                 → D7
  - LCD I2C (16x2)                    → SDA=A4, SCL=A5
  - LED 빨강                           → D9
  - LED 주황                           → D10
  - LED 초록                           → D11
  - 부저                               → D8
  =============================================
  필요 라이브러리:
  - LiquidCrystal_I2C  (Frank de Brabander)
  - DHT sensor library (Adafruit)
  =============================================
  ※ MH 조도 센서 특성:
    값이 작을수록 밝음, 클수록 어두움 (반비례)
    예) 한낮: 100~300 / 실내: 400~700 / 어두움: 800 이상

  ※ LED 상태 표시:
    🔴 빨강 - 위험  : 과습, 목마름, 더위, 추위
    🟠 주황 - 주의  : 어두움, 건조
    🟢 초록 - 정상  : 편안
*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

// ─── LCD 설정 (주소가 0x3F인 경우 변경) ──────
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ─── DHT11 설정 ──────────────────────────────
#define DHT_PIN     7
#define DHT_TYPE    DHT11
DHT dht(DHT_PIN, DHT_TYPE);

// ─── 핀 설정 ─────────────────────────────────
#define SOIL_PIN      A0   // 토양 습도 센서
#define LIGHT_PIN     A1   // MH 조도 센서 (아날로그 AO)
#define LED_RED        9   // LED 빨강 (위험)
#define LED_ORANGE    10   // LED 주황 (주의)
#define LED_GREEN     11   // LED 초록 (정상)
#define BUZZER_PIN     8   // 부저

// ─── 센서 임계값 (환경에 맞게 조정) ──────────
#define SOIL_DRY      600   // 토양값 이 이상 → 목말라요
#define SOIL_WET      300   // 토양값 이 이하 → 물 너무 많아요
#define LIGHT_DARK    800   // 조도값 이 이상 → 어두워요 (MH는 반비례!)
#define TEMP_HOT      28.0  // 이 이상이면 더워요
#define TEMP_COLD     15.0  // 이 이하이면 추워요
#define HUMID_DRY     30.0  // 공기 습도 이 이하 → 건조해요

// ─── 상태 정의 (우선순위 순서) ───────────────
enum State {
  OVERWATER,  // 0. 물이 너무 많아요   (가장 위험)
  THIRSTY,    // 1. 목말라요
  HOT,        // 2. 더워요
  COLD,       // 3. 추워요
  DARK,       // 4. 어두워요
  AIR_DRY,    // 5. 건조해요 (공기)
  HAPPY       // 6. 편안해요 (기본)
};

// ─── LED 등급 정의 ───────────────────────────
enum LedLevel {
  LED_DANGER,   // 빨강
  LED_WARNING,  // 주황
  LED_NORMAL    // 초록
};

State currentState = HAPPY;
State prevState    = HAPPY;
unsigned long lastCheck = 0;
const unsigned long CHECK_INTERVAL = 3000; // 3초마다 측정

// ─── 커스텀 캐릭터 (LCD CGRAM 0~7번) ─────────

// 행복한 얼굴 ^_^
byte happyFace[8] = {
  B00000,
  B01010,
  B01010,
  B00000,
  B10001,
  B01110,
  B00000,
  B00000
};

// 슬픈 얼굴 (목마름) ㅠㅠ
byte sadFace[8] = {
  B00000,
  B01010,
  B11011,
  B01010,
  B00000,
  B01110,
  B10001,
  B00000
};

// 더운 얼굴 (땀)
byte hotFace[8] = {
  B00100,
  B00100,
  B01010,
  B01010,
  B00000,
  B10001,
  B01110,
  B00000
};

// 추운 얼굴 (떠는 눈)
byte coldFace[8] = {
  B00000,
  B10101,
  B01010,
  B10101,
  B00000,
  B11111,
  B00000,
  B00000
};

// 어두움 얼굴 (감긴 눈)
byte darkFace[8] = {
  B00000,
  B00000,
  B11011,
  B00000,
  B00000,
  B01110,
  B00000,
  B00000
};

// 건조 얼굴 (입 벌림)
byte dryFace[8] = {
  B00000,
  B01010,
  B01010,
  B00000,
  B01110,
  B10001,
  B10001,
  B01110
};

// 놀란 얼굴 (과습) O_O
byte shockedFace[8] = {
  B00000,
  B01010,
  B11011,
  B01010,
  B00000,
  B01110,
  B01010,
  B01110
};

// ─── 초기 설정 ───────────────────────────────
void setup() {
  Serial.begin(9600);

  Serial.println();
  Serial.println(F("╔═════════════════════════════════════════╗"));
  Serial.println(F("║       I am Groot - System Start         ║"));
  Serial.println(F("║       팀: 아두이노 오브 갤럭시 (14조)    ║"));
  Serial.println(F("╚═════════════════════════════════════════╝"));
  Serial.println(F("센서 초기화 중..."));

  // ─ LED 핀 출력 설정
  pinMode(LED_RED,    OUTPUT);
  pinMode(LED_ORANGE, OUTPUT);
  pinMode(LED_GREEN,  OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  // 시작 시 모든 LED OFF
  digitalWrite(LED_RED,    LOW);
  digitalWrite(LED_ORANGE, LOW);
  digitalWrite(LED_GREEN,  LOW);

  // LCD 초기화
  lcd.begin();
  lcd.backlight();

  // 커스텀 캐릭터 등록 (7개)
  lcd.createChar(0, happyFace);
  lcd.createChar(1, sadFace);
  lcd.createChar(2, hotFace);
  lcd.createChar(3, coldFace);
  lcd.createChar(4, darkFace);
  lcd.createChar(5, dryFace);
  lcd.createChar(6, shockedFace);

  // DHT11 시작
  dht.begin();

  // 부팅 화면
  showBootScreen();
  delay(2000);

  Serial.println(F("센서 초기화 완료!"));
  Serial.println(F("측정을 시작합니다..."));
  Serial.println();
}

// ─── 메인 루프 ───────────────────────────────
void loop() {
  if (millis() - lastCheck >= CHECK_INTERVAL) {
    lastCheck = millis();

    int   soilValue  = analogRead(SOIL_PIN);
    int   lightValue = analogRead(LIGHT_PIN);
    float temp       = dht.readTemperature();
    float humid      = dht.readHumidity();

    if (isnan(temp) || isnan(humid)) {
      Serial.println(F("[!] DHT11 읽기 실패 - 다음 측정까지 대기"));
      return;
    }

    // 상태 판단 (우선순위: 과습 > 목마름 > 더위 > 추위 > 어두움 > 건조 > 편안)
    if (soilValue <= SOIL_WET) {
      currentState = OVERWATER;
    } else if (soilValue >= SOIL_DRY) {
      currentState = THIRSTY;
    } else if (temp >= TEMP_HOT) {
      currentState = HOT;
    } else if (temp <= TEMP_COLD) {
      currentState = COLD;
    } else if (lightValue >= LIGHT_DARK) {
      currentState = DARK;
    } else if (humid <= HUMID_DRY) {
      currentState = AIR_DRY;
    } else {
      currentState = HAPPY;
    }

    printLog(soilValue, lightValue, temp, humid, currentState);

    if (currentState != prevState) {
      printStateChange(prevState, currentState);
      if (currentState != HAPPY) playAlert(currentState);
      prevState = currentState;
    }

    updateLCD(currentState, temp, humid);
    updateLED(currentState);   // ← 개별 LED 업데이트
  }
}

// ─── 시리얼 로그 출력 ────────────────────────
void printLog(int soil, int light, float temp, float humid, State state) {
  unsigned long sec = millis() / 1000;

  Serial.println(F("┌─────────────────────────────────────────┐"));
  Serial.print(F("│ [I am Groot] 측정 #"));
  Serial.print(sec);
  Serial.println(F("초"));
  Serial.println(F("├─────────────────────────────────────────┤"));

  Serial.print(F("│ 🌱 토양 습도   : "));
  Serial.print(soil);
  Serial.print(F("\t["));
  Serial.print(getSoilStatus(soil));
  Serial.println(F("]"));

  Serial.print(F("│ ☀  조도        : "));
  Serial.print(light);
  Serial.print(F("\t["));
  Serial.print(getLightStatus(light));
  Serial.println(F("]"));

  Serial.print(F("│ 🌡  온도        : "));
  Serial.print(temp, 1);
  Serial.print(F("°C\t["));
  Serial.print(getTempStatus(temp));
  Serial.println(F("]"));

  Serial.print(F("│ 💧 공기 습도   : "));
  Serial.print(humid, 1);
  Serial.print(F("%\t["));
  Serial.print(getHumidStatus(humid));
  Serial.println(F("]"));

  Serial.println(F("├─────────────────────────────────────────┤"));
  Serial.print(F("│ ▶ 현재 감정    : "));
  Serial.println(stateToString(state));

  // LED 상태도 로그에 표시
  Serial.print(F("│ 💡 LED 상태    : "));
  Serial.println(getLedStatus(state));
  Serial.println(F("└─────────────────────────────────────────┘"));
  Serial.println();
}

// ─── 상태 변경 알림 로그 ─────────────────────
void printStateChange(State from, State to) {
  Serial.println(F("*** 상태 변경 ***"));
  Serial.print(F("    "));
  Serial.print(stateToString(from));
  Serial.print(F(" → "));
  Serial.println(stateToString(to));
  Serial.println();
}

// ─── 상태별 문자열 변환 ──────────────────────
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

// ─── LED 상태 문자열 (로그용) ─────────────────
const char* getLedStatus(State s) {
  switch (s) {
    case OVERWATER:
    case THIRSTY:
    case HOT:
    case COLD:      return "🔴 빨강 (위험)";
    case DARK:
    case AIR_DRY:   return "🟠 주황 (주의)";
    case HAPPY:     return "🟢 초록 (정상)";
  }
  return "알 수 없음";
}

// ─── 센서값별 상태 문자열 ────────────────────
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

// ─── LCD 표시 함수 ───────────────────────────
void updateLCD(State state, float temp, float humid) {
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
      lcd.setCursor(15, 1);
      lcd.write(byte(0));
      break;
    case THIRSTY:
      lcd.print(" Thirsty... ");
      lcd.setCursor(15, 1);
      lcd.write(byte(1));
      break;
    case HOT:
      lcd.print(" Too hot!   ");
      lcd.setCursor(15, 1);
      lcd.write(byte(2));
      break;
    case COLD:
      lcd.print(" Too cold!  ");
      lcd.setCursor(15, 1);
      lcd.write(byte(3));
      break;
    case DARK:
      lcd.print(" Dark zzz.. ");
      lcd.setCursor(15, 1);
      lcd.write(byte(4));
      break;
    case AIR_DRY:
      lcd.print(" Air dry... ");
      lcd.setCursor(15, 1);
      lcd.write(byte(5));
      break;
    case OVERWATER:
      lcd.print(" Too wet!   ");
      lcd.setCursor(15, 1);
      lcd.write(byte(6));
      break;
  }
}

// ─── LED 제어 (개별 LED 3개) ──────────────────
/*
  상태 → LED 등급 매핑
  ┌──────────────┬──────────┬────────────────────────────┐
  │ State        │ LED 색상 │ 이유                        │
  ├──────────────┼──────────┼────────────────────────────┤
  │ OVERWATER    │ 🔴 빨강  │ 뿌리 손상 위험, 즉시 조치  │
  │ THIRSTY      │ 🔴 빨강  │ 수분 부족, 즉시 조치       │
  │ HOT          │ 🔴 빨강  │ 고온 스트레스, 즉시 조치   │
  │ COLD         │ 🔴 빨강  │ 저온 스트레스, 즉시 조치   │
  │ DARK         │ 🟠 주황  │ 광합성 저하, 위치 조정 필요 │
  │ AIR_DRY      │ 🟠 주황  │ 건조 환경, 관리 필요       │
  │ HAPPY        │ 🟢 초록  │ 모든 환경 정상             │
  └──────────────┴──────────┴────────────────────────────┘
*/
void updateLED(State state) {
  // 먼저 모든 LED OFF
  digitalWrite(LED_RED,    LOW);
  digitalWrite(LED_ORANGE, LOW);
  digitalWrite(LED_GREEN,  LOW);

  // 상태에 따라 해당 LED만 ON
  switch (state) {
    case OVERWATER:
    case THIRSTY:
    case HOT:
    case COLD:
      digitalWrite(LED_RED, HIGH);      // 🔴 빨강: 위험
      break;

    case DARK:
    case AIR_DRY:
      digitalWrite(LED_ORANGE, HIGH);   // 🟠 주황: 주의
      break;

    case HAPPY:
      digitalWrite(LED_GREEN, HIGH);    // 🟢 초록: 정상
      break;
  }
}

// ─── 부저 알림음 ─────────────────────────────
void playAlert(State state) {
  switch (state) {
    case THIRSTY:
      tone(BUZZER_PIN, 880, 200); delay(300);
      tone(BUZZER_PIN, 880, 200); delay(300);
      break;
    case HOT:
      tone(BUZZER_PIN, 1500, 300); delay(400);
      break;
    case COLD:
      tone(BUZZER_PIN, 300, 400); delay(500);
      break;
    case DARK:
      tone(BUZZER_PIN, 500, 400); delay(500);
      break;
    case AIR_DRY:
      tone(BUZZER_PIN, 700, 300); delay(400);
      break;
    case OVERWATER:
      tone(BUZZER_PIN, 1200, 150); delay(200);
      tone(BUZZER_PIN, 1200, 150); delay(200);
      tone(BUZZER_PIN, 1200, 150); delay(200);
      break;
    default:
      break;
  }
  noTone(BUZZER_PIN);
}

// ─── 부팅 화면 ───────────────────────────────
void showBootScreen() {
  lcd.clear();
  lcd.setCursor(3, 0);
  lcd.print("I am Groot");
  lcd.setCursor(0, 1);
  lcd.print(" Loading...");
}