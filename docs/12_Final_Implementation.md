# 12 — Final Implementation

**This is the normative document.** Everything here describes the robot as it
actually exists: the hardware, the wiring, the pin map, the code, and the
checks that were performed before publishing it. Nothing in
[`history/`](../history/) overrides this page.

![Final concept](../diagrams/robot_final_concept.png)

---

## 1. Final hardware architecture

| Layer | Final choice |
|---|---|
| Controller | Arduino Uno, ATmega328P |
| Motor driver | L293D / Adafruit Motor Shield V1-compatible shield |
| Motor library | `AFMotor` (Adafruit Motor Shield Library) |
| Actuators | 4 DC geared motors, `MOTOR_SPEED = 150` |
| Sensors | 4 × FC-51 IR module, digital `OUT`, one per corner of the lower deck |
| Indicators | 4 × 5 mm LED (2 red front, 2 white rear) with series resistors, on the upper deck |
| Power | 2 × 3.7 V 18650 in series → 7.4 V nominal motor rail, capacity to be verified |
| Chassis | two levels: lower ≈ 1.8 cm (sensors), upper ≈ 5.2 cm (LEDs) |

![System block diagram](../diagrams/system_block_diagram.png)

## 2. Final wiring overview

![Wiring diagram](../diagrams/wiring_diagram.png)

| Domain | Path |
|---|---|
| Motor power | battery `+` → shield `VIN`; shield `M1…M4` → the four motors |
| Logic power | shield stack → Uno 5 V → 5 V rail → 4 × FC-51 `VCC` and the LED branches |
| Common ground | battery `−`, shield `GND`, Uno `GND`, 4 × FC-51 `GND`, 4 × LED cathodes |
| Sensors | 4 × FC-51 `OUT` → `A2`, `A3`, `A4`, `A5` |
| Indicators | `D2`, `D9`, `D10`, `D13` → resistor → LED anode; cathode → ground |

Full connection list: [05 Wiring §5](05_Wiring.md#5-full-connection-table).

## 3. Final motor mapping

| Channel | Position | Sketch object | Terminal order at the shield |
|---|---|---|---|
| M1 | front left | `AFMotor motorFL(1)` | red, black |
| M2 | back left | `AFMotor motorBL(2)` | red, black |
| M3 | back right | `AFMotor motorBR(3)` | **black, red** (mirrored) |
| M4 | front right | `AFMotor motorFR(4)` | **black, red** (mirrored) |

Speed: `MOTOR_SPEED = 150` (0–255). Direction is fixed by the wiring mirror;
there is no per-motor inversion in the firmware.

## 4. Final sensor mapping

| Sensor | Position | Sketch pin | Digital number |
|---|---|---|---|
| IR front left | front-left corner, lower deck | `A2` | 16 |
| IR front right | front-right corner, lower deck | `A3` | 17 |
| IR rear left | rear-left corner, lower deck | `A4` | 18 |
| IR rear right | rear-right corner, lower deck | `A5` | 19 |

Interface: `VCC` / `GND` / `OUT`, read with `digitalRead()`.
`SENSOR_ACTIVE_LOW = 1`: **`OUT` is LOW when an obstacle is in range.**

`analogRead()` is **not** used anywhere in the final implementation. The
analog approach is history: [history/analog_sensor_experiment.md](../history/analog_sensor_experiment.md).

## 5. Final LED mapping

| LED | Colour | Position | Sketch pin |
|---|---|---|---|
| front left | red | upper deck, above the front-left IR | `D2` |
| front right | red | upper deck, above the front-right IR | `D9` |
| rear left | white | upper deck, above the rear-left IR | `D10` |
| rear right | white | upper deck, above the rear-right IR | `D13` |

Each LED has a current-limiting resistor in series. The resistor may sit on
either side of the LED; this build places it between the Arduino pin and the
anode. Resistor value: **to be verified**.

### LED behaviour matrix

| Situation | FL (D2) | FR (D9) | RL (D10) | RR (D13) |
|---|---|---|---|---|
| Startup (3 s) | blink | blink | blink | blink |
| Normal forward | **on** | **on** | off | off |
| Stop on a front obstacle | on | on | on | on |
| Reversing | off | off | **blink** | **blink** |
| Turning left | **blink** | off | **blink** | off |
| Turning right | off | **blink** | off | **blink** |
| Rear-obstacle stop / creep | on | on | blink during the creep | blink during the creep |

"Blink" = 250 ms on, 250 ms off, toggled by a non-blocking `millis()` task.

## 6. Final pin mapping

| Pin | Role | Direction | Conflict with the shield |
|---|---|---|---|
| `A2` (16) | IR front left | input | free |
| `A3` (17) | IR front right | input | free |
| `A4` (18) | IR rear left | input | free |
| `A5` (19) | IR rear right | input | free |
| `D2` | LED front left | output | free |
| `D9` | LED front right | output | free |
| `D10` | LED rear left | output | free |
| `D13` | LED rear right | output | free (shared with the Uno's onboard LED) |
| `D3` | M2 speed PWM | — | **reserved by the shield** |
| `D4` | 74HC595 clock | — | **reserved by the shield** |
| `D5` | M4 speed PWM | — | **reserved by the shield** |
| `D6` | M3 speed PWM | — | **reserved by the shield** |
| `D7` | 74HC595 enable | — | **reserved by the shield** |
| `D8` | 74HC595 data | — | **reserved by the shield** |
| `D11` | M1 speed PWM | — | **reserved by the shield** |
| `D12` | 74HC595 latch | — | **reserved by the shield** |
| `D0`, `D1` | USB serial | — | keep free for uploading |
| `A0`, `A1` | analog input only | — | cannot be used as digital I/O |

![Pin map](../diagrams/pin_map_diagram.png)

## 7. Pin conflict / verification

**The check that was performed before publishing the pinout.**

Shields reserve Arduino pins for motor control, and a shield that "looks like
it has spare pins" can still take almost all of them. The mapping above was
therefore verified against the `AFMotor` library source rather than assumed.

### 7.1 What the shield reserves

From `AFMotor.h` / `AFMotor.cpp`:

| Function | Pins |
|---|---|
| Speed PWM, one per channel | `11` (M1), `3` (M2), `6` (M3), `5` (M4) |
| 74HC595 shift register | `MOTORLATCH 12`, `MOTORCLK 4`, `MOTORDATA 8`, `MOTORENABLE 7` |
| **Total** | **8 pins: 3, 4, 5, 6, 7, 8, 11, 12** |

`AFMotor` in this version does **not** use `D13`; there is no status LED on the
shield. D13 is the Uno's built-in LED pin.

### 7.2 What is left on an Uno

| Set | Pins |
|---|---|
| Digital-capable and free | `D2`, `D9`, `D10`, `D13`, `A2`, `A3`, `A4`, `A5` |
| Digital-capable but reserved | `D0`, `D1` (USB) |
| Analog-only (no `digitalRead`/`digitalWrite`) | `A0`, `A1` |
| Taken by the shield | `D3`, `D4`, `D5`, `D6`, `D7`, `D8`, `D11`, `D12` |

### 7.3 Result

The project needs **8** digital-capable pins (4 sensors + 4 LEDs) and only
**8** are actually available (7 free digital pins on the header plus A2–A5,
minus the 4 analog-header pins the sensors take — leaving exactly D2, D9, D10,
D13 for the LEDs). **The mapping has no conflict with the AFMotor / L293D
shield.**

### 7.4 The one caveat, flagged rather than hidden

**`D13` is the Uno's built-in LED pin.** Using it for the rear-right indicator:

* is electrically safe — the onboard LED and its series resistor sit in
  parallel with the external LED, so both are driven safely;
* has a visible side effect: the Uno's onboard LED will mirror the rear-right
  indicator, including its blink phase;
* does **not** affect the motor shield, which does not use D13.

If that side effect is unacceptable in a build, the alternative mapping is to
move one indicator onto `A0` or `A1` using `analogWrite()` as a PWM output
instead of a digital one. That is a **behaviour change** and is therefore listed
in [13 Future Improvements](13_Future_Improvements.md), not applied here.

### 7.5 A second, forward-looking conflict

`D9` and `D10` are the two pins a servo library would claim. This project uses
them as plain digital outputs for LEDs, which is only safe because **no servo is
fitted**. Add a servo and `LED_FRONT_RIGHT` (D9) or `LED_REAR_LEFT` (D10) must
move. This is listed so the next person does not have to rediscover it.

### 7.6 The rule

> Before publishing any pinout for this project, check the pins against **your**
> shield version. The table above is true for the L293D / Adafruit Motor Shield
> V1 pinout as used by `AFMotor`. A different shield revision can move them.

## 8. Final robot behaviour

```
STARTUP  3 s, all four LEDs blinking, motors released
   |
FORWARD  drive forward, front LEDs on, rear LEDs off
   |-- front obstacle --> STOP_HOLD (all 4 LEDs on, 500 ms)
   |                        --> REVERSE (rear LEDs blink, 700 ms)
   |                             --> TURN (random side, that side's LEDs blink, 600 ms)
   |                                  --> FORWARD
   |-- rear obstacle --> REAR_BRAKE (front LEDs on, 300 ms)
   |                        --> REAR_CREEP (forward, rear LEDs blink, 600 ms)
   |                             --> REAR_SETTLE (front LEDs on, 300 ms)
   |                                  --> FORWARD
```

Sensors are read every 50 ms; the front test has priority over the rear test.
Full pseudocode: [08 Algorithm §1](08_Algorithm.md#1-concise-pseudocode).

## 9. Compile and upload — Arduino IDE 1.8.19

### 9.1 Install the library

`AFMotor` is not bundled with the IDE.

1. Arduino IDE 1.8.19 → **Sketch** → **Include Library** → **Manage Libraries…**
2. Search for `AFMotor`.
3. Select the **Adafruit Motor Shield Library** by Adafruit and click
   **Install**.
4. Confirm the file `AFMotor.h` is now in
   `Documents/Arduino/libraries/AFMotor/`.

No additional dependencies are required for motor control. (The Adafruit
bundle's `AccelStepper` / `Encoder` examples are not used by this project.)

### 9.2 Open and upload

1. **File** → **Open…** → `src/robot/Robot.ino`. Opening the `.ino` directly
   (rather than the folder) keeps the IDE from renaming the sketch.
2. **Tools** → **Board** → **Arduino Uno**
3. **Tools** → **Port** → the port that appears when the board is plugged in.
4. **Tools** → **Programmer** → **Arduino as ISP** (only if you upload with an
   ISP; the USB route does not need a programmer selection)
5. **Verify** (✓) — the sketch should compile with no error and no warning.
6. **Upload** (→).
7. Open the Serial Monitor at **9600 baud** if you added the temporary
   `Serial.println()` from [09 Testing step 4](09_Testing.md#step-4--lowhigh-sensor-logic).

**If verification reports a missing `AFMotor.h`:** the library is not installed
or is installed for a different board core — repeat §9.1.

**If the upload fails with `avrdude: stk500_recv()`:** the board is not in
reset. Press the RESET button once and upload again. Nothing in the firmware
causes this — it is a USB/reset issue, not a code issue.

## 10. Basic hardware testing procedure

Run these in order; the full version with expected readings is in
[09 Testing](09_Testing.md).

| # | Test | Expected |
|---|---|---|
| 1 | Continuity between `VIN` and `GND`, battery disconnected | open |
| 2 | Rail voltage at `VIN` with the pack connected | ≈ 7.4 V nominal, **to be verified** |
| 3 | Uno 5 V pin | ≈ 5 V |
| 4 | One motor per channel, robot on blocks | each spins forward on `FORWARD` |
| 5 | All four channels together, on blocks | all four start together |
| 6 | All four together, on the floor | the robot drives straight forward |
| 7 | Each IR module, obstacle in front of it | the matching LED group reacts |
| 8 | Front LEDs on, rear off, at power-on after 3 s | normal operation |

## 11. Safety checklist before powering

- [ ] No bare conductor is visible anywhere, especially near the wheels.
- [ ] No stray strand can reach a wheel or a gear through its full rotation.
- [ ] Motor wires are tied down and cannot touch the chassis.
- [ ] Sensor and LED wiring is routed away from the motor cables.
- [ ] Battery polarity verified with a meter **before** connection.
- [ ] Cells are in a proper 2-cell holder, undamaged, not swollen, not hot.
- [ ] Nothing is connected to the Uno's 5 V pin from the pack.
- [ ] Wheel retention checked — no play on any wheel.
- [ ] Robot is on blocks for the first power-up.
- [ ] A hand can reach the battery connector within one second.
- [ ] Nobody's fingers are near the wheels.

## 12. Files that make up the final implementation

| Path | Content |
|---|---|
| [`src/robot/Robot.ino`](../src/robot/Robot.ino) | the firmware, single file, no blocking delays |
| [`docs/03_Hardware.md`](03_Hardware.md) | components and the *to be verified* list |
| [`docs/05_Wiring.md`](05_Wiring.md) | every connection |
| [`docs/06_Mechanical_Construction.md`](06_Mechanical_Construction.md) | the 14 build steps |
| [`docs/08_Algorithm.md`](08_Algorithm.md) | pseudocode, states, timings |
| [`docs/09_Testing.md`](09_Testing.md) | the bench procedure |
| [`diagrams/pin_map_diagram.png`](../diagrams/pin_map_diagram.png) | the pin-conflict check, drawn |
| [`diagrams/wiring_diagram.png`](../diagrams/wiring_diagram.png) | the complete wiring |
| [`diagrams/robot_top_view.png`](../diagrams/robot_top_view.png) | sensor and LED positions |
| [`images/robot_overview.png`](../images/robot_overview.png) | the finished robot |
