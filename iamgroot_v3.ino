/*
  =============================================
  아이엠 그루트 (I Am Groot) v3
  팀: 아두이노 오브 갤럭시 (14조)
  =============================================
  [v3 변경사항]
  - 표정 2칸으로 확장 (14~15번 위치)
  - 표정 디테일 개선
  - 환영 메시지에 웃는 표정 추가
  =============================================
*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

#define DHT_PIN     7
#define DHT_TYPE    DHT11
DHT dht(DHT_PIN, DHT_TYPE);

#define SOIL_PIN      A0
#define LIGHT_PIN     A1
#define LED_RED        9
#define LED_ORANGE    10
#define LED_GREEN     11
#define BUZZER_PIN     8
#define TRIG_PIN       5
#define ECHO_PIN       6

#define SOIL_DRY      600
#define SOIL_WET      300
#define LIGHT_DARK    800
#define TEMP_HOT      28.0
#define TEMP_COLD     15.0
#define HUMID_DRY     30.0

#define GREET_DIST_CM   20
#define GREET_COOLDOWN  8000
#define GREET_SHOW_MS   3000

enum State { OVERWATER, THIRSTY, HOT, COLD, DARK, AIR_DRY, HAPPY };

State currentState  = HAPPY;
State prevState     = HAPPY;
unsigned long lastCheck     = 0;
unsigned long lastGreetTime = 0;
bool          greeting      = false;
unsigned long greetStart    = 0;
const unsigned long CHECK_INTERVAL = 3000;

// ─── 커스텀 캐릭터 (좌/우 2칸 구성) ──────────
/*
  CGRAM 슬롯 배정:
  0 = 행복 좌    1 = 행복 우
  2 = 슬픔 좌    3 = 슬픔 우
  4 = 더위 좌    5 = 더위 우
  6 = 추위 좌    7 = 추위 우

  ※ CGRAM은 0~7번 총 8슬롯만 가능
     → 어두움/건조/과습은 행복/슬픔 슬롯 재활용
       (해당 상태에선 happy/sad 문자를 다르게 씀)
*/

// 😊 행복 좌 (눈 ^, 볼)
byte happyL[8] = {
  B00011,  //    **
  B00110,  //   **
  B01100,  //  **
  B01000,  //  *
  B01011,  //  * **
  B00110,  //   **
  B00001,  //      *
  B00000
};

// 😊 행복 우 (눈 ^, 볼)
byte happyR[8] = {
  B11000,  // **
  B01100,  //  **
  B00110,  //   **
  B00010,  //    *
  B11010,  // ** *
  B01100,  //  **
  B10000,  // *
  B00000
};

// 😢 슬픔 좌 (눈 ㅠ, 눈물)
byte sadL[8] = {
  B00011,  //    **
  B00110,  //   **
  B01100,  //  **
  B00001,  //      *
  B01010,  //  * *
  B00110,  //   **
  B00001,  //      *
  B00000
};

// 😢 슬픔 우
byte sadR[8] = {
  B11000,  // **
  B01100,  //  **
  B00110,  //   **
  B10000,  // *
  B01010,  //  * *
  B01100,  //  **
  B10000,  // *
  B00000
};

// 🥵 더위 좌 (땀방울 + 찡그린 눈)
byte hotL[8] = {
  B00010,  //    *
  B00101,  //   * *
  B00010,  //    *
  B01100,  //  **
  B00100,  //   *
  B01011,  //  * **
  B00110,  //   **
  B00000
};

// 🥵 더위 우
byte hotR[8] = {
  B01000,  //  *
  B10100,  // * *
  B01000,  //  *
  B00110,  //   **
  B00100,  //   *
  B11010,  // ** *
  B01100,  //  **
  B00000
};

// 🥶 추위 좌 (떨리는 눈 + 이빨)
byte coldL[8] = {
  B01010,  //  * *
  B10101,  // * * *
  B01010,  //  * *
  B00000,
  B01111,  //  ****
  B01010,  //  * *
  B01111,  //  ****
  B00000
};

// 🥶 추위 우
byte coldR[8] = {
  B01010,  //  * *
  B10101,  // * * *
  B01010,  //  * *
  B00000,
  B11110,  // **** 
  B01010,  //  * *
  B11110,  // ****
  B00000
};

// ─── 상태별 표정 출력 함수 ───────────────────
// CGRAM 슬롯이 8개뿐이라 4쌍(행복/슬픔/더위/추위)만 등록
// 어두움→슬픔, 건조→슬픔, 과습→더위 슬롯 재활용

void printFace(State state, int row) {
  switch (state) {
    case HAPPY:
      lcd.setCursor(14, row); lcd.write(byte(0));
      lcd.setCursor(15, row); lcd.write(byte(1));
      break;
    case THIRSTY:
    case DARK:
    case AIR_DRY:
      lcd.setCursor(14, row); lcd.write(byte(2));
      lcd.setCursor(15, row); lcd.write(byte(3));
      break;
    case HOT:
    case OVERWATER:
      lcd.setCursor(14, row); lcd.write(byte(4));
      lcd.setCursor(15, row); lcd.write(byte(5));
      break;
    case COLD:
      lcd.setCursor(14, row); lcd.write(byte(6));
      lcd.setCursor(15, row); lcd.write(byte(7));
      break;
  }
}

// ─── 환영 문구 (2행 각 14글자, 나머지 2칸은 표정) ──
// 14자 맞추기: "  Hello! :)   " (14자)
const char* greetLines[][2] = {
  { "  Hello! :)   ", " I am Groot~  " },
  { " Welcome!     ", " Nice 2 see u " },
  { "  Hi there~   ", " I am Groot!  " },
  { " Groot happy! ", "  Come close~ " },
  { " Oh!A visitor!", " I am Groot:D " },
};
const int GREET_COUNT = 5;

// ─────────────────────────────────────────────
void setup() {
  Serial.begin(9600);

  Serial.println(F("╔═════════════════════════════════════════╗"));
  Serial.println(F("║     I am Groot v3 - System Start        ║"));
  Serial.println(F("║     팀: 아두이노 오브 갤럭시 (14조)      ║"));
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

  lcd.begin(16, 2);
  lcd.backlight();

  // CGRAM 등록 (8슬롯 전부 사용)
  lcd.createChar(0, happyL);
  lcd.createChar(1, happyR);
  lcd.createChar(2, sadL);
  lcd.createChar(3, sadR);
  lcd.createChar(4, hotL);
  lcd.createChar(5, hotR);
  lcd.createChar(6, coldL);
  lcd.createChar(7, coldR);

  dht.begin();
  randomSeed(analogRead(A3));

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
  long dist = measureDistance();

  if (dist > 0
      && dist <= GREET_DIST_CM
      && !greeting
      && (now - lastGreetTime >= GREET_COOLDOWN)) {
    startGreeting(dist, now);
  }

  if (greeting && (now - greetStart >= GREET_SHOW_MS)) {
    greeting = false;
    updateLCD(currentState, dht.readTemperature(), dht.readHumidity());
  }

  if (now - lastCheck >= CHECK_INTERVAL) {
    lastCheck = now;

    int   soilValue  = analogRead(SOIL_PIN);
    int   lightValue = analogRead(LIGHT_PIN);
    float temp       = dht.readTemperature();
    float humid      = dht.readHumidity();

    if (isnan(temp) || isnan(humid)) {
      Serial.println(F("[!] DHT11 읽기 실패"));
      return;
    }

    if      (soilValue  <= SOIL_WET)    currentState = OVERWATER;
    else if (soilValue  >= SOIL_DRY)    currentState = THIRSTY;
    else if (temp       >= TEMP_HOT)    currentState = HOT;
    else if (temp       <= TEMP_COLD)   currentState = COLD;
    else if (lightValue >= LIGHT_DARK)  currentState = DARK;
    else if (humid      <= HUMID_DRY)   currentState = AIR_DRY;
    else                                currentState = HAPPY;

    printLog(soilValue, lightValue, temp, humid, dist);

    if (currentState != prevState) {
      printStateChange(prevState, currentState);
      if (currentState != HAPPY) playAlert(currentState);
      prevState = currentState;
    }

    if (!greeting) updateLCD(currentState, temp, humid);
    updateLED(currentState);
  }
}

// ─── 초음파 거리 측정 ────────────────────────
long measureDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) return -1;
  return duration / 58;
}

// ─── 환영 시작 (웃는 표정 2칸 포함) ─────────
void startGreeting(long dist, unsigned long now) {
  greeting      = true;
  greetStart    = now;
  lastGreetTime = now;

  int idx = random(GREET_COUNT);

  lcd.clear();

  // 1행: 문구(14자) + 웃는 표정 좌우
  lcd.setCursor(0, 0);
  lcd.print(greetLines[idx][0]);   // 14자
  lcd.setCursor(14, 0); lcd.write(byte(0));  // 행복 좌
  lcd.setCursor(15, 0); lcd.write(byte(1));  // 행복 우

  // 2행: 문구(14자) + 웃는 표정 좌우
  lcd.setCursor(0, 1);
  lcd.print(greetLines[idx][1]);   // 14자
  lcd.setCursor(14, 1); lcd.write(byte(0));  // 행복 좌
  lcd.setCursor(15, 1); lcd.write(byte(1));  // 행복 우

  // 경쾌한 환영음
  tone(BUZZER_PIN, 1047, 120); delay(160);
  tone(BUZZER_PIN, 1319, 150); delay(200);
  noTone(BUZZER_PIN);

  Serial.println(F("╔═════════════════════════════════════════╗"));
  Serial.println(F("║            👋 방문자 감지!              ║"));
  Serial.println(F("╠═════════════════════════════════════════╣"));
  Serial.print(F("║  감지 거리 : "));
  Serial.print(dist);
  Serial.println(F(" cm"));

  String msg = String(greetLines[idx][0]); msg.trim();
  Serial.print(F("║  메시지    : ")); Serial.print(msg);
  Serial.print(F(" / "));
  msg = String(greetLines[idx][1]); msg.trim();
  Serial.println(msg);

  Serial.println(F("╚═════════════════════════════════════════╝"));
  Serial.println();
}

// ─── LCD 식물 상태 표시 ──────────────────────
void updateLCD(State state, float temp, float humid) {
  if (isnan(temp) || isnan(humid)) return;

  lcd.clear();

  // 1행: 온습도 (최대 13자) + 여백
  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print((int)temp);
  lcd.print("C H:");
  lcd.print((int)humid);
  lcd.print("%");

  // 2행: 상태 메시지 (13자) + 표정 2칸
  lcd.setCursor(0, 1);
  switch (state) {
    case HAPPY:     lcd.print(" I'm happy~  "); break;  // 13자
    case THIRSTY:   lcd.print(" Thirsty...  "); break;
    case HOT:       lcd.print(" Too hot!    "); break;
    case COLD:      lcd.print(" Too cold!   "); break;
    case DARK:      lcd.print(" Dark zzz... "); break;
    case AIR_DRY:   lcd.print(" Air dry...  "); break;
    case OVERWATER: lcd.print(" Too wet!    "); break;
  }

  // 2행 14~15칸에 표정 출력
  printFace(state, 1);
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
      tone(BUZZER_PIN, 880,  200); delay(350);
      tone(BUZZER_PIN, 880,  200); delay(350); break;
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
      tone(BUZZER_PIN, 1200, 150); delay(280); break;
    default: break;
  }
  noTone(BUZZER_PIN);
}

// ─── 시리얼 로그 ─────────────────────────────
void printLog(int soil, int light, float temp, float humid, long dist) {
  unsigned long sec = millis() / 1000;

  Serial.println(F("┌─────────────────────────────────────────┐"));
  Serial.print(F("│ [I am Groot] 측정 "));
  Serial.print(sec / 60); Serial.print(F("분 "));
  Serial.print(sec % 60); Serial.println(F("초"));
  Serial.println(F("├─────────────────────────────────────────┤"));

  Serial.print(F("│ 토양 습도  : ")); Serial.print(soil);
  Serial.print(F("\t[")); Serial.print(getSoilStatus(soil)); Serial.println(F("]"));

  Serial.print(F("│ 조도       : ")); Serial.print(light);
  Serial.print(F("\t[")); Serial.print(getLightStatus(light)); Serial.println(F("]"));

  Serial.print(F("│ 온도       : ")); Serial.print(temp, 1);
  Serial.print(F("C\t[")); Serial.print(getTempStatus(temp)); Serial.println(F("]"));

  Serial.print(F("│ 공기 습도  : ")); Serial.print(humid, 1);
  Serial.print(F("%\t[")); Serial.print(getHumidStatus(humid)); Serial.println(F("]"));

  Serial.print(F("│ 초음파     : "));
  if (dist < 0) Serial.println(F("측정 불가"));
  else { Serial.print(dist); Serial.println(F(" cm")); }

  Serial.println(F("├─────────────────────────────────────────┤"));
  Serial.print(F("│ 현재 상태  : ")); Serial.println(stateToString(currentState));
  Serial.print(F("│ LED 상태   : ")); Serial.println(getLedStatus(currentState));
  Serial.println(F("└─────────────────────────────────────────┘"));
  Serial.println();
}

void printStateChange(State from, State to) {
  Serial.println(F("*** 상태 변경 ***"));
  Serial.print(stateToString(from));
  Serial.print(F(" -> "));
  Serial.println(stateToString(to));
  Serial.println();
}

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

void showBootScreen() {
  lcd.clear();
  lcd.setCursor(3, 0);
  lcd.print("I am Groot");
  lcd.setCursor(0, 1);
  lcd.print("  Loading...  ");
}
