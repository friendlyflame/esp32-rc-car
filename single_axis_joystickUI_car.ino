#include <WiFi.h>
#include <WebServer.h>

// =====================================================
// ESP32 CAR - SINGLE JOYSTICK CONTROLLER
// MX1508 MOTOR DRIVER
// =====================================================

// ---------------- PIN CONFIGURATION ------------------

const int STEER_IN1 = 27;
const int STEER_IN2 = 26;

const int DRIVE_IN1 = 25;
const int DRIVE_IN2 = 33;

// -----------------------------------------------------

// ---------------- PWM CONFIGURATION ------------------

const int PWM_FREQ = 1000;
const int PWM_BITS = 8;

// ESP32 PWM channels
const int CH_STEER_1 = 0;
const int CH_STEER_2 = 1;
const int CH_DRIVE_1 = 2;
const int CH_DRIVE_2 = 3;

// -----------------------------------------------------

WebServer server(80);

// Maximum PWM
int maxDrivePWM = 255;
int maxSteerPWM = 255;

// Minimum PWM needed to start motor
const int MIN_PWM = 100;

// Joystick dead zone
const int DEAD_ZONE = 8;

// Safety timeout
const unsigned long COMMAND_TIMEOUT = 500;

// Last command received
unsigned long lastCommandTime = 0;


// =====================================================
// HTML USER INTERFACE
// =====================================================

const char* htmlPage = R"rawliteral(

<!DOCTYPE html>

<html>

<head>

<meta name="viewport"
      content="width=device-width, initial-scale=1.0,
               maximum-scale=1.0, user-scalable=no">

<title>ESP32 RC Car</title>

<style>

* {
  box-sizing: border-box;
  -webkit-user-select: none;
  user-select: none;
}

body {
  margin: 0;
  background: #111;
  color: white;
  font-family: Arial, sans-serif;
  overflow: hidden;
  touch-action: none;
}

h1 {
  text-align: center;
  font-size: 22px;
  margin: 12px 0 5px 0;
}

.status {
  text-align: center;
  font-size: 14px;
  color: #aaa;
  margin-bottom: 10px;
}

.controller {
  display: flex;
  justify-content: center;
  align-items: center;
  width: 100%;
  height: calc(100vh - 100px);
}

.joystick-area {
  width: 75vw;
  height: 75vw;
  max-width: 360px;
  max-height: 360px;
  position: relative;
}

.joystick-base {
  position: absolute;
  width: 100%;
  height: 100%;

  border-radius: 50%;

  background: #292929;

  border: 4px solid #555;

  box-shadow:
    inset 0 0 20px #000,
    0 5px 15px #000;
}

.joystick-stick {
  position: absolute;

  width: 34%;
  height: 34%;

  left: 33%;
  top: 33%;

  border-radius: 50%;

  background: #777;

  border: 3px solid #aaa;

  box-shadow:
    0 4px 10px #000;

  transition:
    left 0.05s,
    top 0.05s;
}

.label {
  position: absolute;

  width: 100%;

  text-align: center;

  font-size: 16px;

  color: #bbb;

  pointer-events: none;
}

.label.top {
  top: -30px;
}

.label.bottom {
  bottom: -30px;
}

.label.left {
  left: -60px;
  top: 48%;
}

.label.right {
  right: -60px;
  top: 48%;
}

.stop {
  position: absolute;

  left: 50%;
  bottom: 20px;

  transform: translateX(-50%);

  width: 100px;
  height: 55px;

  border-radius: 15px;

  border: 2px solid #fff;

  background: #c62828;

  color: white;

  font-size: 18px;
  font-weight: bold;

  box-shadow: 0 4px 10px #000;
}

.stop:active {
  transform:
    translateX(-50%)
    scale(0.95);
}

.info {
  position: absolute;

  bottom: 5px;

  width: 100%;

  text-align: center;

  font-size: 12px;

  color: #777;
}

</style>

</head>

<body>

<h1>ESP32 RC CAR</h1>

<div class="status" id="status">
  Connected
</div>

<div class="controller">

  <!-- SINGLE JOYSTICK -->

  <div class="joystick-area"
       id="joystick">

    <div class="joystick-base"></div>

    <div class="joystick-stick"
         id="joystickStick"></div>

    <div class="label top">
      FORWARD
    </div>

    <div class="label bottom">
      REVERSE
    </div>

    <div class="label left">
      LEFT
    </div>

    <div class="label right">
      RIGHT
    </div>

  </div>

</div>

<button class="stop" id="stopButton">
  STOP
</button>

<div class="info">
  Joystick = Steering + Throttle
</div>


<script>

// =====================================================
// JOYSTICK VARIABLES
// =====================================================

let steering = 0;
let throttle = 0;

let lastSend = 0;

const SEND_INTERVAL = 50;


// =====================================================
// GET ELEMENTS
// =====================================================

const joystickArea =
  document.getElementById("joystick");

const joystickStick =
  document.getElementById("joystickStick");


// =====================================================
// JOYSTICK FUNCTION
// =====================================================

let active = false;


function moveJoystick(x, y) {

  const rect =
    joystickArea.getBoundingClientRect();


  const centerX =
    rect.left + rect.width / 2;

  const centerY =
    rect.top + rect.height / 2;


  let dx =
    x - centerX;

  let dy =
    y - centerY;


  const maxDistance =
    rect.width * 0.33;


  const distance =
    Math.sqrt(
      dx * dx +
      dy * dy
    );


  // Limit joystick to circular area

  if (distance > maxDistance) {

    dx =
      dx / distance *
      maxDistance;

    dy =
      dy / distance *
      maxDistance;

  }


  // Convert to -100 ... +100

  let horizontal =
    dx / maxDistance * 100;


  let vertical =
    -dy / maxDistance * 100;


  // Dead zone

  if (Math.abs(horizontal) < 8)
    horizontal = 0;

  if (Math.abs(vertical) < 8)
    vertical = 0;


  // Update joystick visual

  joystickStick.style.left =
    (33 + horizontal * 0.33) + "%";


  joystickStick.style.top =
    (33 - vertical * 0.33) + "%";


  // Horizontal = steering

  steering =
    Math.round(horizontal);


  // Vertical = throttle

  throttle =
    Math.round(vertical);

}


// =====================================================
// RESET JOYSTICK
// =====================================================

function resetJoystick() {

  active = false;

  steering = 0;
  throttle = 0;

  joystickStick.style.left =
    "33%";

  joystickStick.style.top =
    "33%";

}


// =====================================================
// POINTER DOWN
// =====================================================

joystickArea.addEventListener(
  "pointerdown",
  function(e) {

    active = true;

    joystickArea.setPointerCapture(
      e.pointerId
    );

    moveJoystick(
      e.clientX,
      e.clientY
    );

  }
);


// =====================================================
// POINTER MOVE
// =====================================================

joystickArea.addEventListener(
  "pointermove",
  function(e) {

    if (!active)
      return;

    moveJoystick(
      e.clientX,
      e.clientY
    );

  }
);


// =====================================================
// POINTER UP
// =====================================================

joystickArea.addEventListener(
  "pointerup",
  function() {

    resetJoystick();

  }
);


// =====================================================
// POINTER CANCEL
// =====================================================

joystickArea.addEventListener(
  "pointercancel",
  function() {

    resetJoystick();

  }
);


// =====================================================
// SEND COMMAND TO ESP32
// =====================================================

function sendCommand() {

  const now =
    Date.now();


  if (
    now - lastSend <
    SEND_INTERVAL
  )
    return;


  lastSend = now;


  fetch(
    "/control?steer=" +
    steering +
    "&throttle=" +
    throttle,
    {
      cache: "no-store"
    }
  )
  .catch(function() {

    document.getElementById("status")
      .innerText =
      "Connection lost";

  });

}


// Send continuously

setInterval(
  sendCommand,
  SEND_INTERVAL
);


// =====================================================
// EMERGENCY STOP
// =====================================================

document
  .getElementById("stopButton")
  .addEventListener(
    "pointerdown",
    function(e) {

      e.preventDefault();

      resetJoystick();

      fetch("/stop");

    }
  );


// =====================================================
// PREVENT SCROLLING / GESTURES
// =====================================================

document.body.addEventListener(
  "touchmove",
  function(e) {

    e.preventDefault();

  },
  {
    passive: false
  }
);

</script>

</body>

</html>

)rawliteral";


// =====================================================
// MOTOR CONTROL
// =====================================================

void stopAll() {

  ledcWrite(CH_STEER_1, 0);
  ledcWrite(CH_STEER_2, 0);

  ledcWrite(CH_DRIVE_1, 0);
  ledcWrite(CH_DRIVE_2, 0);

}


// =====================================================
// CONVERT JOYSTICK VALUE TO PWM
// =====================================================

int joystickToPWM(
  int value,
  int maximumPWM
) {

  value =
    constrain(
      value,
      -100,
      100
    );


  if (value == 0)
    return 0;


  int magnitude =
    abs(value);


  int pwm =
    map(
      magnitude,
      0,
      100,
      MIN_PWM,
      maximumPWM
    );


  return constrain(
    pwm,
    0,
    255
  );

}


// =====================================================
// DRIVE MOTOR
// =====================================================

void setDrive(
  int throttle
) {

  throttle =
    constrain(
      throttle,
      -100,
      100
    );


  if (throttle > 0) {

    // FORWARD

    int pwm =
      joystickToPWM(
        throttle,
        maxDrivePWM
      );


    ledcWrite(
      CH_DRIVE_1,
      pwm
    );

    ledcWrite(
      CH_DRIVE_2,
      0
    );

  }

  else if (throttle < 0) {

    // REVERSE

    int pwm =
      joystickToPWM(
        throttle,
        maxDrivePWM
      );


    ledcWrite(
      CH_DRIVE_1,
      0
    );

    ledcWrite(
      CH_DRIVE_2,
      pwm
    );

  }

  else {

    ledcWrite(
      CH_DRIVE_1,
      0
    );

    ledcWrite(
      CH_DRIVE_2,
      0
    );

  }

}


// =====================================================
// STEERING MOTOR
// =====================================================

void setSteering(
  int steer
) {

  steer =
    constrain(
      steer,
      -100,
      100
    );


  if (steer > 0) {

    // RIGHT

    int pwm =
      joystickToPWM(
        steer,
        maxSteerPWM
      );


    ledcWrite(
      CH_STEER_1,
      0
    );

    ledcWrite(
      CH_STEER_2,
      pwm
    );

  }

  else if (steer < 0) {

    // LEFT

    int pwm =
      joystickToPWM(
        steer,
        maxSteerPWM
      );


    ledcWrite(
      CH_STEER_1,
      pwm
    );

    ledcWrite(
      CH_STEER_2,
      0
    );

  }

  else {

    ledcWrite(
      CH_STEER_1,
      0
    );

    ledcWrite(
      CH_STEER_2,
      0
    );

  }

}


// =====================================================
// ROOT PAGE
// =====================================================

void handleRoot() {

  server.send(
    200,
    "text/html",
    htmlPage
  );

}


// =====================================================
// JOYSTICK CONTROL
// =====================================================

void handleControl() {

  if (
    !server.hasArg("steer") ||
    !server.hasArg("throttle")
  ) {

    server.send(
      400,
      "text/plain",
      "Missing command"
    );

    return;

  }


  int steer =
    server.arg("steer").toInt();


  int throttle =
    server.arg("throttle").toInt();


  steer =
    constrain(
      steer,
      -100,
      100
    );


  throttle =
    constrain(
      throttle,
      -100,
      100
    );


  // Apply steering

  setSteering(
    steer
  );


  // Apply throttle

  setDrive(
    throttle
  );


  // Update safety timer

  lastCommandTime =
    millis();


  server.send(
    200,
    "text/plain",
    "OK"
  );

}


// =====================================================
// STOP
// =====================================================

void handleStop() {

  stopAll();


  lastCommandTime =
    millis();


  server.send(
    200,
    "text/plain",
    "STOPPED"
  );

}


// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(
    115200
  );


  // ---------------- PWM ----------------

  ledcSetup(
    CH_STEER_1,
    PWM_FREQ,
    PWM_BITS
  );


  ledcSetup(
    CH_STEER_2,
    PWM_FREQ,
    PWM_BITS
  );


  ledcSetup(
    CH_DRIVE_1,
    PWM_FREQ,
    PWM_BITS
  );


  ledcSetup(
    CH_DRIVE_2,
    PWM_FREQ,
    PWM_BITS
  );


  ledcAttachPin(
    STEER_IN1,
    CH_STEER_1
  );


  ledcAttachPin(
    STEER_IN2,
    CH_STEER_2
  );


  ledcAttachPin(
    DRIVE_IN1,
    CH_DRIVE_1
  );


  ledcAttachPin(
    DRIVE_IN2,
    CH_DRIVE_2
  );


  // Stop motors at startup

  stopAll();


  // ---------------- WiFi AP ----------------

  const char* apName =
    "ESP32-Car";

  const char* apPassword =
    "12345678";


  WiFi.softAP(
    apName,
    apPassword
  );


  Serial.println();

  Serial.println(
    "================================"
  );

  Serial.println(
    "ESP32 RC CAR READY"
  );

  Serial.println(
    "WiFi: ESP32-Car"
  );

  Serial.println(
    "Password: 12345678"
  );

  Serial.print(
    "IP address: "
  );

  Serial.println(
    WiFi.softAPIP()
  );

  Serial.println(
    "================================"
  );


  // ---------------- Web Server ----------------

  server.on(
    "/",
    handleRoot
  );


  server.on(
    "/control",
    handleControl
  );


  server.on(
    "/stop",
    handleStop
  );


  server.begin();


  lastCommandTime =
    millis();

}


// =====================================================
// MAIN LOOP
// =====================================================

void loop() {

  server.handleClient();


  // ---------------- SAFETY FAILSAFE ----------------

  if (
    millis() - lastCommandTime >
    COMMAND_TIMEOUT
  ) {

    stopAll();

  }

}

