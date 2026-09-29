# 02 — Requirements

This page turns the project brief into a checkable list. Every functional
requirement is traceable to a section of
[`src/robot/Robot.ino`](../src/robot/Robot.ino); every non-functional
requirement is traceable to a documented design decision.

## 1. Functional requirements

| ID | Requirement | Source in the final code | Status | Verified by |
|---|---|---|---|---|
| **F1** | The robot must drive forward autonomously when the path is clear. | `moveForward()` in `loop()` | met | [T1](09_Testing.md) |
| **F2** | The robot must detect obstacles in front and react. | `frontLeft < 200 \|\| frontRight < 200` | met | [T2](09_Testing.md) |
| **F3** | On a front obstacle: **stop** first. | `stopMotors(true)`, then `delay(1000)` | met | [T2](09_Testing.md) |
| **F4** | The stop must be indicated on the LEDs. | `stopMotors(true)` sets all four indicators HIGH | met | [T2](09_Testing.md) |
| **F5** | Then **reverse**. | `moveBackward()` — 2000 ms timed loop | met | [T2](09_Testing.md) |
| **F6** | Then choose a **random** turn direction. | `random(2) == 0 ? turnLeft() : turnRight()` | met | [T2](09_Testing.md) |
| **F7** | Then **turn**. | `turnLeft()` / `turnRight()` — 1000 ms timed loops | met | [T2](09_Testing.md) |
| **F8** | Then resume forward movement. | the `loop()` re-test, next pass | met, after a 5000 ms blind period | [T2](09_Testing.md) |
| **F9** | On a rear obstacle: stop → move forward → stop → resume. | `stopMotors(true)`, `delay(1000)`, `moveForward()`, `delay(500)`, `stopMotors(true)`, `delay(300)` | met | [T3](09_Testing.md) |
| **F10** | Four IR sensors must be read, front-left / front-right / rear-left / rear-right. | `analogRead()` on `A0`, `A1`, `A2`, `A3` | met, with a pin caveat | [T4](09_Testing.md) |
| **F11** | Four LEDs must indicate direction: 2 red front, 2 white rear. | direct `digitalWrite()` in five functions | met in code, **not functional as pinned** — [12 §7](12_Final_Implementation.md#7-pin-conflict--verification) | [T2, T3](09_Testing.md) |
| **F12** | A safety delay must run before normal autonomous movement. | two `delay(1000)` calls in `setup()` — 2 s, all indicators on then off | met, but the robot moves the moment it ends | power-on test |
| **F13** | All four LEDs are used as the startup indicator. | all four HIGH 1000 ms, all four LOW 1000 ms | met, but on the conflicted pins of F11 | power-on test |
| **F14** | *(added by review)* The loop must keep sensing while the robot manoeuvres. | not present — ten `delay()` calls, longest block 2000 ms | **not met** | [12 §5](12_Final_Implementation.md#6-timings-as-coded) |

## 2. Hardware requirements

| ID | Requirement | Value | Note |
|---|---|---|---|
| **H1** | Controller | Arduino Uno (ATmega328P) | given by the brief |
| **H2** | Motor driver | L293D / Adafruit Motor Shield V1-compatible shield | given by the brief |
| **H3** | Motor library | `AFMotor` | given by the brief |
| **H4** | Drive | 4 DC geared motors | M1 FL, M2 BL, M3 BR, M4 FR |
| **H5** | Motor speed | ≈ 150 / 255 | `setSpeed(150)` in `setup()`, never changed |
| **H6** | Motor direction | physically tested and corrected so all four wheels move together | see [history/motor_testing.md](../history/motor_testing.md) |
| **H7** | Obstacle sensing | 4 × FC-51 IR module, one per corner | comparator `OUT`, read with `analogRead()` |
| **H8** | Chassis | two levels: lower = IR sensors, upper = LEDs | [06 Mechanical](06_Mechanical_Construction.md) |
| **H9** | Sensor height | lower level ≈ 1.8 cm | from the brief, approximate |
| **H10** | Upper level height | ≈ 5.2 cm | from the brief, approximate |
| **H11** | Indicators | 4 directional LEDs, 2 red front / 2 white rear, above the IR sensors | [05 Wiring](05_Wiring.md); the sketch's pin choices are wrong — [05 §3.1](05_Wiring.md#31-why-the-four-indicator-pins-in-the-sketch-are-wrong) |
| **H12** | LED protection | current-limiting resistor in series with every LED | value **to be verified** |
| **H13** | Power | 2 × 3.7 V 18650 Li-ion cells in series | 7.4 V nominal motor rail |

## 3. Constraints that shaped the design

| ID | Constraint | Consequence in the design |
|---|---|---|
| **C1** | The motor shield already occupies 8 Arduino pins (`3, 4, 5, 6, 7, 8, 11, 12`), and `D0`/`D1` are the USB serial lines. | Nine more pins are spoken for before the robot has a sensor. The supplied firmware puts all four indicators on four of them, which does not survive checking — [12 §7](12_Final_Implementation.md#7-pin-conflict--verification). |
| **C2** | Motor current peaks far exceed what the Uno's regulator can supply. | The motor rail is fed directly from the battery, not through the board. |
| **C3** | Several robot tasks must run at once (scan, blink, drive). | The brief's non-blocking requirement is **not met**: the firmware uses `millis()` in three loops and `delay()` ten times. It blinks while it drives, but it does not scan. [13 §1](13_Future_Improvements.md#11-make-the-loop-non-blocking) |
| **C4** | The IR module output level is a board property, not a datasheet guarantee. | The firmware hard-codes the assumption `analogRead() < 200` and has no polarity switch, so an inverted module never triggers. [13 §1](13_Future_Improvements.md#12-read-the-sensors-digitally) |
| **C5** | Only four LED-capable pins are free on an Uno with this shield: `D2`, `D9`, `D10`, `D13`. | Exactly enough for four indicators, and they are the pins a working build must use. The supplied firmware does not use them, which is the fault reported in C1. |
| **C6** | `A0` and `A1` cannot be used as digital pins. | The supplied sensor assignment is legal for `analogRead()` but forecloses a later digital rework, which needs `A2`–`A5`. [05 §2](05_Wiring.md#2-sensors-the-four-ir-inputs) |
| **C7** | The brief forbids inventing specifications. | Every unspecified value is marked **to be verified** in the diagrams and in [03 Hardware](03_Hardware.md). |

## 4. Non-functional requirements

| ID | Requirement | How it is met |
|---|---|---|
| **N1** | The loop must never block. | **Not met.** There are ten `delay()` calls and three `while` loops; the longest stretch without a sensor read is 5000 ms. [07 §5](07_Software_Architecture.md#5-why-that-matters-here) |
| **N2** | The firmware must be readable and reviewable. | One file, seven functions, six of which drive indicators directly with `digitalWrite()`. Readable — but the obstacle threshold `200` is written out four times and un-named. [07 §3](07_Software_Architecture.md#3-two-raw-numbers-and-a-nameless-blink-interval) |
| **N3** | No unused variables or dead code. | Met. Every declared identifier is used and every function is called; there are no unused variables. |
| **N4** | Documentation must match the code, not an idealised robot. | Met by this revision: [07](07_Software_Architecture.md), [08](08_Algorithm.md), [12](12_Final_Implementation.md) and the wiring pages describe the supplied sketch, including its faults. |
| **N5** | The build must be reproducible. | Arduino IDE 1.8.19 + `AFMotor`, install steps in [12 §9](12_Final_Implementation.md#9-compile-and-upload--arduino-ide-1819). |
| **N6** | The robot must be safe to handle. | Pre-power checklist in [12 §11](12_Final_Implementation.md#11-safety-checklist-before-powering). Note the 2 s startup signal is not a safety delay. |

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
