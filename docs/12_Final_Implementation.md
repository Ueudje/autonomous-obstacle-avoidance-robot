# 12 — Final Implementation

**This is the normative document.** It describes the robot exactly as the
firmware in [`src/robot/Robot.ino`](../src/robot/Robot.ino) implements it:
the pin assignments as written, the timings as coded, the blocking behaviour
that follows from `delay()`, and the conflicts that follow from the pin
assignments. Where a value was never measured, it says **to be verified**.

The documents under `history/` record how the project got here. They are not
specifications, and where they disagree with this page, this page is right.

![Final concept](../diagrams/robot_final_concept.png)

---

## 1. Final hardware architecture

| Layer | Final choice |
|---|---|
| Controller | Arduino Uno, ATmega328P |
| Motor driver | L293D / Adafruit Motor Shield V1, driven by `AFMotor` |
| Motor type | `AF_DCMotor`, one object per channel, `setSpeed(150)` |
| Sensors | 4 × FC-51 IR module, read with `analogRead()` |
| Indicators | 4 × LED — 2 red at the front, 2 white at the rear — with series resistors |
| Power | 2 × 18650 in series, 7.4 V nominal to `VIN`, **to be verified** |
| Chassis | two decks: lower ≈ 1.8 cm (sensors), upper ≈ 5.2 cm (indicators) |

## 2. Final pin assignment, as written in the sketch

| `#define` | Pin | Arduino pin | Role | Free? |
|---|---|---|---|---|
| `IR_FRONT_LEFT` | `A0` | analog input | front-left obstacle input | yes |
| `IR_FRONT_RIGHT` | `A1` | analog input | front-right obstacle input | yes |
| `IR_BACK_LEFT` | `A2` | analog input | back-left obstacle input | yes |
| `IR_BACK_RIGHT` | `A3` | analog input | back-right obstacle input | yes |
| `LED_FRONT_LEFT` | `6` | D6 | front-left indicator | **no — M3 speed PWM** |
| `LED_FRONT_RIGHT` | `0` | D0 | front-right indicator | **no — USB serial TX** |
| `LED_BACK_LEFT` | `12` | D12 | back-left indicator | **no — 74HC595 latch** |
| `LED_BACK_RIGHT` | `3` | D3 | back-right indicator | **no — M2 speed PWM** |

![Pin map](../diagrams/pin_map_diagram.png)

**The four sensor pins are sound. The four LED pins are not.** Section 7 works
through why, and what the alternatives are.

## 3. Motor mapping

| Object | Channel | Position | Terminal order at the shield |
|---|---|---|---|
| `motor1` | M1 | front left | red, black |
| `motor2` | M2 | back left | red, black |
| `motor3` | M3 | back right | **black, red** |
| `motor4` | M4 | front right | **black, red** |

Speed is set once, in `setup()`, with `setSpeed(150)` on all four objects, and
is never changed afterwards. Direction is fixed by the wiring mirror, so no
motor is inverted in code.

## 4. Sensor handling

All four sensors are read with `analogRead()`, once per pass of `loop()`:

```cpp
int frontLeft  = analogRead(IR_FRONT_LEFT);
int frontRight = analogRead(IR_FRONT_RIGHT);
int backLeft   = analogRead(IR_BACK_LEFT);
int backRight  = analogRead(IR_BACK_RIGHT);
```

and compared against a single hard-coded threshold of `200`:

| Test | Meaning |
|---|---|
| `frontLeft < 200 \|\| frontRight < 200` | obstacle in front |
| `backLeft < 200 \|\| backRight < 200` | obstacle behind |

An FC-51 sits behind an LM393 comparator, so its `OUT` pin is a digital level
presented as an analog value: close to `0` for LOW and close to `1023` for
HIGH. The comparison therefore works as a digital test **only because of that**,
and only for a module that pulls `OUT` LOW while an obstacle is in range. A
module with inverted output polarity, or with a weak analog output, is not
handled — there is no `SENSOR_ACTIVE_LOW`-style switch, and no hysteresis.

Note that the threshold is written as a bare `200` at both test sites rather
than as a named constant. That is how the sketch is written; [07 §3](07_Software_Architecture.md#3-two-raw-numbers-and-a-nameless-blink-interval)
records it as a maintainability finding.

## 5. Indicator behaviour

`stopMotors(bool allLedsOn = false)` releases the four motors and then drives
all four indicators to `HIGH` (obstacle indication) or to `LOW` (all clear).
`moveForward()` and the two turn functions set the indicators individually.

| Situation | FL (D6) | FR (D0) | BL (D12) | BR (D3) |
|---|---|---|---|---|
| `setup()`, first second | on | on | on | on |
| `setup()`, second second | off | off | off | off |
| driving forward | on | on | off | off |
| stopped, `allLedsOn = true` | on | on | on | on |
| stopped, `allLedsOn = false` | off | off | off | off |
| reversing (2 s) | off | off | **blink** | **blink** |
| `turnLeft()` (1 s) | off | off | off | **blink** |
| `turnRight()` (1 s) | **blink** | **blink** | off | off |

Blink half-period is `blinkInterval = 200` ms, so a full on-off cycle is
400 ms. During a turn, the side that blinks is the **opposite** side to the one
the robot rotates towards: `turnLeft()` drives the left motors backwards and the
right motors forwards — turning the body left — while it blinks the
right-hand pair. Section 8 explains that in full.

## 6. Timings, as coded

| Duration | Value | Where it is produced |
|---|---|---|
| startup, indicators on | 1000 ms | `delay(1000)` in `setup()` |
| startup, indicators off | 1000 ms | `delay(1000)` in `setup()` |
| front reaction, stop | 1000 ms | `delay(1000)` after `stopMotors(true)` |
| reverse | 2000 ms | `while (millis() - moveStart < 2000)` |
| front reaction, second stop | 1000 ms | `delay(1000)` after `stopMotors(true)` |
| turn | 1000 ms | `while (millis() - turnStart < 1000)` |
| rear reaction, stop | 1000 ms | `delay(1000)` after `stopMotors(true)` |
| rear reaction, creep | 500 ms | `delay(500)` after `moveForward()` |
| rear reaction, settle | 300 ms | `delay(300)` after `stopMotors(true)` |
| blink half-period | 200 ms | `millis()` comparison inside the loops |
| loop poll | every 10 ms while in a timed loop | `delay(10)` |

Worst-case time between two sensor reads: **5000 ms** — 1000 + 2000 + 1000 +
1000 through the whole front-obstacle sequence. Best case: one read per
`loop()` pass, i.e. as fast as the unblocked forward case allows.

![Control flow](../diagrams/algorithm_flowchart.png)

## 7. Pin conflict / verification

This is the check that has to be done before the pinout is published, and it is
reported here even though it fails.

### 7.1 What the shield reserves

From `AFMotor.h` and `AFMotor.cpp`:

| Function | Pins |
|---|---|
| Speed PWM, one per channel | `11` (M1), `3` (M2), `6` (M3), `5` (M4) |
| 74HC595 shift register | `MOTORLATCH 12`, `MOTORCLK 4`, `MOTORDATA 8`, `MOTORENABLE 7` |
| **Total** | **8 pins: 3, 4, 5, 6, 7, 8, 11, 12** |

The Uno's own `D0` and `D1` are the USB serial lines.

### 7.2 The result

| Sketch pin | Assigned to | Owner | Verdict |
|---|---|---|---|
| `A0`, `A1`, `A2`, `A3` | the four sensors | nothing else | **clear** |
| `D6` | front-left LED | M3 speed PWM | **conflict** |
| `D3` | back-right LED | M2 speed PWM | **conflict** |
| `D12` | back-left LED | 74HC595 latch | **conflict** |
| `D0` | front-right LED | USB serial TX | **conflict** |

**Three of the four indicators are wired to motor-control pins and the fourth
is wired to the serial port.** The consequences are not cosmetic:

* `digitalWrite(6, HIGH)` and `digitalWrite(3, HIGH)` land on PWM lines the
  shield drives in `setSpeed()` and in the motor `run()` calls, so an indicator
  write and a speed change fight over the same pin.
* `digitalWrite(12, …)` toggles the latch line the shield itself drives inside
  `latch_tx()`. The two can interleave, which can leave the 74HC595 holding a
  half-written output pattern — the L293D then sees a direction it was not asked
  for.
* `pinMode(0, OUTPUT)` takes the USB-serial transmit line away from the
  on-board USB interface, so the Serial Monitor is unusable and the
  auto-reset upload path can be blocked.

### 7.3 What is still free

| Pin | Status |
|---|---|
| `A4`, `A5` | free, and unused by the sketch |
| `D1` | USB serial RX — keep it free |
| `D2`, `D9`, `D10`, `D13` | free and LED-capable |
| `A2`, `A3` | used by the sensors, but A2–A5 are also digital-capable |

So there are four free, LED-capable pins — `D2`, `D9`, `D10`, `D13` — and moving
the four indicators there is a change to four `#define` lines. The sensors do
not have to move: `A2` and `A3` are in use, and `A4`/`A5` are free if a second
sensor row is ever wanted ([13 §2](13_Future_Improvements.md#22-a-second-sensor-height-never-built)).

### 7.4 Two secondary observations

* **`D0` is not merely occupied, it is the wrong kind of pin for this job.** It
  is the one pin on the board that must stay an input for the board to be
  usable at all.
* **The two sensors on `A0` and `A1` are analog-only pins** on the ATmega328P:
  `digitalRead()` and `digitalWrite()` do not work on them. The sketch only ever
  calls `analogRead()`, so this is correct today, but it forecloses any later
  change to digital sensing on those two corners.

### 7.5 The rule

> Check the pinout against **your** shield version, from the library source,
> before publishing it. The table in §7.1 is true for the L293D / Adafruit
> Motor Shield V1 pinout used by `AFMotor`. A different shield revision can
> move these pins entirely.

## 8. Final robot behaviour

```
setup():  speeds 150, randomSeed, LEDs all on 1 s, LEDs all off 1 s
   |
loop():
   read A0, A1, A2, A3 with analogRead()
   |
   +-- front blocked (A0 or A1 < 200) --> stopMotors(true)      1000 ms
   |     moveBackward()  all BACKWARD, back pair blinking      2000 ms
   |     stopMotors(true)                                      1000 ms
   |     random(2): turnLeft() or turnRight()                  1000 ms
   |        (turn ends with stopMotors(true))
   |
   +-- rear blocked (A2 or A3 < 200) --> stopMotors(true)       1000 ms
   |     moveForward()  front pair on, back pair off            500 ms
   |     stopMotors(true)                                       300 ms
   |
   +-- clear --> moveForward()  front pair on, back pair off     no timing
```

### Why the indicators blink on the opposite side

`turnLeft()` runs `motor1` and `motor2` — the two left-hand motors — backwards,
and `motor3` and `motor4` forwards. The left wheels therefore roll backwards
while the right wheels roll forwards, and the body rotates to the left. In the
same function the firmware drives `LED_FRONT_RIGHT` and `LED_BACK_RIGHT` with
the blink state, so the **right-hand** pair blinks. `turnRight()` is the exact
mirror image: the body rotates right and the **left-hand** pair blinks.

This is a mismatch between what the indicator names and what the robot does.
It is reported rather than fixed because the firmware is presented as supplied;
the fix is a one-line change in each of the two functions, listed in
[13 §4](13_Future_Improvements.md#4-the-turn-indicators-are-on-the-wrong-side).

### Why the rear reaction drives forward

The rear sequence stops, drives **forward** for 500 ms, stops again and
resumes. That is deliberate in the sense that it is what the code says: the
rear obstacle is not escaped from, it is checked. A reader expecting the front
behaviour — reverse — should know that the two cases differ, which is why both
[08 Algorithm](08_Algorithm.md) and this section spell the sequence out in full
rather than describing it as "avoid".

## 9. Compile and upload — Arduino IDE 1.8.19

1. **Sketch → Include Library → Manage Libraries…** → install **Adafruit Motor
   Shield Library** by Adafruit. It provides `AFMotor.h`; it is not bundled with
   the IDE.
2. **File → Open…** → `src/robot/Robot.ino`.
3. **Tools → Board → Arduino Uno**; **Tools → Port** → the board.
4. **Tools → Programmer** → *Arduino as ISP* only if you are using an ISP; the
   USB route needs no programmer selection.
5. **Verify** (✓), then **Upload** (→).

**`avrdude: stk500_recv()`** — the board is not entering the bootloader. Press
RESET once and upload again. Note that with `LED_FRONT_RIGHT` on `D0` the
auto-reset path is compromised; see §7.2.

## 10. Basic hardware testing procedure

Run these in order. The full version with expected readings is in
[09 Testing](09_Testing.md).

| # | Test | Expected |
|---|---|---|
| 1 | Continuity between `VIN` and `GND`, battery disconnected | open |
| 2 | Rail voltage at `VIN`, pack connected | 7.4 V nominal, **to be verified** |
| 3 | Uno 5 V pin | ≈ 5 V |
| 4 | One motor per channel, on blocks | each turns forward on `run(FORWARD)` |
| 5 | All four channels, on the floor | the robot drives straight forward |
| 6 | Each IR module, obstacle in front | `analogRead()` drops below 200 |
| 7 | The four indicator pins, driven by hand | **see §7 — three of them are not free** |

## 11. Safety checklist before powering

- [ ] No bare conductor visible, especially near the wheels.
- [ ] No stray strand can reach a wheel or a gear through its full rotation.
- [ ] Motor wires tied down and clear of the chassis.
- [ ] Battery polarity verified with a meter **before** connection.
- [ ] Cells in a proper 2-cell holder, undamaged, not swollen, not hot.
- [ ] Nothing connected to the Uno's 5 V pin from the pack.
- [ ] Wheel retention checked on all four.
- [ ] Robot on blocks for the first power-up.
- [ ] The battery connector is within reach of one hand.
- [ ] The two seconds of the startup signal are understood: the robot starts
      moving as soon as they end.

## 12. Files that make up the final implementation

| Path | Content |
|---|---|
| [`src/robot/Robot.ino`](../src/robot/Robot.ino) | the firmware, as supplied |
| [`docs/03_Hardware.md`](03_Hardware.md) | components and the *to be verified* list |
| [`docs/05_Wiring.md`](05_Wiring.md) | every connection, and the four conflicts |
| [`docs/07_Software_Architecture.md`](07_Software_Architecture.md) | every function, line by line |
| [`docs/08_Algorithm.md`](08_Algorithm.md) | the control flow, in order |
| [`docs/09_Testing.md`](09_Testing.md) | the bench procedure |
| [`diagrams/pin_map_diagram.png`](../diagrams/pin_map_diagram.png) | the conflict check, drawn |
| [`diagrams/wiring_diagram.png`](../diagrams/wiring_diagram.png) | the complete wiring |
| [`diagrams/robot_top_view.png`](../diagrams/robot_top_view.png) | sensor and indicator positions |
| [`diagrams/robot_component_layout.png`](../diagrams/robot_component_layout.png) | component positions and heights above the ground, in cm |
| [`images/robot_overview.png`](../images/robot_overview.png) | the finished robot |
