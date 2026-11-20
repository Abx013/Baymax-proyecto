#include <WiFi.h>
#include <WebServer.h>

// =========================
// BAYMAX - FIRMWARE DE EXPO
// =========================
// Red propia para la exposición.
// Celular/laptop -> WiFi "Baymax-Expo" -> http://192.168.4.1

const char* AP_SSID = "Baymax-Expo";
const char* AP_PASS = "BaymaxExpo2026";

WebServer server(80);

// -------- PINES PROPUESTOS --------
// TB6612FNG
const int AIN1 = 25;
const int AIN2 = 26;
const int PWMA = 27;
const int BIN1 = 14;
const int BIN2 = 12;
const int PWMB = 13;
const int STBY = 33;

// HC-SR04
const int TRIG_PIN = 4;
const int ECHO_PIN = 16;

// Buzzer activo
const int BUZZER_PIN = 17;

// Valores temporales / simulados.
// Más adelante se sustituyen por lecturas reales de:
// MLX90614, HX711 y VL53L1X.
float temperatura = 36.6;
float peso = 68.0;
float altura = 1.70;
float distancia = 76.0;
String movimiento = "detenido";

// ---------- MOTORES ----------
void motorA(int velocidad) {
  if (velocidad > 0) {
    digitalWrite(AIN1, HIGH);
    digitalWrite(AIN2, LOW);
  } else if (velocidad < 0) {
    digitalWrite(AIN1, LOW);
    digitalWrite(AIN2, HIGH);
  } else {
    digitalWrite(AIN1, LOW);
    digitalWrite(AIN2, LOW);
  }
  ledcWrite(0, abs(velocidad));
}

void motorB(int velocidad) {
  if (velocidad > 0) {
    digitalWrite(BIN1, HIGH);
    digitalWrite(BIN2, LOW);
  } else if (velocidad < 0) {
    digitalWrite(BIN1, LOW);
    digitalWrite(BIN2, HIGH);
  } else {
    digitalWrite(BIN1, LOW);
    digitalWrite(BIN2, LOW);
  }
  ledcWrite(1, abs(velocidad));
}

void detener() {
  motorA(0);
  motorB(0);
  movimiento = "detenido";
}

void mover(String accion) {
  const int v = 180;

  if (accion == "adelante") {
    motorA(v);
    motorB(v);
    movimiento = "en_movimiento";
  } else if (accion == "atras") {
    motorA(-v);
    motorB(-v);
    movimiento = "en_movimiento";
  } else if (accion == "izquierda") {
    motorA(-v);
    motorB(v);
    movimiento = "en_movimiento";
  } else if (accion == "derecha") {
    motorA(v);
    motorB(-v);
    movimiento = "en_movimiento";
  } else {
    detener();
  }
}

// ---------- HC-SR04 ----------
float leerDistancia() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duracion = pulseIn(ECHO_PIN, HIGH, 25000);
  if (duracion == 0) return distancia;

  return duracion * 0.0343 / 2.0;
}

// ---------- BUZZER ----------
void beep(int ms = 120) {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(ms);
  digitalWrite(BUZZER_PIN, LOW);
}

// ---------- API ----------
void enviarDatos() {
  distancia = leerDistancia();

  String json = "{";
  json += "\"temperatura\":" + String(temperatura, 1) + ",";
  json += "\"peso\":" + String(peso, 1) + ",";
  json += "\"altura\":" + String(altura, 2) + ",";
  json += "\"distancia\":" + String(distancia, 0) + ",";
  json += "\"movimiento\":\"" + movimiento + "\"";
  json += "}";

  server.send(200, "application/json", json);
}

void apiMovimiento() {
  String accion = server.arg("accion");
  mover(accion);
  server.send(200, "text/plain", "OK");
}

void apiBuzzer() {
  beep();
  server.send(200, "text/plain", "OK");
}

void apiAudio() {
  // Próxima integración:
  // DFPlayer Mini + microSD.
  // pista=1 saludo
  // pista=2 inicio medición
  // pista=3 medición terminada
  String pista = server.arg("pista");
  Serial.println("Pista solicitada: " + pista);
  server.send(200, "text/plain", "OK");
}

void apiEvaluacion() {
  String accion = server.arg("accion");

  if (accion == "iniciar") {
    beep(80);

    // Próxima integración real:
    // 1. MLX90614 -> temperatura
    // 2. HX711 -> peso
    // 3. VL53L1X -> altura
    // 4. IMC se calcula en la web
    // 5. DFPlayer reproduce resultado
    server.send(200, "text/plain", "EVALUACION_INICIADA");
  } else {
    server.send(200, "text/plain", "REINICIADA");
  }
}

// Página mínima de acceso.
// El panel completo vive en GitHub Pages,
// pero en la exposición se recomienda servir
// también una copia local del index.html desde ESP32.
const char PAGE[] PROGMEM = R"rawliteral(
<!doctype html>
<html lang="es">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Baymax Expo</title>
<style>
body{font-family:system-ui;background:#eef4fb;color:#152238;margin:0;padding:24px}
main{max-width:720px;margin:auto;background:#fff;border-radius:20px;padding:24px}
button{padding:14px 18px;border:0;border-radius:12px;background:#2563d8;color:white;font-weight:700;margin:5px}
.card{padding:16px;background:#f5f8fc;border-radius:14px;margin:10px 0}
</style>
</head>
<body>
<main>
<h1>Baymax Expo</h1>
<p>ESP32 conectado. Para la interfaz completa usa la página del proyecto o integra el index.html final en este firmware.</p>
<div class="card" id="datos">Cargando...</div>
<button onclick="m('adelante')">↑</button><br>
<button onclick="m('izquierda')">←</button>
<button onclick="m('detener')">■</button>
<button onclick="m('derecha')">→</button><br>
<button onclick="m('atras')">↓</button>
<script>
async function datos(){
 const r=await fetch('/api/datos'); const d=await r.json();
 document.getElementById('datos').innerHTML=
 'Temp: '+d.temperatura+' °C<br>Peso: '+d.peso+' kg<br>Altura: '+d.altura+' m<br>Distancia: '+d.distancia+' cm';
}
function m(a){fetch('/api/movimiento?accion='+a)}
setInterval(datos,1500);datos();
</script>
</main>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);

  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(BIN1, OUTPUT);
  pinMode(BIN2, OUTPUT);
  pinMode(STBY, OUTPUT);
  digitalWrite(STBY, HIGH);

  ledcSetup(0, 1000, 8);
  ledcAttachPin(PWMA, 0);
  ledcSetup(1, 1000, 8);
  ledcAttachPin(PWMB, 1);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  detener();

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);

  Serial.println();
  Serial.println("=== BAYMAX EXPO ===");
  Serial.print("WiFi: ");
  Serial.println(AP_SSID);
  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", []() {
    server.send_P(200, "text/html", PAGE);
  });

  server.on("/api/datos", enviarDatos);
  server.on("/api/movimiento", apiMovimiento);
  server.on("/api/buzzer", apiBuzzer);
  server.on("/api/audio", apiAudio);
  server.on("/api/evaluacion", apiEvaluacion);

  server.begin();
  Serial.println("Servidor iniciado.");
}

void loop() {
  server.handleClient();
}
