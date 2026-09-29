# 13 — Future Improvements

Everything on this page is **deliberately not part of the final
implementation**. The robot described in
[12 — Final Implementation](12_Final_Implementation.md) is the machine the
supplied firmware in [`src/robot/Robot.ino`](../src/robot/Robot.ino) actually
produces — faults included. This page is the honest list of what it does not
do, ordered by how much each item would improve it.

None of it has been implemented, and none of it has been quietly added to the
firmware. The two exceptions are §1 and §2: those are faults, not improvements,
and they are listed first because the robot does not work correctly without
them.

| Priority | Item | Section |
|---|---|---|
| 1 | Stop blocking the loop for 5 s at a time | [§1.1](#11-make-the-loop-non-blocking) |
| 2 | Read the sensors with `digitalRead()` and a polarity switch | [§1.2](#12-read-the-sensors-digitally) |
| 3 | Move the four indicators off the shield's pins | [§2.1](#21-move-the-indicators-off-the-shields-pins) |
| 4 | Put the turn indicators on the side the robot turns towards | [§4](#4-the-turn-indicators-are-on-the-wrong-side) |
| 5 | Name the magic numbers | [§3](#3-three-naming-and-structure-fixes-worth-making) |
| 6 | Filter the sensor readings | [§1.3](#13-re-read-the-sensors-more-than-once-per-poll) |

---

## 1. Faults to fix first

### 1.1 Make the loop non-blocking

**Now:** `millis()` is used in three places, correctly, as the deadline of the
three timed `while` loops: the 2000 ms reverse, the 1000 ms left turn and the
1000 ms right turn. Everywhere else the sketch waits with a bare `delay()` —
ten times.

| Symptom | Cause |
|---|---|
| The robot drives into a wall it saw | no sensor is read for up to 5000 ms after a single triggering sample |
| A new obstacle mid-manoeuvre is ignored | same cause |
| The indicators are the only thing still alive | the blink runs *inside* the two timed loops; the scan does not |
| Timing is hard to change | five durations, five literals, spread over four functions |

**Option:** a `millis()` state machine with one state per step —
`FORWARD → STOP_HOLD → REVERSE → TURN → FORWARD`, plus `REAR_BRAKE`,
`REAR_CREEP`, `REAR_SETTLE` and `STARTUP` — and a single state-exit timer per
state. Each duration becomes one named constant, the sensor read moves to the
top of the loop, and the scan runs at a fixed interval through every manoeuvre.

**Cost:** roughly 60 lines rewritten, and a behaviour change at the margin —
the robot will start reacting during a manoeuvre, which is the point. An earlier
version of this project did exactly this and is recorded in
[history/final_changes.md](../history/final_changes.md); the differences are
compared in [08 §9](08_Algorithm.md#9-what-the-state-machine-would-have-done).

### 1.2 Read the sensors digitally

**Now:** `analogRead()` on `A0`–`A3`, compared against a literal `200`.

| Problem | Detail |
|---|---|
| No polarity switch | a module that pulls `OUT` high while blocked reads ≈ 1023 and **never triggers** |
| No hysteresis | one marginal sample starts a full 5 s sequence |
| Threshold is meaningless | the FC-51 output is a comparator level, so the "analog" reading carries no more information than one bit |
| The threshold is un-named | `200` appears four times, in two statements, and can drift apart |

**Option:** move the sensors to `A2`–`A5` — which are ordinary GPIO, digital
18–19 for `A2`/`A3` and digital 20–21 for `A4`/`A5` — and use
`digitalRead()` with one `#define SENSOR_ACTIVE_LOW 1`. That restores the
polarity switch, makes the reading a clean boolean, and frees `A0`/`A1`.

**Why the move is necessary rather than optional:** `A0` and `A1` are
analog-only pins on the ATmega328P. `digitalRead()` does not work on them at
all, so a digital rework cannot keep the supplied sensor assignment. It is
four `#define` lines — [05 §2](05_Wiring.md#2-sensors-the-four-ir-inputs).

**Cost:** four `#define` lines and four `analogRead()` → `digitalRead()`
changes; no hardware change beyond moving four wires.

### 1.3 Re-read the sensors more than once per poll

**Now:** each sensor is sampled once per `loop()` pass and the value is used
as-is.

**Why it matters:** a single sample of a reflective sensor is noisy. A speck of
dust, a shadow, or a wheel-splash can produce one false reading — and one false
front reading triggers a whole 5 s sequence.

**Option:** require `N` consecutive identical readings before acting, e.g. 3 of 3
at 50 ms. Cost: up to 150 ms more reaction time, and roughly 15 lines of code.

**Note:** averaging and debouncing were already explored during the analog
approach — see
[history/analog_sensor_experiment.md](../history/analog_sensor_experiment.md).
It is not implemented because it changes the documented timing, so it is a
design decision, not an oversight.

### 1.4 Re-evaluate obstacles mid-sequence

**Now:** the sensor read happens once at the top of `loop()`. Once the robot
commits to the front-obstacle sequence it completes all 5000 ms of it
regardless of what it sees — see
[08 §4](08_Algorithm.md#4-case-1--obstacle-in-front).

**Option:** re-read the sensors inside the reverse and add an abort condition —
for example, if both front sensors stay clear for 100 ms, skip the turn and
resume forward. This changes the observable sequence, so it belongs here and
not in the final firmware.

## 2. Faults to fix in the wiring

### 2.1 Move the indicators off the shield's pins

**Now:** the four indicators are on `D6`, `D0`, `D12` and `D3` — the M3 speed
PWM, the USB serial TX line, the 74HC595 latch, and the M2 speed PWM. All four
conflict with something the shield or the board already owns. The derivation
and the consequences are in
[12 §7](12_Final_Implementation.md#7-pin-conflict--verification).

| Symptom | Cause |
|---|---|
| An indicator flickers when a motor starts | `digitalWrite()` fights the shield's PWM |
| A motor turns the wrong way after a write to an indicator | the 74HC595 latch can be written half-way through `latch_tx()` |
| Serial Monitor is dead after upload | `pinMode(0, OUTPUT)` took the USB TX line away from the board |
| `avrdude: stk500_recv()` | the auto-reset path is compromised by the same `D0` assignment |

**Option — four lines, no hardware change.** `D2`, `D9`, `D10` and `D13` are
free and LED-capable:

| `#define` | Current | Change to |
|---|---|---|
| `LED_FRONT_LEFT` | `6` | `2` |
| `LED_FRONT_RIGHT` | `0` | `9` |
| `LED_BACK_LEFT` | `12` | `10` |
| `LED_BACK_RIGHT` | `3` | `13` |

`D13` also drives the Uno's built-in LED, which is a cosmetic side effect. If
the indicators are ever dimmed rather than switched, note that `D13` is not a
PWM pin.

**Not done**, because the firmware is published exactly as supplied.

### 2.2 A second sensor height, never built

**Now:** the lower deck carries the four IR modules at ≈ 1.8 cm; the upper deck
carries the four LEDs at ≈ 5.2 cm.

**Originally:** the idea was two *sensor* levels, to catch obstacles of
different heights — a low one for small obstacles, a higher one for something
taller than the chassis can see past.

**Blocked by:** the pin budget. With this shield an Uno has exactly four
LED-capable pins free, and the four indicators need all of them.

**Options if it is ever built:**

* move to a board with more GPIO (a Mega, or an I/O expander on the I²C bus);
* use `A0`/`A1` — free once §1.2 is done — for a second row, accepting that
  those two corners would be analog-only and could never be switched to
  `digitalRead()`;
* use an I/O expander such as a PCF8574 for the second sensor row.

This is the clearest example in the project of a good idea that the chosen
controller cannot afford.

## 3. Three naming and structure fixes worth making

**Now:** two significant values are inline literals and one is misfiled.

| Value | Where | Fix |
|---|---|---|
| `200` | four times, in the two `if` statements of `loop()` | one `const int OBSTACLE_THRESHOLD = 200;` at the top |
| `randomSeed(analogRead(5))` | `setup()` | `A5` is not connected to anything, so the seed is noise. Use a fixed seed, or read a real pin |
| `blinkInterval` | declared between the pin defines and `setup()` | group it with the other timing constants, and name the other five durations to match |

**Also worth doing:** `stopMotors(bool allLedsOn = false)` drives the
indicators as well as the motors, which the name does not suggest. Renaming it
to something like `stopAndSignal(bool allOn)` costs nothing and removes a
question every reader has to answer. Rationale in
[07 §7](07_Software_Architecture.md#7-stopmotorsbool-allledson--false).

## 4. The turn indicators are on the wrong side

**Now:** `turnLeft()` runs `motor1` and `motor2` backwards and `motor3` and
`motor4` forwards — the body rotates left — while driving `LED_FRONT_RIGHT` and
`LED_BACK_RIGHT` with the blink state. `turnRight()` is the exact mirror.

So the pair that blinks is always the side the robot is turning *away* from. A
watcher cannot tell from the indicators which way the robot is about to go.

**Fix:** swap the two `LED_*` writes in each function. Two lines.

| Function | Blinks now | Should blink |
|---|---|---|
| `turnLeft()` | front right, back right | front left, back left |
| `turnRight()` | front left, back left | front right, back right |

**Not done**, because the firmware is published as supplied. The behaviour is
documented in [12 §8](12_Final_Implementation.md#why-the-indicators-blink-on-the-opposite-side).

## 5. Turn by angle instead of by time

**Now:** the turn is a 1000 ms timed loop, so the angle covered depends on the
floor, the battery voltage and the load.

**Option:** drive the turn until one of the front sensors sees a clear path, or
until an encoder-based angle is reached. Neither exists on this build: there
are no encoders, and the sensors are at the corners rather than in front of the
turn axis.

## 6. Encoders and closed-loop speed control

**Now:** every motor gets the same PWM value — `setSpeed(150)`, set once — and
is assumed to turn at the same speed. The robot therefore drifts to one side on
a rough floor.

**Option:** encoders on the motor shafts, with a per-motor PID so that
`run(FORWARD)` really means the same linear speed on all four wheels, and an
IMU for line-following. This is the single largest change to the mechanical and
electrical design, and the one that most improves reliability.

## 7. A better escape strategy

**Now:** `random(2)` picks the turn direction once, when the turn begins.

**Weakness:** two runs in the same room can repeat the same dead end, and a
random turn does not prefer the side with more free space.

**Options:** bias the random choice with the number of consecutive failed
escapes; remember the last successful turn direction; or add a "creep forward
while turning" state so the robot slides along a wall instead of turning in
place. Each is a behaviour change and is therefore not in the final firmware.

## 8. Power and measurement improvements

| Item | Why |
|---|---|
| Measure the current | no current figure in this project is measured; the rail behaviour under load is described, not quantified |
| Inline fuse | sized once the current is known — a cheap way to make the pack safe |
| Battery voltage monitoring | an analog input reading the pack would let the firmware refuse to start on a flat cell — `A4` and `A5` are free |
| A pack that is characterised | capacity, internal resistance and maximum current, measured rather than read from a label — see [11 Power System §8](11_Power_System.md#8-how-to-characterise-a-pack-before-trusting-it) |
| Better cells | a genuine high-current cell would fix the "rear motors sometimes do not start" symptom at its root |

## 9. Communications and telemetry

**Not implemented at all:** no Bluetooth, no Wi-Fi, no serial reporting of state.

**Options:** print the current state over the serial port during development —
which requires first moving the front-right indicator off `D0`; add a Bluetooth
module on the software-serial pins; add a small OLED to show the state without
opening a laptop.

Useful, but it does not make the robot better at avoiding obstacles — it makes
it easier to debug.

## 10. Robustness improvements

| Improvement | Effect |
|---|---|
| Watchdog timer | the robot recovers from a firmware hang instead of sitting still |
| Filter obviously impossible readings (both rear sensors triggered while both front ones are clear) | rejects some electrical noise |
| Hysteresis on the sensor threshold | fewer stop/creep cycles on a marginal surface |
| Light-dependent auto-calibration of the trimpots | needs a servo or a digital pot — not applicable to a plain FC-51 |
| Mute the turn blink or make it dimmer | cosmetic, and pointless while §2.1 is unfixed |

## 11. Changes *not* recommended

* **Increasing the speed above 150/255.** More speed means more current and less
  time to react; the current pack is already the limiting factor.
* **Shortening or removing the 2 s startup signal.** It is a signal, not a real
  safety delay — the robot moves the instant it ends. Make it *longer* and
  visible, or make it a button, rather than removing it.
* **Swapping the firmware for a library-based line follower.** That is a
  different project, and the reaction behaviour documented in
  [08 Algorithm](08_Algorithm.md) would no longer describe the machine.

## 12. If you build this yourself

The order that works:

1. Build the mechanical chassis and check the wheels by hand.
2. **Fix the pin map first** — §2.1 and §1.2. Four lines each, and nothing
   below works until they are done.
3. One motor at a time, on blocks — [09 Testing](09_Testing.md#5-stage-c--one-motor-at-a-time).
4. All four together — [stage D](09_Testing.md#6-stage-d--all-four-motors-together).
5. Characterise the pack — [11 Power System §8](11_Power_System.md#8-how-to-characterise-a-pack-before-trusting-it).
6. One sensor at a time, including the LOW/HIGH check — [stage E](09_Testing.md#7-stage-e--one-ir-sensor-at-a-time).
7. LEDs, then the whole robot — [stages F and G](09_Testing.md#8-stage-f--sensors-and-leds-together).
8. Only then consider anything from §5 onwards.
