#include <WiFi.h>
#include <WebServer.h>

// ----- CONFIGURE THESE PINS -----
const int IN1 = 27;  // Steering motor IN1
const int IN2 = 26;  // Steering motor IN2
const int IN3 = 25;  // Drive motor IN3
const int IN4 = 33;  // Drive motor IN4
// --------------------------------

// PWM settings
const int PWM_FREQ = 1000;   // 1 kHz safe for MX1508
const int PWM_BITS = 8;      // 8-bit resolution: 0-255

// PWM channels
const int CH_IN1 = 0;
const int CH_IN2 = 1;
const int CH_IN3 = 2;
const int CH_IN4 = 3;

WebServer server(80);

// Default speeds (0-255)
int driveSpeed = 128;    // Forward/Reverse default 50%
int steerSpeed = 255;    // Left/Right default ~100%

// Mobile web UI
const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
  body { font-family: Arial; text-align:center; margin-top:20px; }
  button { width:160px; height:60px; font-size:20px; margin:8px; }
  input[type=range] { width:80%; }
</style>
</head>
<body>
  <h2>ESP32 Car Control</h2>

  <p>Drive Speed: <span id="driveTxt">50</span>%</p>
  <input id="driveSpeed" type="range" min="0" max="100" value="50" 
         oninput="updateSpeed('drive', this.value)">

  <p>Steering Speed: <span id="steerTxt">90</span>%</p>
  <input id="steerSpeed" type="range" min="0" max="100" value="100" 
         oninput="updateSpeed('steer', this.value)">

  <p>
    <button id="fwd" ontouchstart="hold('forward')" ontouchend="release()"
            onmousedown="hold('forward')" onmouseup="release()">Forward</button><br>

    <button id="left" ontouchstart="hold('left')" ontouchend="release()"
            onmousedown="hold('left')" onmouseup="release()">Left</button>
    <button id="right" ontouchstart="hold('right')" ontouchend="release()"
            onmousedown="hold('right')" onmouseup="release()">Right</button><br>

    <button id="rev" ontouchstart="hold('reverse')" ontouchend="release()"
            onmousedown="hold('reverse')" onmouseup="release()">Reverse</button>
  </p>

<script>
function hold(cmd) {
  fetch('/move?dir=' + cmd);
}
function release() {
  fetch('/stop');
}
function updateSpeed(type, val) {
  if(type === 'drive') {
    document.getElementById('driveTxt').innerText = val;
  } else {
    document.getElementById('steerTxt').innerText = val;
  }
  fetch('/speed?type=' + type + '&val=' + val);
}
</script>
</body>
</html>
)rawliteral";

// ---------- Motor control ----------
void stopAll() {
  ledcWrite(CH_IN1, 0);
  ledcWrite(CH_IN2, 0);
  ledcWrite(CH_IN3, 0);
  ledcWrite(CH_IN4, 0);
}

// Swapped left/right directions
void steerLeft() {
  ledcWrite(CH_IN1, 0);
  ledcWrite(CH_IN2, steerSpeed);
}
void steerRight() {
  ledcWrite(CH_IN1, steerSpeed);
  ledcWrite(CH_IN2, 0);
}

void driveForward() {
  ledcWrite(CH_IN3, driveSpeed);
  ledcWrite(CH_IN4, 0);
}
void driveReverse() {
  ledcWrite(CH_IN3, 0);
  ledcWrite(CH_IN4, driveSpeed);
}

// ---------- HTTP handlers ----------
void handleRoot() {
  server.send(200, "text/html", htmlPage);
}

void handleMove() {
  String dir = server.hasArg("dir") ? server.arg("dir") : "";
  if (dir == "forward") driveForward();
  else if (dir == "reverse") driveReverse();
  else if (dir == "left") steerLeft();
  else if (dir == "right") steerRight();
  server.send(200, "text/plain", "OK");
}

void handleStop() {
  stopAll();
  server.send(200, "text/plain", "Stopped");
}

void handleSpeed() {
  if (server.hasArg("type") && server.hasArg("val")) {
    int pct = constrain(server.arg("val").toInt(), 0, 100);
    if (server.arg("type") == "drive") {
      driveSpeed = map(pct, 0, 100, 0, 255);
      Serial.printf("Drive speed: %d%% -> %d\n", pct, driveSpeed);
    } else if (server.arg("type") == "steer") {
      steerSpeed = map(pct, 0, 100, 0, 255);
      Serial.printf("Steer speed: %d%% -> %d\n", pct, steerSpeed);
    }
  }
  server.send(200, "text/plain", "Speed updated");
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);

  // Configure PWM channels
  ledcSetup(CH_IN1, PWM_FREQ, PWM_BITS); ledcAttachPin(IN1, CH_IN1);
  ledcSetup(CH_IN2, PWM_FREQ, PWM_BITS); ledcAttachPin(IN2, CH_IN2);
  ledcSetup(CH_IN3, PWM_FREQ, PWM_BITS); ledcAttachPin(IN3, CH_IN3);
  ledcSetup(CH_IN4, PWM_FREQ, PWM_BITS); ledcAttachPin(IN4, CH_IN4);

  stopAll(); // Car stopped at boot

  // Start Access Point
  const char* apName = "ESP32-Car";
  const char* apPass = "12345678";
  WiFi.softAP(apName, apPass);
  Serial.println("AP started. Connect to SSID: ESP32-Car, pass: 12345678");
  Serial.println(WiFi.softAPIP());

  // Routes
  server.on("/", handleRoot);
  server.on("/move", handleMove);
  server.on("/stop", handleStop);
  server.on("/speed", handleSpeed);

  server.begin();
}

// ---------- Loop ----------
void loop() {
  server.handleClient();
}
