# 07 — Software Architecture

This document describes the firmware in [`src/robot/Robot.ino`](../src/robot/Robot.ino)
as it is written. It is a description, not an endorsement: where the code has a
weakness, the weakness is named here and cross-referenced to
[13 — Future Improvements](13_Future_Improvements.md).

![Control flow](../diagrams/algorithm_flowchart.png)

---

## 1. File structure

| Line | Item | Purpose |
|---|---|---|
| 1 | `#include <AFMotor.h>` | the only library |
| 2–9 | the eight pin `#define`s | every pin the sketch uses |
| 10–13 | four `AF_DCMotor` objects | one per shield channel |
| 15 | `blinkInterval` | the blink half-period, 200 ms |
| 16–41 | `setup()` | speeds, RNG seed, 2 s startup signal |
| 42–58 | `stopMotors()` | release + set all four indicators |
| 59–68 | `moveForward()` | all four forward, front pair lit |
| 69–88 | `moveBackward()` | 2 s reverse with a blinking rear pair |
| 89–110 | `turnLeft()` | 1 s left turn, right pair blinking |
| 111–132 | `turnRight()` | 1 s right turn, left pair blinking |
| 133–159 | `loop()` | the sensor read and the three cases |

Single file, 159 lines, seven functions, no classes, no `struct`, no headers of
its own, no serial output.

## 2. The pin map in code

```cpp
#define IR_FRONT_LEFT A0     // free
#define IR_FRONT_RIGHT A1    // free
#define IR_BACK_LEFT A2      // free
#define IR_BACK_RIGHT A3     // free
#define LED_FRONT_LEFT 6     // TAKEN - M3 speed PWM
#define LED_FRONT_RIGHT 0    // TAKEN - USB serial TX
#define LED_BACK_LEFT 12     // TAKEN - 74HC595 latch
#define LED_BACK_RIGHT 3     // TAKEN - M2 speed PWM
```

Named constants, and a `#define` rather than a `const` — which is the
conventional choice for pins in this style of sketch and costs nothing here.
The mapping from these names to physical positions is in
[12 §2](12_Final_Implementation.md#2-final-pin-assignment-as-written-in-the-sketch),
and the conflicts in [12 §7](12_Final_Implementation.md#7-pin-conflict--verification).

## 3. Two raw numbers and a nameless blink interval

Two values in the firmware are written inline instead of being named:

| Value | Where | Problem |
|---|---|---|
| `200` | four times, in the two `if` statements of `loop()` | the obstacle threshold is the single most important number in the sketch, and it is written out four times, so the two statements can drift apart |
| `randomSeed(analogRead(5))` | `setup()` | `A5` is not used for anything; the seed comes from a floating pin |

The blink half-period *is* named (`blinkInterval = 200`), but it is a file-scope
`const` declared between the pin defines and `setup()`, so it reads more like a
pin definition than a timing constant.

None of these are bugs. They are the points a reviewer would raise first, and
the fixes are small — see
[13 §3](13_Future_Improvements.md#3-three-naming-and-structure-fixes-worth-making).

## 4. Timing with `millis()`

`millis()` is used in three places only, and all three are the deadline test of
a timed `while` loop:

```cpp
while (millis() - moveStart < 2000) { …  delay(10);  }   // moveBackward()
while (millis() - turnStart  < 1000) { …  delay(10);  }   // turnLeft()
while (millis() - turnStart  < 1000) { …  delay(10);  }   // turnRight()
```

That combination is the important point. `millis()` is used for the *deadline*
and `delay(10)` for the *pacing*, so the durations are accurate to about 10 ms
and the loop body — which re-issues `run()` and re-writes the indicators — runs
100 times a second while it lasts. The blink toggle uses `millis()` too, against
`lastBlink` with the same 200 ms `blinkInterval`.

Everywhere else, the sketch waits with a bare `delay()`:

| Call | Duration |
|---|---|
| `delay(1000)` in `setup()` ×2 | 1 s on, 1 s off |
| `delay(1000)` before `moveBackward()` | 1 s |
| `delay(1000)` after `stopMotors(true)` in the front path | 1 s |
| `delay(1000)`, `delay(500)`, `delay(300)` in the rear path | 1 s, 0.5 s, 0.3 s |

So the sketch is *not* a non-blocking design. Its three `millis()` deadlines are
correct, but the ten `delay()` calls — and the bodies of the three timed loops
themselves — mean the loop is blocked for as long as any of them lasts. See §5.

## 5. Why that matters here

| Property | Value |
|---|---|
| Longest single block | 2000 ms, inside `moveBackward()` |
| Longest total unresponsiveness | 5000 ms, across the whole front-obstacle path |
| Sensor reads during a manoeuvre | none |
| LED blink during a manoeuvre | yes — the blink runs inside the three `while` loops |
| What the robot does if an obstacle appears mid-sequence | nothing, until the sequence ends |

The indicators are the one thing that keeps working, because the blink lives
inside the timed loops rather than being produced by a separate task.

`delay(10)` inside the loops also means the blink half-period of 200 ms is
resolved to 10 ms, so the blink is symmetric to within one poll.

## 6. Why the sensors are read with `analogRead()`

The sketch calls `analogRead()` on all four FC-51 `OUT` pins and compares the
result against `200`. The modules sit behind an LM393 comparator, so what
arrives at the pin is a digital level, and this test is a digital test written
in analog clothing.

| Consequence | Detail |
|---|---|
| It works | LOW reads ≈ 0, which is `< 200` |
| Inverted-polarity modules | do **not** work: HIGH would read ≈ 1023 and never trigger |
| Threshold tuning | the FC-51's own trimpot is the threshold; the `200` only has to sit between the two digital levels |
| Hysteresis | none |
| `A0` and `A1` | analog-only pins; they cannot be used with `digitalRead()` or `digitalWrite()` at all |

The digital alternative — `digitalRead()` on `A2`–`A5` with a single
`SENSOR_ACTIVE_LOW` switch — was written and then superseded. It is recorded in
[history/analog_sensor_experiment.md](../history/analog_sensor_experiment.md),
and the reasons are in
[13 §1](13_Future_Improvements.md#12-read-the-sensors-digitally).

## 7. `stopMotors(bool allLedsOn = false)`

A default argument used to give one function two meanings:

```cpp
void stopMotors(bool allLedsOn = false) {
  motor1.run(RELEASE); … motor4.run(RELEASE);
  if (allLedsOn) { all four indicators HIGH; }
  else           { all four indicators LOW;  }
}
```

| Call | Effect |
|---|---|
| `stopMotors(true)` | motors released, all four indicators on — the obstacle indication |
| `stopMotors()` | motors released, all four indicators off — the all-clear state |

Both turn functions end with `stopMotors(true)`, so a completed turn leaves all
four indicators lit until the next `loop()` pass calls `moveForward()`.

The name is slightly misleading — it also drives the indicators — but it is
consistent and it is called from six places, always as `stopMotors(true)`.

## 8. LED logic, written out

There is no bit mask and no table; the indicators are written directly.

| Function | FL | FR | BL | BR |
|---|---|---|---|---|
| `moveForward()` | HIGH | HIGH | LOW | LOW |
| `moveBackward()` | LOW | LOW | `blinkState` | `blinkState` |
| `turnLeft()` | LOW | LOW | LOW | `blinkState` |
| `turnRight()` | `blinkState` | `blinkState` | LOW | LOW |
| `stopMotors(true)` | HIGH | HIGH | HIGH | HIGH |
| `stopMotors()` | LOW | LOW | LOW | LOW |

`blinkState` is a local `bool` in each of the three timed functions, initialised
to `false`, which is why the first 200 ms of `moveBackward()` and of both turns
are dark before the first toggle.

## 9. Types used

| Type | Where |
|---|---|
| `unsigned long` | `blinkInterval`, and the `millis()` deadlines inside the loops |
| `int` | the four `analogRead()` results |
| `bool` | the `stopMotors()` parameter, and the two `blinkState` locals |
| `uint8_t`, `unsigned long` | from the library's own declarations, not from this sketch |

`random(2)` returns `int`; the result is only ever compared against `0`, so no
cast is needed.

## 10. `setup()` and `loop()`

`setup()` does five things, in this order:

1. `pinMode()` on the four sensor pins as `INPUT` and the four indicator pins
   as `OUTPUT`;
2. `setSpeed(150)` on all four motors — once, and never changed;
3. `randomSeed(analogRead(5))` — seeds the RNG from a floating pin;
4. all four indicators `HIGH`, `delay(1000)`;
5. all four indicators `LOW`, `delay(1000)`.

`loop()` reads, tests, and dispatches — the three cases are described in
[08 Algorithm](08_Algorithm.md). There is no `else` between the front and rear
tests other than the `else if`, so a front obstacle always wins over a rear one.

## 11. `AFMotor` and the direction constants

`AF_DCMotor motor1(1)` … `motor4(4)` bind the objects to shield channels M1–M4.
`run(FORWARD)`, `run(BACKWARD)` and `run(RELEASE)` are the library's own
constants. **No motor is ever inverted in software** — the left/right mirror is
a wiring fact, documented in
[05 §4](05_Wiring.md#4-motor-wiring-and-the-leftright-mirror) and in
[history/motor_testing.md](../history/motor_testing.md).

## 12. Quality checklist applied to the final file

| Check | Result |
|---|---|
| Compiles as a single `.ino` | yes — see §9 for the library install |
| Every `pinMode()` present for every pin used | yes, four and four |
| Every pin written by `digitalWrite()` also set to `OUTPUT` | yes |
| Speeds set for every motor before any `run()` | yes, in `setup()` |
| `millis()` used as a deadline, not as a delay | yes, in both loops |
| Blocking calls | **ten `delay()` calls and three `while` loops** — see §5 |
| Unused variables | none |
| Unused functions | none — all seven functions are called |
| `random()` seeded | yes, once, in `setup()` |
| Blocking vs non-blocking requirement | **not met** — see [13 §1](13_Future_Improvements.md#11-make-the-loop-non-blocking) |
| Pin conflict with the shield | **three conflicts** — see [12 §7](12_Final_Implementation.md#7-pin-conflict--verification) |

## 13. Where to look next

* [08 — Algorithm](08_Algorithm.md): the three cases, in order, with every duration.
* [12 — Final Implementation](12_Final_Implementation.md): the normative pinout
  and the conflict report.
* [10 — Troubleshooting](10_Troubleshooting.md): what to check when the robot
  misbehaves with blocking waits in the loop.
* [13 — Future Improvements](13_Future_Improvements.md): the ordered list of
  changes that would make this firmware match the documentation it used to have.
