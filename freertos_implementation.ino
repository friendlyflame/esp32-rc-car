
#include <WiFi.h>
#include <WebServer.h>

// ==========================================
// 1. MOTOR DRIVER PIN CONFIGURATION (MX1508)
// ==========================================

const int IN1 = 27;  // Steering motor IN1
const int IN2 = 26;  // Steering motor IN2
const int IN3 = 25;  // Drive motor IN3
const int IN4 = 33;  // Drive motor IN4

// PWM settings
const int PWM_FREQ = 1000;
const int PWM_BITS = 8;

// PWM channels
const int CH_IN1 = 0;
const int CH_IN2 = 1;
const int CH_IN3 = 2;
const int CH_IN4 = 3;

// ==========================================
// 2. WIFI AND WEB SERVER
// ==========================================

WebServer server(80);

const char* apName = "ESP32-Car";
const char* apPass = "12345678";

// Default motor speeds
int driveSpeed = 128;
int steerSpeed = 255;

// ==========================================
// 3. FREERTOS CONFIGURATION
// ==========================================

// Types of motor commands
enum MotorCommandType {
  CMD_FORWARD,
  CMD_REVERSE,
  CMD_LEFT,
  CMD_RIGHT,
  CMD_STOP,
  CMD_DRIVE_SPEED,
  CMD_STEER_SPEED
};

// Structure containing command information
struct MotorCommand {
  MotorCommandType type;
  int value;
};

// Queue handle
QueueHandle_t motorQueue;

// Task handles
TaskHandle_t webTaskHandle = NULL;
TaskHandle_t motorTaskHandle = NULL;
TaskHandle_t safetyTaskHandle = NULL;

// Last time a movement command was received
volatile uint32_t lastMoveTime = 0;

// ==========================================
// 4. MOBILE WEBPAGE
// ==========================================

const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">

<style>
  body {
    font-family: Arial;
    text-align: center;
    margin-top: 20px;
  }

  button {
    width: 160px;
    height: 60px;
    font-size: 20px;
    margin: 8px;
    touch-action: none;
  }

  input[type=range] {
    width: 80%;
  }
</style>
</head>

<body>

<h2>ESP32 Car Control</h2>

<p>
Drive Speed: <span id="driveTxt">50</span>%
</p>

<input id="driveSpeed"
type="range" min="0" max="100" value="50"
oninput="updateSpeed('drive', this.value)">

<p>
Steering Speed: <span id="steerTxt">100</span>%
</p>

<input id="steerSpeed"
type="range" min="0" max="100" value="100"
oninput="updateSpeed('steer', this.value)">

<p>

<button id="fwd"
onpointerdown="hold('forward')"
onpointerup="release()"
onpointerleave="release()"
onpointercancel="release()">
Forward
</button>

<br>

<button id="left"
onpointerdown="hold('left')"
onpointerup="release()"
onpointerleave="release()"
onpointercancel="release()">
Left
</button>

<button id="right"
onpointerdown="hold('right')"
onpointerup="release()"
onpointerleave="release()"
onpointercancel="release()">
Right
</button>

<br>

<button id="rev"
onpointerdown="hold('reverse')"
onpointerup="release()"
onpointerleave="release()"
onpointercancel="release()">
Reverse
</button>

</p>

<script>

let movementTimer = null;

function hold(cmd) {

  release();

  fetch('/move?dir=' + cmd);

  movementTimer = setInterval(function() {
    fetch('/move?dir=' + cmd);
  }, 200);

}

function release() {

  if (movementTimer !== null) {
    clearInterval(movementTimer);
    movementTimer = null;
  }

  fetch('/stop');

}

function updateSpeed(type, val) {

  if (type === 'drive') {
    document.getElementById('driveTxt').innerText = val;
  }

  else {
    document.getElementById('steerTxt').innerText = val;
  }

  fetch('/speed?type=' + type + '&val=' + val);

}

</script>

</body>
</html>
)rawliteral";

// ==========================================
// 5. MOTOR CONTROL FUNCTIONS
// ==========================================

void stopAll() {

  ledcWrite(CH_IN1, 0);
  ledcWrite(CH_IN2, 0);

  ledcWrite(CH_IN3, 0);
  ledcWrite(CH_IN4, 0);

}

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

// ==========================================
// 6. QUEUE STOP HELPER
// ==========================================

void sendStopCommand() {

  MotorCommand cmd;

  cmd.type = CMD_STOP;
  cmd.value = 0;

  // Remove old pending commands
  xQueueReset(motorQueue);

  // Place STOP command at the front
  xQueueSendToFront(motorQueue, &cmd, 0);

}

// ==========================================
// 7. HTTP REQUEST HANDLERS
// ==========================================

void handleRoot() {

  server.send(200, "text/html", htmlPage);

}

// Movement handler
void handleMove() {

  String dir = server.hasArg("dir")
               ? server.arg("dir") : "";

  MotorCommand cmd;

  if (dir == "forward") {
    cmd.type = CMD_FORWARD;
  }

  else if (dir == "reverse") {
    cmd.type = CMD_REVERSE;
  }

  else if (dir == "left") {
    cmd.type = CMD_LEFT;
  }

  else if (dir == "right") {
    cmd.type = CMD_RIGHT;
  }

  else {
    server.send(400, "text/plain", "Invalid command");
    return;
  }

  cmd.value = 0;

  // Send movement command to FreeRTOS queue
  if (xQueueSend(motorQueue, &cmd, 0) == pdTRUE) {

    lastMoveTime = millis();

    server.send(200, "text/plain", "OK");

  }

  else {

    server.send(503, "text/plain", "Command queue full");

  }

}

// Stop handler
void handleStop() {

  sendStopCommand();

  server.send(200, "text/plain", "Stopped");

}

// Speed handler
void handleSpeed() {

  if (server.hasArg("type") && server.hasArg("val")) {

    int pct = constrain(
      server.arg("val").toInt(), 0, 100
    );

    MotorCommand cmd;

    if (server.arg("type") == "drive") {

      cmd.type = CMD_DRIVE_SPEED;

      cmd.value = map(pct, 0, 100, 0, 255);

    }

    else if (server.arg("type") == "steer") {

      cmd.type = CMD_STEER_SPEED;

      cmd.value = map(pct, 0, 100, 0, 255);

    }

    else {

      server.send(400, "text/plain", "Invalid type");
      return;

    }

    if (xQueueSend(motorQueue, &cmd, 0) != pdTRUE) {

      server.send(503, "text/plain", "Queue full");
      return;

    }

    Serial.printf(
      "Speed updated: %d%%\n", pct
    );

  }

  server.send(200, "text/plain", "Speed updated");

}

// ==========================================
// 8. FREERTOS TASK 1: MOTOR CONTROL
// ==========================================

void motorControlTask(void *parameter) {

  MotorCommand cmd;

  while (1) {

    // Wait until a command is available
    if (xQueueReceive(
          motorQueue,
          &cmd,
          portMAX_DELAY
        ) == pdTRUE) {

      switch (cmd.type) {

        case CMD_FORWARD:

          driveForward();

          break;

        case CMD_REVERSE:

          driveReverse();

          break;

        case CMD_LEFT:

          steerLeft();

          break;

        case CMD_RIGHT:

          steerRight();

          break;

        case CMD_STOP:

          stopAll();

          break;

        case CMD_DRIVE_SPEED:

          driveSpeed = cmd.value;

          break;

        case CMD_STEER_SPEED:

          steerSpeed = cmd.value;

          break;

      }

    }

  }

}

// ==========================================
// 9. FREERTOS TASK 2: WEB SERVER
// ==========================================

void webServerTask(void *parameter) {

  while (1) {

    // Process incoming HTTP requests
    server.handleClient();

    // Allow other tasks to execute
    vTaskDelay(pdMS_TO_TICKS(2));

  }

}

// ==========================================
// 10. FREERTOS TASK 3: SAFETY MONITOR
// ==========================================

void safetyTask(void *parameter) {

  uint32_t observedMoveTime = lastMoveTime;

  bool stopSent = false;

  while (1) {

    uint32_t currentMoveTime = lastMoveTime;

    // Detect a new movement command
    if (currentMoveTime != observedMoveTime) {

      observedMoveTime = currentMoveTime;

      stopSent = false;

    }

    // Stop if no movement command arrives for 700 ms
    if (!stopSent &&
        (uint32_t)(millis() - currentMoveTime) > 700) {

      // Recheck before stopping
      if (lastMoveTime == currentMoveTime) {

        sendStopCommand();

        Serial.println("Safety timeout: Car stopped");

        stopSent = true;

      }

    }

    // Check every 100 milliseconds
    vTaskDelay(pdMS_TO_TICKS(100));

  }

}

// ==========================================
// 11. SETUP
// ==========================================

void setup() {

  Serial.begin(115200);

  Serial.println();
  Serial.println("ESP32 FreeRTOS Car Starting...");

  // Configure PWM channels
  ledcSetup(CH_IN1, PWM_FREQ, PWM_BITS);
  ledcAttachPin(IN1, CH_IN1);

  ledcSetup(CH_IN2, PWM_FREQ, PWM_BITS);
  ledcAttachPin(IN2, CH_IN2);

  ledcSetup(CH_IN3, PWM_FREQ, PWM_BITS);
  ledcAttachPin(IN3, CH_IN3);

  ledcSetup(CH_IN4, PWM_FREQ, PWM_BITS);
  ledcAttachPin(IN4, CH_IN4);

  // Stop motors at startup
  stopAll();

  lastMoveTime = millis();

  // Create FreeRTOS queue
  motorQueue = xQueueCreate(
    10,
    sizeof(MotorCommand)
  );

  if (motorQueue == NULL) {

    Serial.println("Queue creation failed!");

    while (1) {
      delay(1000);
    }

  }

  Serial.println("Motor queue created successfully");

  // Start Wi-Fi Access Point
  WiFi.softAP(apName, apPass);

  Serial.println("WiFi Access Point Started");

  Serial.print("SSID: ");
  Serial.println(apName);

  Serial.print("Password: ");
  Serial.println(apPass);

  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());

  // Configure HTTP routes
  server.on("/", handleRoot);
  server.on("/move", handleMove);
  server.on("/stop", handleStop);
  server.on("/speed", handleSpeed);

  server.begin();

  Serial.println("Web server started");

  // ========================================
  // CREATE FREERTOS TASKS
  // ========================================

  // Task 1: Motor Control
  BaseType_t motorResult = xTaskCreate(
    motorControlTask,
    "Motor Task",
    2048,
    NULL,
    3,
    &motorTaskHandle
  );

  // Task 2: Web Server
  BaseType_t webResult = xTaskCreate(
    webServerTask,
    "Web Server Task",
    4096,
    NULL,
    2,
    &webTaskHandle
  );

  // Task 3: Safety Monitoring
  BaseType_t safetyResult = xTaskCreate(
    safetyTask,
    "Safety Task",
    2048,
    NULL,
    1,
    &safetyTaskHandle
  );

  // Check task creation
  if (motorResult != pdPASS ||
      webResult != pdPASS ||
      safetyResult != pdPASS) {

    Serial.println("ERROR: Task creation failed!");

    stopAll();

    while (1) {
      delay(1000);
    }

  }

  Serial.println("-----------------------------");
  Serial.println("All FreeRTOS tasks created!");
  Serial.println("Motor Task Priority: 3");
  Serial.println("Web Task Priority: 2");
  Serial.println("Safety Task Priority: 1");
  Serial.println("-----------------------------");

}

// ==========================================
// 12. MAIN LOOP
// ==========================================

void loop() {

  // Main loop is no longer responsible
  // for handling HTTP requests.

  vTaskDelay(pdMS_TO_TICKS(1000));

}
