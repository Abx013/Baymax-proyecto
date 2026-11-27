#include <Wire.h>
#include <U8g2lib.h>
#include <Adafruit_MLX90614.h>
#include <Adafruit_VL53L1X.h>

// ================================
// BAYMAX - MODO EMERGENCIA EXPO
// Version ligera, sin WiFi ni DFPlayer
// ================================

// TB6612
#define PWMA 25
#define AIN1 26
#define AIN2 27
#define PWMB 33
#define BIN1 32
#define BIN2 14

// HC-SR04
#define TRIG 18
#define ECHO 19

// Botones
#define BTN_UP 4
#define BTN_OK 5
#define BTN_DOWN 13

// Buzzer
#define BUZZER 17

// I2C
#define SDA_PIN 21
#define SCL_PIN 22

float ALTURA_SENSOR_CM = 200.0;

U8G2_SSD1309_128X64_NONAME0_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);
Adafruit_MLX90614 mlx;
Adafruit_VL53L1X tof;

bool mlxOK = false;
bool tofOK = false;
bool motores = false;

const char* menu[] = {"Temperatura", "Altura", "Distancia", "Motores"};
const int N = 4;
int opcion = 0;

void beep() {
  ledcWriteTone(BUZZER, 2200);
  delay(60);
  ledcWriteTone(BUZZER, 0);
}

void pantalla(const char* a, const char* b) {
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x10_tr);
  oled.drawStr(4, 22, a);
  oled.drawStr(4, 43, b);
  oled.sendBuffer();
}

void menuOLED() {
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x10_tr);
  for (int i = 0; i < N; i++) {
    int y = 14 + i * 13;
    oled.drawStr(2, y, i == opcion ? ">" : " ");
    oled.drawStr(12, y, menu[i]);
  }
  oled.sendBuffer();
}

float distancia() {
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);

  unsigned long t = pulseIn(ECHO, HIGH, 30000);
  if (t == 0) return -1;
  return t * 0.0343 / 2.0;
}

void detener() {
  digitalWrite(PWMA, LOW);
  digitalWrite(PWMB, LOW);
  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, LOW);
  digitalWrite(BIN1, LOW);
  digitalWrite(BIN2, LOW);
  motores = false;
}

void avanzar() {
  digitalWrite(AIN1, HIGH);
  digitalWrite(AIN2, LOW);
  digitalWrite(BIN1, HIGH);
  digitalWrite(BIN2, LOW);
  digitalWrite(PWMA, HIGH);
  digitalWrite(PWMB, HIGH);
  motores = true;
}

void ejecutar() {
  beep();

  if (opcion == 0) {
    if (!mlxOK) {
      pantalla("TEMPERATURA", "Sensor no disponible");
      delay(1200);
      return;
    }

    Wire.setClock(100000);
    float t = mlx.readObjectTempC();

    char txt[24];
    snprintf(txt, sizeof(txt), "%.1f C", t);
    pantalla("TEMPERATURA", txt);
    delay(1600);
  }

  else if (opcion == 1) {
    if (!tofOK) {
      pantalla("ALTURA", "Conecta el poste");
      delay(1200);
      return;
    }

    unsigned long inicio = millis();
    while (!tof.dataReady()) {
      if (millis() - inicio > 1500) {
        pantalla("ALTURA", "Sin lectura");
        delay(1200);
        return;
      }
    }

    int16_t mm = tof.distance();
    tof.clearInterrupt();

    float altura = ALTURA_SENSOR_CM - (mm / 10.0);

    char txt[24];
    snprintf(txt, sizeof(txt), "%.1f cm", altura);
    pantalla("ALTURA", txt);
    delay(1600);
  }

  else if (opcion == 2) {
    float d = distancia();

    char txt[24];
    if (d < 0) {
      snprintf(txt, sizeof(txt), "Sin lectura");
    } else {
      snprintf(txt, sizeof(txt), "%.1f cm", d);
    }

    pantalla("DISTANCIA", txt);
    delay(1400);
  }

  else if (opcion == 3) {
    if (motores) {
      detener();
      pantalla("MOTORES", "DETENIDOS");
    } else {
      avanzar();
      pantalla("MOTORES", "AVANZANDO");
    }
    delay(900);
  }

  menuOLED();
}

void setup() {
  Serial.begin(115200);

  pinMode(PWMA, OUTPUT);
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(PWMB, OUTPUT);
  pinMode(BIN1, OUTPUT);
  pinMode(BIN2, OUTPUT);
  detener();

  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);

  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_OK, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);

  ledcAttach(BUZZER, 2500, 8);
  ledcWriteTone(BUZZER, 0);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);

  oled.setBusClock(100000);
  oled.begin();
  pantalla("BAYMAX", "INICIANDO");

  mlxOK = mlx.begin();
  Wire.setClock(100000);

  Wire.beginTransmission(0x29);
  if (Wire.endTransmission() == 0) {
    if (tof.begin(0x29, &Wire) && tof.startRanging()) {
      tof.setTimingBudget(50);
      tofOK = true;
    }
  }

  delay(500);
  menuOLED();
}

void loop() {
  if (digitalRead(BTN_UP) == LOW) {
    opcion--;
    if (opcion < 0) opcion = N - 1;
    menuOLED();
    beep();
    delay(180);
    while (digitalRead(BTN_UP) == LOW) delay(5);
  }

  if (digitalRead(BTN_DOWN) == LOW) {
    opcion++;
    if (opcion >= N) opcion = 0;
    menuOLED();
    beep();
    delay(180);
    while (digitalRead(BTN_DOWN) == LOW) delay(5);
  }

  if (digitalRead(BTN_OK) == LOW) {
    ejecutar();
    delay(180);
    while (digitalRead(BTN_OK) == LOW) delay(5);
  }

  if (motores) {
    float d = distancia();
    if (d > 0 && d <= 20.0) {
      detener();
      beep();
      pantalla("OBSTACULO", "MOTORES DETENIDOS");
      delay(1200);
      menuOLED();
    }
  }

  delay(5);
}
