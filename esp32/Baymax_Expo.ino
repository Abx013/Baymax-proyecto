#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <Adafruit_MLX90614.h>
#include <Adafruit_VL53L1X.h>
#include <DFRobotDFPlayerMini.h>

// =====================================================
// BAYMAX - FIRMWARE FINAL DE EXPO
// ESP32 Dev Module
// Botones: ARRIBA / OK / ABAJO
// El interruptor fisico NO va a un GPIO:
// corta/da energia de las 4 AA hacia VM del TB6612.
// =====================================================

// ---------- WIFI ----------
const char* AP_SSID = "Baymax-Expo";
const char* AP_PASS = "BaymaxExpo2026";
WebServer server(80);

// ---------- TB6612FNG ----------
// Cableado REAL probado
const int PWMA = 25;
const int AIN1 = 26;
const int AIN2 = 27;
const int PWMB = 33;
const int BIN1 = 32;
const int BIN2 = 14;
// STBY esta conectado fisicamente a 3.3V.

// ---------- BUZZER PASIVO ----------
const int BUZZER_PIN = 17;

// ---------- DFPLAYER ----------
const int DF_RX = 16; // ESP32 RX <- TX DFPlayer
const int DF_TX = 23; // ESP32 TX -> 1k -> RX DFPlayer
HardwareSerial DFSerial(2);
DFRobotDFPlayerMini player;
bool dfOK = false;

// ---------- I2C ----------
const int SDA_PIN = 21;
const int SCL_PIN = 22;

Adafruit_MLX90614 mlx = Adafruit_MLX90614();
Adafruit_VL53L1X tof;
U8G2_SSD1309_128X64_NONAME0_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);

bool mlxOK = false;
bool tofOK = false;

// ---------- HC-SR04 ----------
const int TRIG_PIN = 18;
const int ECHO_PIN = 19; // ECHO -> 1k -> GPIO19; desde GPIO19 -> 2k -> GND
const float DISTANCIA_FRENO_CM = 20.0;

// ---------- BOTONERA ----------
const int BTN_ARRIBA = 4;
const int BTN_OK     = 5;
const int BTN_ABAJO  = 13;

// ---------- ALTURA ----------
// IMPORTANTE: cambia 200.0 por la altura REAL desde el piso
// hasta la cara del VL53L1X cuando el poste quede armado.
float ALTURA_SENSOR_CM = 200.0;

// ---------- VARIABLES ----------
float temperaturaC = NAN;
float distanciaCm = NAN;
float alturaCm = NAN;
float pesoKg = 60.0;  // se captura desde bascula comercial
float imc = NAN;

String movimiento = "detenido";
unsigned long ultimoChequeoObstaculo = 0;

// ---------- MENU ----------
const char* menuItems[] = {
  "Temperatura",
  "Altura",
  "IMC",
  "Distancia",
  "Estado"
};
const int MENU_COUNT = 5;
int menuIndex = 0;

enum ModoUI {
  UI_MENU,
  UI_PESO
};
ModoUI modoUI = UI_MENU;

// =====================================================
// UTILIDADES
// =====================================================

bool i2cPresente(uint8_t direccion) {
  Wire.setClock(100000);
  Wire.beginTransmission(direccion);
  return Wire.endTransmission() == 0;
}

void pantalla2(const char* linea1, const char* linea2) {
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x10_tr);
  oled.drawStr(4, 22, linea1);
  oled.drawStr(4, 43, linea2);
  oled.sendBuffer();
}

void pantallaValor(const char* titulo, float valor, const char* unidad, uint8_t decimales = 1) {
  char numero[16];
  char linea[24];

  dtostrf(valor, 5, decimales, numero);
  snprintf(linea, sizeof(linea), "%s %s", numero, unidad);

  oled.clearBuffer();
  oled.setFont(u8g2_font_6x10_tr);
  oled.drawStr(4, 14, titulo);
  oled.setFont(u8g2_font_ncenB14_tr);
  oled.drawStr(6, 43, linea);
  oled.sendBuffer();
}

void beep(int frecuencia = 2200, int duracion = 110) {
  ledcWriteTone(BUZZER_PIN, frecuencia);
  delay(duracion);
  ledcWriteTone(BUZZER_PIN, 0);
}

// =====================================================
// MOTORES
// =====================================================

void detenerMotores() {
  digitalWrite(PWMA, LOW);
  digitalWrite(PWMB, LOW);
  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, LOW);
  digitalWrite(BIN1, LOW);
  digitalWrite(BIN2, LOW);
  movimiento = "detenido";
}

void adelante() {
  digitalWrite(AIN1, HIGH);
  digitalWrite(AIN2, LOW);
  digitalWrite(BIN1, HIGH);
  digitalWrite(BIN2, LOW);
  digitalWrite(PWMA, HIGH);
  digitalWrite(PWMB, HIGH);
  movimiento = "adelante";
}

void atras() {
  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, HIGH);
  digitalWrite(BIN1, LOW);
  digitalWrite(BIN2, HIGH);
  digitalWrite(PWMA, HIGH);
  digitalWrite(PWMB, HIGH);
  movimiento = "atras";
}

void izquierda() {
  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, HIGH);
  digitalWrite(BIN1, HIGH);
  digitalWrite(BIN2, LOW);
  digitalWrite(PWMA, HIGH);
  digitalWrite(PWMB, HIGH);
  movimiento = "izquierda";
}

void derecha() {
  digitalWrite(AIN1, HIGH);
  digitalWrite(AIN2, LOW);
  digitalWrite(BIN1, LOW);
  digitalWrite(BIN2, HIGH);
  digitalWrite(PWMA, HIGH);
  digitalWrite(PWMB, HIGH);
  movimiento = "derecha";
}

void mover(const String& accion) {
  if (accion == "adelante") adelante();
  else if (accion == "atras") atras();
  else if (accion == "izquierda") izquierda();
  else if (accion == "derecha") derecha();
  else detenerMotores();
}

// =====================================================
// HC-SR04
// =====================================================

float leerDistancia() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  unsigned long duracion = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duracion == 0) return NAN;

  return duracion * 0.0343f / 2.0f;
}

void revisarObstaculo() {
  if (movimiento == "detenido") return;
  if (millis() - ultimoChequeoObstaculo < 120) return;

  ultimoChequeoObstaculo = millis();
  distanciaCm = leerDistancia();

  if (!isnan(distanciaCm) && distanciaCm <= DISTANCIA_FRENO_CM) {
    detenerMotores();
    beep(1200, 220);

    if (dfOK) {
      player.playMp3Folder(7); // 0007.mp3 = Obstaculo detectado
    }

    pantalla2("OBSTACULO", "MOTORES DETENIDOS");
    Serial.print("Obstaculo a ");
    Serial.print(distanciaCm);
    Serial.println(" cm");
  }
}

// =====================================================
// TEMPERATURA
// =====================================================

bool prepararMLX() {
  if (!i2cPresente(0x5A)) {
    mlxOK = false;
    return false;
  }

  if (!mlxOK) {
    Wire.setClock(100000);
    mlxOK = mlx.begin();
    Wire.setClock(100000);
  }

  return mlxOK;
}

void accionTemperatura() {
  beep();

  if (!prepararMLX()) {
    pantalla2("TEMPERATURA", "Sensor no detectado");
    delay(1800);
    return;
  }

  if (dfOK) player.playMp3Folder(3); // Midiendo temperatura

  Wire.setClock(100000);
  temperaturaC = mlx.readObjectTempC();

  if (isnan(temperaturaC)) {
    pantalla2("TEMPERATURA", "Error de lectura");
  } else {
    pantallaValor("TEMPERATURA", temperaturaC, "C", 1);
  }

  delay(2200);
}

// =====================================================
// VL53L1X / ALTURA
// =====================================================

bool prepararToF() {
  if (!i2cPresente(0x29)) {
    tofOK = false;
    return false;
  }

  if (!tofOK) {
    Wire.setClock(100000);

    if (!tof.begin(0x29, &Wire)) {
      tofOK = false;
      return false;
    }

    if (!tof.startRanging()) {
      tofOK = false;
      return false;
    }

    tof.setTimingBudget(50);
    tofOK = true;
  }

  return true;
}

bool medirAlturaReal() {
  if (!prepararToF()) {
    pantalla2("ALTURA", "Conecta poste ToF");
    return false;
  }

  Wire.setClock(100000);

  unsigned long inicio = millis();
  while (!tof.dataReady()) {
    if (millis() - inicio > 1800) {
      pantalla2("ALTURA", "Sin lectura");
      tofOK = false;
      return false;
    }
    delay(5);
  }

  int16_t distanciaMM = tof.distance();
  tof.clearInterrupt();

  if (distanciaMM <= 0) {
    pantalla2("ALTURA", "Error de lectura");
    return false;
  }

  float distanciaCabezaCm = distanciaMM / 10.0f;
  alturaCm = ALTURA_SENSOR_CM - distanciaCabezaCm;

  if (alturaCm <= 0) {
    pantalla2("ALTURA", "Revisa altura poste");
    return false;
  }

  return true;
}

void accionAltura() {
  beep();

  if (dfOK) player.playMp3Folder(4); // Midiendo peso y altura

  if (medirAlturaReal()) {
    pantallaValor("ALTURA", alturaCm, "cm", 1);
  }

  delay(2500);
}

// =====================================================
// IMC CON BASCULA EXTERNA
// =====================================================

void dibujarPeso() {
  char pesoTxt[18];
  dtostrf(pesoKg, 5, 1, pesoTxt);

  oled.clearBuffer();
  oled.setFont(u8g2_font_6x10_tr);
  oled.drawStr(2, 12, "PESO DE LA BASCULA");
  oled.drawStr(2, 24, "ARRIBA/ABAJO cambia");
  oled.setFont(u8g2_font_ncenB14_tr);
  oled.drawStr(20, 47, pesoTxt);
  oled.setFont(u8g2_font_6x10_tr);
  oled.drawStr(82, 47, "kg");
  oled.drawStr(25, 62, "OK = CALCULAR");
  oled.sendBuffer();
}

void iniciarIMC() {
  beep();

  if (isnan(alturaCm) || alturaCm <= 0) {
    pantalla2("IMC", "Mide altura primero");
    delay(1800);
    return;
  }

  modoUI = UI_PESO;
  if (pesoKg < 20 || pesoKg > 200) pesoKg = 60.0;
  dibujarPeso();
}

void calcularIMC() {
  float alturaM = alturaCm / 100.0f;
  if (alturaM <= 0) return;

  imc = pesoKg / (alturaM * alturaM);

  if (dfOK) player.playMp3Folder(5); // Calculando IMC

  pantallaValor("IMC", imc, "", 1);
  delay(2600);

  // Solo muestra el valor numerico por ser un prototipo educativo.
  // No clasifica el resultado como diagnostico.
  if (dfOK) player.playMp3Folder(6); // Evaluacion terminada
}

// =====================================================
// MENU OLED
// =====================================================

void dibujarMenu() {
  modoUI = UI_MENU;

  oled.clearBuffer();
  oled.setFont(u8g2_font_6x10_tr);

  for (int i = 0; i < MENU_COUNT; i++) {
    int y = 11 + i * 12;
    oled.drawStr(2, y, (i == menuIndex) ? ">" : " ");
    oled.drawStr(12, y, menuItems[i]);
  }

  oled.sendBuffer();
}

void accionDistancia() {
  beep();
  distanciaCm = leerDistancia();

  if (isnan(distanciaCm)) {
    pantalla2("DISTANCIA", "Sin lectura");
  } else {
    pantallaValor("DISTANCIA", distanciaCm, "cm", 1);
  }

  delay(1800);
}

void accionEstado() {
  char linea1[24];
  char linea2[24];

  snprintf(linea1, sizeof(linea1), "MLX:%s TOF:%s",
           i2cPresente(0x5A) ? "OK" : "--",
           i2cPresente(0x29) ? "OK" : "--");

  snprintf(linea2, sizeof(linea2), "WiFi:%s", AP_SSID);

  pantalla2(linea1, linea2);
  delay(2200);
}

void ejecutarMenu() {
  switch (menuIndex) {
    case 0: accionTemperatura(); break;
    case 1: accionAltura(); break;
    case 2: iniciarIMC(); break;
    case 3: accionDistancia(); break;
    case 4: accionEstado(); break;
  }

  if (modoUI == UI_MENU) dibujarMenu();
}

// =====================================================
// BOTONES
// =====================================================

void manejarBotones() {
  if (digitalRead(BTN_ARRIBA) == LOW) {
    if (modoUI == UI_MENU) {
      menuIndex--;
      if (menuIndex < 0) menuIndex = MENU_COUNT - 1;
      dibujarMenu();
    } else if (modoUI == UI_PESO) {
      pesoKg += 1.0;
      if (pesoKg > 200) pesoKg = 200;
      dibujarPeso();
    }

    beep(2600, 45);
    delay(170);
    while (digitalRead(BTN_ARRIBA) == LOW) delay(5);
  }

  if (digitalRead(BTN_ABAJO) == LOW) {
    if (modoUI == UI_MENU) {
      menuIndex++;
      if (menuIndex >= MENU_COUNT) menuIndex = 0;
      dibujarMenu();
    } else if (modoUI == UI_PESO) {
      pesoKg -= 1.0;
      if (pesoKg < 20) pesoKg = 20;
      dibujarPeso();
    }

    beep(2000, 45);
    delay(170);
    while (digitalRead(BTN_ABAJO) == LOW) delay(5);
  }

  if (digitalRead(BTN_OK) == LOW) {
    beep(2300, 60);

    if (modoUI == UI_MENU) {
      ejecutarMenu();
    } else if (modoUI == UI_PESO) {
      calcularIMC();
      dibujarMenu();
    }

    delay(170);
    while (digitalRead(BTN_OK) == LOW) delay(5);
  }
}

// =====================================================
// PAGINA LOCAL DEL ESP32
// =====================================================

const char PAGE[] PROGMEM = R"HTML(
<!doctype html>
<html lang="es">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Baymax Expo</title>
<style>
body{font-family:system-ui;background:#eef4fb;color:#142033;margin:0;padding:18px}
main{max-width:720px;margin:auto;background:#fff;border-radius:20px;padding:20px}
h1{margin-top:0}.card{background:#f5f8fc;border-radius:14px;padding:14px;margin:10px 0}
.grid{display:grid;grid-template-columns:repeat(2,1fr);gap:8px}
button,input{font:inherit;padding:12px;border-radius:10px}
button{border:0;background:#2563d8;color:#fff;font-weight:700}
.stop{background:#b43d55}.muted{color:#66778b;font-size:.86rem}
input{border:1px solid #ccd7e4;width:120px}
</style>
</head>
<body>
<main>
<h1>Baymax · Expo</h1>
<p class="muted">El interruptor fisico debe estar en ON para que los motores reciban energia.</p>

<div class="card" id="datos">Cargando...</div>

<div class="card">
<h3>Movimiento</h3>
<div class="grid">
<button onclick="m('adelante')">Adelante</button>
<button onclick="m('atras')">Atras</button>
<button onclick="m('izquierda')">Izquierda</button>
<button onclick="m('derecha')">Derecha</button>
<button class="stop" onclick="m('detener')">DETENER</button>
</div>
</div>

<div class="card">
<h3>Evaluacion</h3>
<button onclick="accion('/api/temperatura')">Temperatura</button>
<button onclick="accion('/api/altura')">Altura</button>
<p>Peso de bascula externa:</p>
<input id="peso" type="number" min="20" max="200" step="0.1" value="60">
<button onclick="guardarPeso()">Guardar peso / IMC</button>
</div>

<script>
async function leer(){
  try{
    const r=await fetch('/api/datos',{cache:'no-store'});
    const d=await r.json();
    document.getElementById('datos').innerHTML=
      'Temperatura: '+d.temperatura+' C<br>'+
      'Altura: '+d.altura+' cm<br>'+
      'Peso: '+d.peso+' kg<br>'+
      'IMC: '+d.imc+'<br>'+
      'Distancia: '+d.distancia+' cm<br>'+
      'Movimiento: '+d.movimiento;
  }catch(e){}
}
function m(a){fetch('/api/movimiento?accion='+a).then(leer)}
function accion(u){fetch(u).then(()=>setTimeout(leer,500))}
function guardarPeso(){
  const p=document.getElementById('peso').value;
  fetch('/api/peso?kg='+encodeURIComponent(p)).then(leer);
}
setInterval(leer,1500);leer();
</script>
</main>
</body>
</html>
)HTML";

// =====================================================
// API WEB
// =====================================================

String numeroJSON(float v, uint8_t dec = 1) {
  if (isnan(v)) return "null";
  return String(v, dec);
}

void apiDatos() {
  distanciaCm = leerDistancia();

  String json = "{";
  json += "\"temperatura\":" + numeroJSON(temperaturaC, 1) + ",";
  json += "\"altura\":" + numeroJSON(alturaCm, 1) + ",";
  json += "\"peso\":" + numeroJSON(pesoKg, 1) + ",";
  json += "\"imc\":" + numeroJSON(imc, 1) + ",";
  json += "\"distancia\":" + numeroJSON(distanciaCm, 1) + ",";
  json += "\"movimiento\":\"" + movimiento + "\"";
  json += "}";

  server.send(200, "application/json", json);
}

void apiMovimiento() {
  mover(server.arg("accion"));
  server.send(200, "text/plain", "OK");
}

void apiTemperatura() {
  accionTemperatura();
  dibujarMenu();
  server.send(200, "text/plain", "OK");
}

void apiAltura() {
  accionAltura();
  dibujarMenu();
  server.send(200, "text/plain", "OK");
}

void apiPeso() {
  if (!server.hasArg("kg")) {
    server.send(400, "text/plain", "Falta kg");
    return;
  }

  float nuevoPeso = server.arg("kg").toFloat();
  if (nuevoPeso < 20 || nuevoPeso > 200) {
    server.send(400, "text/plain", "Peso fuera de rango");
    return;
  }

  pesoKg = nuevoPeso;

  if (!isnan(alturaCm) && alturaCm > 0) {
    float alturaM = alturaCm / 100.0f;
    imc = pesoKg / (alturaM * alturaM);
  }

  server.send(200, "text/plain", "OK");
}

void apiBuzzer() {
  beep();
  server.send(200, "text/plain", "OK");
}

void apiAudio() {
  if (!dfOK) {
    server.send(503, "text/plain", "DFPlayer no disponible");
    return;
  }

  int pista = server.arg("pista").toInt();
  if (pista < 1 || pista > 8) {
    server.send(400, "text/plain", "Pista invalida");
    return;
  }

  player.playMp3Folder(pista);
  server.send(200, "text/plain", "OK");
}

// =====================================================
// SETUP
// =====================================================

void setup() {
  Serial.begin(115200);

  // Motores
  pinMode(PWMA, OUTPUT);
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(PWMB, OUTPUT);
  pinMode(BIN1, OUTPUT);
  pinMode(BIN2, OUTPUT);
  detenerMotores();

  // HC-SR04
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);

  // Botones
  pinMode(BTN_ARRIBA, INPUT_PULLUP);
  pinMode(BTN_OK, INPUT_PULLUP);
  pinMode(BTN_ABAJO, INPUT_PULLUP);

  // Buzzer pasivo - API nueva ESP32
  ledcAttach(BUZZER_PIN, 2500, 8);
  ledcWriteTone(BUZZER_PIN, 0);

  // I2C
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);

  // OLED
  oled.setBusClock(100000);
  oled.begin();
  Wire.setClock(100000);
  pantalla2("BAYMAX", "INICIANDO...");

  // Temperatura
  mlxOK = prepararMLX();

  // ToF es desmontable. Si no esta conectado, Baymax sigue iniciando.
  tofOK = prepararToF();

  // DFPlayer
  DFSerial.begin(9600, SERIAL_8N1, DF_RX, DF_TX);
  delay(500);

  if (player.begin(DFSerial)) {
    dfOK = true;
    player.volume(27);
    player.playMp3Folder(1); // Hola, soy Baymax
  }

  // WiFi propio
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);

  server.on("/", []() {
    server.send_P(200, "text/html", PAGE);
  });
  server.on("/api/datos", apiDatos);
  server.on("/api/movimiento", apiMovimiento);
  server.on("/api/temperatura", apiTemperatura);
  server.on("/api/altura", apiAltura);
  server.on("/api/peso", apiPeso);
  server.on("/api/buzzer", apiBuzzer);
  server.on("/api/audio", apiAudio);
  server.begin();

  Serial.println();
  Serial.println("=== BAYMAX EXPO LISTO ===");
  Serial.print("WiFi: ");
  Serial.println(AP_SSID);
  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());

  beep();
  delay(450);
  dibujarMenu();
}

// =====================================================
// LOOP
// =====================================================

void loop() {
  server.handleClient();
  manejarBotones();
  revisarObstaculo();
  delay(5);
}
