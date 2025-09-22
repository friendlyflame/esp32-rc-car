# esp32-rc-car
# 🚗 ESP32 Wi-Fi Controlled Car

## 📌 Introduction
This project demonstrates the design and implementation of a Wi-Fi-controlled car using an **ESP32 microcontroller**, an **MX1508 motor driver**, and simple **3V DC toy motors**.  

The ESP32 hosts a local Wi-Fi access point and provides a control webpage that can be accessed via any smartphone browser to steer and drive the car.

---

## 🎯 Objectives
- Control a toy car wirelessly using Wi-Fi (Access Point mode).
- Provide a simple and responsive web interface for control.
- Explore power supply challenges and learn to stabilize the ESP32 during motor operation.
- Gain practical experience with PWM motor control, embedded systems, and basic robotics.

---

## 🔧 Components Used
- ESP32 Dev Board – microcontroller with Wi-Fi
- Old Toy RC Car chassis
- MX1508 Motor Driver – dual H-bridge for motor control
- 2 × 3V DC Motors – one for drive, one for steering
- 4 × AA Alkaline batteries – to power the motors
- Buck Converter (MP1584) – step down battery voltage to 3.3V for ESP32
- Jumper wires, solder, electrical tape
- (Optional) 4 × AAA batteries (initially used separately for ESP32)

---

## ⚡ Power Supply Design
### Initial Setup
- Both motors and ESP32 powered from 4 × AA alkaline batteries → **ESP32 kept resetting** due to voltage sag.  

### Experiments
- Tried separate 4 × AAA pack for ESP32.   
- Added **buck converter** for stable regulated 3.3V.  

### Final Setup
- Motors powered by 4 × Alkaline AA batteries.  
- ESP32 powered via buck converter at 3.3V.  
- All grounds tied together.  
- ✅ Stable operation, some resets.  

> **Lesson:** ESP32 is highly sensitive to voltage drops. Always ensure regulated supply and proper grounding.

---

## 🔌 Wiring Connections
- ESP32 → MX1508 Motor Driver  
  - IN1 → GPIO 26  
  - IN2 → GPIO 27  
  - IN3 → GPIO 25  
  - IN4 → GPIO 33  
- Motor Driver VCC → 4 × AA Alkaline battery pack (+)  
- Buck Converter  
  - IN+ → Battery VCC  
  - IN– → Battery GND  
  - OUT+ → ESP32 3.3V  
  - OUT– → ESP32 GND  
- **Common Ground:** battery negative, buck negative, ESP32 GND, motor driver GND  

---

## 💻 Software Design
- **Platform:** Arduino IDE with ESP32 board package  
- **Web Interface:** hosted via `WebServer` library  
  - Sliders for drive & steering speed control (0–100%)  
  - Buttons for Forward, Reverse, Left, Right (hold-to-drive)  
  - Responsive design for mobile use  
- **Motor Control:**  
  - PWM at 1 kHz, 8-bit resolution (0–255)  
  - Functions: `driveForward()`, `driveReverse()`, `steerLeft()`, `steerRight()`, `stopAll()`  
  - Default speed: 50% for drive motor, 100% for steering motor  

---

## ▶️ Operation Steps
1. Insert batteries and power on.  
2. ESP32 creates Wi-Fi hotspot:  
   - **SSID:** `ESP32-Car`  
   - **Password:** `12345678`  
3. Connect smartphone to Wi-Fi.  
4. Open browser → [http://192.168.4.1](http://192.168.4.1).  
5. Use buttons to drive and sliders to adjust speed.  

---

## 🛠️ Challenges & Solutions
| Challenge | Cause | Solution |
|-----------|-------|----------|
| ESP32 kept resetting | Voltage sag due to motor load | Used additional 4 x AAA batteries|
| Loose wires | Long jumper wires | Soldered connections, secured with tape |
| Steering inverted | Wrong pin mapping | Swapped IN1/IN2 |
| Car not moving at low PWM | Motors need torque to start | Set minimum speed ~50% |
| Power confusion | Multiple supplies unstable | Used common ground |

---

## 🔬 Research & Exploration

### 1. Motor Driver Technologies
- **Legacy BJT drivers (L298N, L293D):** bulky, inefficient, large voltage drop.  
- **MOSFET drivers (MX1508, TB6612FNG, DRV8833):** efficient, compact, cooler, better PWM response.  
- ✅ MX1508 chosen: cheap, compact, efficient for small DC motors.  

### 2. Power Supply Alternatives
- **Buck Converter:** best choice (stable 3.3V).  
- **AMS1117 regulator:** simple but wastes power as heat.  
- **Direct AAA supply:** unstable until common ground added.  
- ✅ Buck converter + 4 x AA cells + 4 x AAA cells = most stable.  

### 3. Battery Chemistry
- **Alkaline AA:** voltage sag under load.  
- **NiMH AA:** better current delivery, rechargeable.  
- **NiCd:** outdated.  
- **Li-ion 18650:** best long-term, needs protection/charging circuit.  

### 4. Other Control Options
- **Bluetooth/BLE:** lower power but requires custom app.  
- **Wi-Fi:** universal, works via any browser.  

---

## 📷 Demo
![Image Alt](https://github.com/friendlyflame/esp32-rc-car/blob/06cc3d6c3b4a37498c5fda434adeef7fdd41e54e/esp32_car.jpeg)

---

## 🚀 Future Improvements
- Add ultrasonic sensor for obstacle avoidance.  
- Stream video using ESP32-CAM.  
- 4 x AA Ni-Mh Batteries.  

---

## 📜 License
This project is licensed under the [GNU General Public License v3.0](LICENSE).
