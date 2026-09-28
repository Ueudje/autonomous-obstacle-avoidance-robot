# 02 — Requirements

This page turns the project brief into a checkable list. Every functional
requirement is traceable to a section of
[`src/robot/Robot.ino`](../src/robot/Robot.ino); every non-functional
requirement is traceable to a documented design decision.

## 1. Functional requirements

| ID | Requirement | Source in the final code | Verified by |
|---|---|---|---|
| **F1** | The robot must drive forward autonomously when the path is clear. | `beginNormalForward()` → `STATE_FORWARD` | [T1](09_Testing.md) |
| **F2** | The robot must detect obstacles in front and react. | `frontBlocked()` in `STATE_FORWARD` | [T2](09_Testing.md) |
| **F3** | On a front obstacle: **stop** first. | `beginStopHold()` → `stopMotors(true)` | [T2](09_Testing.md) |
| **F4** | The stop must be indicated on the LEDs. | `stopMotors(true)` sets `LED_ALL` on | [T2](09_Testing.md) |
| **F5** | Then **reverse**. | `beginReverse()` → `driveAllBackward()` | [T2](09_Testing.md) |
| **F6** | Then choose a **random** turn direction. | `beginTurn()` → `random(0, 2) == 0` | [T2](09_Testing.md) |
| **F7** | Then **turn**. | `turnInPlace(turnLeft)` | [T2](09_Testing.md) |
| **F8** | Then resume forward movement. | `STATE_TURN` → `beginNormalForward()` | [T2](09_Testing.md) |
| **F9** | On a rear obstacle: stop → move forward → stop → resume. | `STATE_REAR_BRAKE` → `REAR_CREEP` → `REAR_SETTLE` → forward | [T3](09_Testing.md) |
| **F10** | Four IR sensors must be read, front-left / front-right / rear-left / rear-right. | `readSensors()` | [T4](09_Testing.md) |
| **F11** | Four LEDs must indicate direction: 2 red front, 2 white rear. | `LED_*_BIT` mask constants | [T2, T3](09_Testing.md) |
| **F12** | A safety delay must run before normal autonomous movement. | `STATE_STARTUP`, `STARTUP_DELAY_MS = 3000` | power-on test |
| **F13** | All four LEDs are used as the startup indicator. | `beginStartup()` → `startBlink(LED_ALL)` | power-on test |

## 2. Hardware requirements

| ID | Requirement | Value | Note |
|---|---|---|---|
| **H1** | Controller | Arduino Uno (ATmega328P) | given by the brief |
| **H2** | Motor driver | L293D / Adafruit Motor Shield V1-compatible shield | given by the brief |
| **H3** | Motor library | `AFMotor` | given by the brief |
| **H4** | Drive | 4 DC geared motors | M1 FL, M2 BL, M3 BR, M4 FR |
| **H5** | Motor speed | ≈ 150 / 255 | `MOTOR_SPEED = 150` |
| **H6** | Motor direction | physically tested and corrected so all four wheels move together | see [history/motor_testing.md](../history/motor_testing.md) |
| **H7** | Obstacle sensing | 4 × FC-51 IR module, one per corner | digital `OUT` |
| **H8** | Chassis | two levels: lower = IR sensors, upper = LEDs | [06 Mechanical](06_Mechanical_Construction.md) |
| **H9** | Sensor height | lower level ≈ 1.8 cm | from the brief, approximate |
| **H10** | Upper level height | ≈ 5.2 cm | from the brief, approximate |
| **H11** | Indicators | 4 directional LEDs, 2 red front / 2 white rear, above the IR sensors | [05 Wiring](05_Wiring.md) |
| **H12** | LED protection | current-limiting resistor in series with every LED | value **to be verified** |
| **H13** | Power | 2 × 3.7 V 18650 Li-ion cells in series | 7.4 V nominal motor rail |

## 3. Constraints that shaped the design

| ID | Constraint | Consequence in the design |
|---|---|---|
| **C1** | The motor shield already occupies 8 Arduino pins. | Sensor and LED pins had to be chosen from what is left — see [12 Final Implementation §7](12_Final_Implementation.md#7-pin-conflict--verification). |
| **C2** | Motor current peaks far exceed what the Uno's regulator can supply. | The motor rail is fed directly from the battery, not through the board. |
| **C3** | Several robot tasks must run at once (scan, blink, drive). | `millis()` state machine instead of `delay()`. |
| **C4** | The IR module output level is a board property, not a datasheet guarantee. | `SENSOR_ACTIVE_LOW` is a single `#define` so it can be flipped after a bench test. |
| **C5** | Only 8 digital-capable pins are free on an Uno with this shield (D2, D9, D10, D13, A2, A3, A4, A5), and exactly 8 are needed. | The budget is fully spent, with nothing spare; D13 was therefore needed for the fourth LED, with the onboard-LED side effect documented. |
| **C6** | The brief forbids inventing specifications. | Every unspecified value is marked **to be verified** in the diagrams and in [03 Hardware](03_Hardware.md). |

## 4. Non-functional requirements

| ID | Requirement | How it is met |
|---|---|---|
| **N1** | The loop must never block. | No `delay()` / `delayMicroseconds()` anywhere in the sketch. |
| **N2** | The firmware must be readable and reviewable. | One `beginX()` function per behaviour, one `case` per state, LED logic as bit masks. |
| **N3** | No unused variables or dead code. | Every declared identifier is used; no function is defined and never called. |
| **N4** | Documentation must match the code, not an idealised robot. | [08 Algorithm](08_Algorithm.md) describes the state machine that is actually implemented. |
| **N5** | The build must be reproducible. | Arduino IDE 1.8.19 + `AFMotor`, install steps in [12 Final Implementation §9](12_Final_Implementation.md#9-compile-and-upload--arduino-ide-1819). |
| **N6** | The robot must be safe to handle. | Pre-power checklist in [12 Final Implementation §11](12_Final_Implementation.md#11-safety-checklist-before-powering). |

## 5. Explicitly out of scope

These were **not** required, were **not** implemented, and are therefore not
part of the final design. They are listed as open work in
[13 Future Improvements](13_Future_Improvements.md):

* distance measurement, mapping or path planning;
* line following, encoders, closed-loop speed control;
* Bluetooth / Wi-Fi remote control, telemetry;
* a second sensor height (the original "two sensor levels" idea — see
  [history/analog_sensor_experiment.md](../history/analog_sensor_experiment.md));
* measured battery runtime, current logging, or obstacle-range characterisation.
