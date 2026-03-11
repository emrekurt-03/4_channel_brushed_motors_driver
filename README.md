# STM32G4 4-Channel DC Motor Controller (UART & I2C)

This repository contains the embedded firmware for a 4-channel brushed DC motor controller based on the **STM32G431xx** microcontroller. The system is designed to interface with **TB67H451FNG** (or similar H-Bridge) motor drivers, accepting commands via **UART** or **I2C** to control speed and direction independently for each motor.

A standout feature of this project is the **Dynamic Power Limiting** hardware switch, allowing the user to toggle between a "Safe Mode" (5V equivalent) and "Full Power Mode" (12V) on the fly.

## 🚀 Key Features

* **Independent 4-Channel Control:** Full control over direction (Forward/Reverse) and speed (0-100% duty cycle).
* **Dual Communication Interfaces:**
    * **UART:** Interrupt-based serial communication for PC or telemetry control.
    * **I2C:** Slave-mode implementation for integration into multi-MCU systems (Master-Slave architecture).
* **Hardware-Level Safety (Voltage Limiting):** A physical switch on `PA0` dynamically scales the PWM output to limit the effective voltage (Safe 5V vs. Raw 12V).
* **Optimized Drive Logic:** Custom algorithm to handle the inverted PWM logic required by TB67H-series drivers during reverse operation.
* **Robust Command Parsing:** `sscanf`-based string parser to filter and validate incoming serial commands.

## 🛠️ Hardware Requirements (BOM)

| Component | Function |
| :--- | :--- |
| **STM32G431xx** | Main MCU (Cortex-M4, 170 MHz) |
| **TB67H451FNG** | Dual-channel H-Bridge Motor Drivers (2 units) |
| **12V DC Motor** | Actuators (4 units) |
| **MP1584EN Buck Converter** | 12V to 3.3V Step-down Voltage Regulator |
| **SPST Switch** | Safety / Voltage Mode selection |
| **Capacitors (MLCC & Electrolytic)** | MCU Decoupling (100nF) and Motor in-rush current filtering |

## 🔌 Pinout Configuration

* **Motor 1:** Dir -> `PA5`, PWM -> `PA6` (TIM3_CH1)
* **Motor 2:** Dir -> `PC4`, PWM -> `PA7` (TIM3_CH2)
* **Motor 3:** Dir -> `PC5`, PWM -> `PB0` (TIM3_CH3)
* **Motor 4:** Dir -> `PB2`, PWM -> `PB1` (TIM3_CH4)
* **Mode Switch:** `PA0` (Pull-up enabled. GND = 5V Mode, Open = 12V Mode)
* **UART (PC Comm):** TX -> `PA9`, RX -> `PA10` (Baudrate: 115200)
* **I2C:** SCL -> `PA15`, SDA -> `PB9`

## 💻 Communication Protocol

The system accepts text-based (String) commands over UART. Every command must terminate with a `\n` (Line Feed) or `\r` (Carriage Return).

**Command Format:** `M<ID> <DIR> <SPEED>`

* `<ID>`: Motor Number (1, 2, 3, or 4)
* `<DIR>`: `F` (Forward) or `R` (Reverse)
* `<SPEED>`: Integer value between `0` and `100`

**Examples:**
* `M1 F 100` -> Run Motor 1 Forward at 100% power.
* `M3 R 50` -> Run Motor 3 Reverse at 50% power.
* `M2 F 0` -> Stop Motor 2.

**System Feedback:**
Upon a successful command, the MCU replies:
`OK -> Motor:1 Yon:F Hiz:100 (Limit:1000)`

## ⚙️ Software Architecture

* **Non-Blocking Event Handling:** The main loop handles logic while UART operates in interrupt mode, ensuring no dropped packets during motor regulation.
* **PWM Inversion Logic:** To achieve consistent torque in both directions with TB67H drivers, the firmware automatically calculates `1000 - target_pwm` when in Reverse mode to maintain linearity.
* **Dynamic Scaling:** The `voltaj_limiti` variable scales the 0-100 user input to the 0-1000 timer period based on the real-time state of the hardware switch.

## 🚀 Installation & Usage

1.  Clone the repository: `git clone <your-repo-link>`
2.  Open **STM32CubeIDE** and import the project: *File > Import > Existing Projects into Workspace*.
3.  Build the project and flash your STM32 board via ST-Link.
4.  Connect a Serial Terminal (e.g., PuTTY, TeraTerm) to the designated COM port.
5.  Set the baudrate to **115200** and start sending commands!