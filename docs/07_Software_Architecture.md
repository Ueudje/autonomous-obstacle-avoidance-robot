# 07 — Software Architecture

How [`src/robot/Robot.ino`](../src/robot/Robot.ino) is organised, and why each
programming technique in the brief is used where it is used.

---

## 1. File structure of the sketch

The firmware is a single `.ino` file, sectioned in this order:

| Section | Content |
|---|---|
| Header comment | platform, hardware, behaviour, timing model |
| 1 | `AFMotor` objects, one per channel, and `MOTOR_SPEED` |
| 2 | `#define` pin names (sensors and LEDs) |
| 3 | `SENSOR_ACTIVE_LOW` |
| 4 | LED bit masks |
| 5 | timing constants |
| 6 | state enum, state variable, `millis()` bookkeeping, sensor flags |
| — | LED task: `setLed`, `applyLeds`, `setLeds`, `startBlink`, `updateBlink` |
| — | sensor task: `isObstacle`, `readSensors`, `frontBlocked`, `rearBlocked` |
| — | motor task: `setMotorSpeed`, `driveAllForward`, `driveAllBackward`, `stopMotors`, `turnInPlace` |
| — | state helpers: `changeState`, `stateElapsed` |
| — | behaviour: one `beginX()` per state |
| — | `updateRobot()`, `setup()`, `loop()` |

Three tasks — LEDs, sensors, motors — are kept separate on purpose. They do not
call each other; the state machine decides what each one should be doing.

## 2. The pin map in code

```cpp
#define IR_FRONT_LEFT   A2   // digital pin 16
#define IR_FRONT_RIGHT  A3   // digital pin 17
#define IR_REAR_LEFT    A4   // digital pin 18
#define IR_REAR_RIGHT   A5   // digital pin 19

#define LED_FRONT_LEFT  2
#define LED_FRONT_RIGHT 9
#define LED_REAR_LEFT   10
#define LED_REAR_RIGHT  13
```

`#define` is used rather than variables because a pin name is a compile-time
constant: the Arduino preprocessor substitutes the text, so there is no RAM cost
and no runtime indirection.

`A2` is not a magic number — it is the Arduino's own alias for digital pin 16.

## 3. LED logic as a bit mask

```cpp
const uint8_t LED_FL_BIT = 0x01;
const uint8_t LED_FR_BIT = 0x02;
const uint8_t LED_RL_BIT = 0x04;
const uint8_t LED_RR_BIT = 0x08;

const uint8_t LED_FRONT_PAIR = LED_FL_BIT | LED_FR_BIT;
const uint8_t LED_REAR_PAIR  = LED_RL_BIT | LED_RR_BIT;
const uint8_t LED_LEFT_SIDE  = LED_FL_BIT | LED_RL_BIT;
const uint8_t LED_RIGHT_SIDE = LED_FR_BIT | LED_RR_BIT;
const uint8_t LED_ALL        = LED_FRONT_PAIR | LED_REAR_PAIR;
```

`uint8_t` is an unsigned 8-bit integer: one byte, values 0–255. A mask of four
bits fits in it, and `mask & LED_FL_BIT` is a single bit test.

The four *groups the behaviour actually needs* are then named once, and the
whole LED behaviour reads as `setLeds(LED_FRONT_PAIR)` rather than four separate
writes.

## 4. Timing with `millis()`

The pattern used throughout is:

```cpp
if (millis() - previousPollMs >= OBSTACLE_POLL_MS) {
  previousPollMs = millis();
  ...
}
```

In words: *store when something last happened; on every pass through the loop,
ask how long it has been since; act only when enough time has passed.*

Why it is written that way:

* `loop()` runs thousands of times per second, and each pass is cheap — it only
  checks a subtraction.
* `millis()` returns an `unsigned long` (32-bit) that wraps after ≈ 49.7 days.
  Writing the test as `now - previous >= interval` instead of
  `now >= previous + interval` keeps the comparison correct across the wrap,
  because unsigned arithmetic wraps to the correct value in exactly the same
  way.
* Nothing ever waits. The sensor scan, the LED blink and the motor command are
  all updated on every pass.

### 4.1 Why not `delay()`

`delay(700)` does not wait "a bit" — it **stops the whole program**. For the
duration of the call the Arduino executes nothing else: no sensor is read, no
LED blinks, no motor command is issued. Everything is frozen.

That matters here because the robot has several things to do at once:

| Task | Needs to run |
|---|---|
| scan the four IR sensors | every 50 ms, continuously |
| blink the active LED group | every 250 ms, continuously |
| drive / reverse / turn | continuously, with a deadline |
| decide what to do next | on every pass |

With `delay()`, a 700 ms reverse would also blind the robot for 700 ms: it
could not notice that something moved into its path, and the LEDs would freeze
mid-blink. The robot would keep repeating the same reaction on a stale sensor
picture — which is exactly the failure described in
[history/initial_concept.md](../history/initial_concept.md).

The fix was to turn the sequence into a state machine with deadlines. Nothing
waits; each state is left as soon as its own timer expires.

## 5. `random()` and `randomSeed()`

```cpp
bool turnLeft = (random(0, 2) == 0);
...
randomSeed(micros());
```

`random(0, 2)` returns 0 or 1, giving an even left/right choice. Without a seed
the Arduino's pseudo-random generator replays the *same* sequence on every
power-up, so a robot that always turns left first would build a recognisable
pattern into its room. `randomSeed(micros())` seeds it from the microsecond
timer at boot so the choice differs between runs.

## 6. Why the sensors are read digitally

The final implementation uses `digitalRead()` and **never** calls
`analogRead()`. The FC-51 module's `OUT` pin is a digital output from an LM393
comparator: it is a valid logic level, not a proportional measurement. See
[history/analog_sensor_experiment.md](../history/analog_sensor_experiment.md)
for the analog approach that was explored first and then dropped.

The pins are on the ANALOG header, which is legal and intentional:

* A0–A5 are printed in the analog header, but on the ATmega328P they are six
  general-purpose I/O pins.
* A0–A5 map to digital pins 14–19; A2–A5 (16–19) can be used with
  `pinMode()`, `digitalRead()` and `digitalWrite()`.
* A0 and A1 (digital 14 and 15) are **analog input only** — `digitalRead()` and
  `digitalWrite()` do not work on them. That is why they are not used.

The physical header a pin sits in and the way software drives it are two
different questions.

## 7. `stopMotors(bool)` and a default parameter

```cpp
void setLed(uint8_t pin, bool on = true)
void stopMotors(bool indicateStop)
void startBlink(uint8_t mask, bool levelOn = true)
```

`stopMotors(true)` releases the motors **and** lights all four LEDs;
`stopMotors(false)` releases the motors and leaves the indicators alone. The
rear-obstacle sequence needs the second form, because it must show "stop" with
the *front* pair still lit — see [08 Algorithm §5](08_Algorithm.md#5-rear-obstacle-sequence).

Default parameters (`bool on = true`) remove repetition at the call site
without hiding what the function does.

## 8. Types used

| Type | Where | Why |
|---|---|---|
| `const uint8_t` | `MOTOR_SPEED`, LED masks, blink mask | small constants and bit fields; `const` documents intent |
| `const unsigned long` | all `*_MS` timing constants | 32-bit, matches the return type of `millis()` |
| `unsigned long` | `stateStartedAt`, `previousPollMs`, `previousBlinkMs` | timestamps, must not go negative |
| `bool` | `obstacleFrontLeft` … `rearRight`, `blinkLevel` | a sensor result is a yes/no; `int` would invite `1`/`2`/`3` confusion |
| `enum RobotState` | the eight states | the compiler rejects a state value that does not exist |
| `uint8_t` parameter | `applyLeds(uint8_t mask)` | a mask is 8 bits wide |

`bool` is used deliberately for the four obstacle flags. The brief calls for
`frontBlocked()` to be `obstacleFrontLeft || obstacleFrontRight`, and with `bool`
that expression can only be `true` or `false`. Assigning a raw `digitalRead()`
result straight into a `bool` would also work, but then the "which level means
obstacle" decision would be scattered across four places instead of being
handled once in `isObstacle()`.

## 9. `setup()` and `loop()`

```cpp
void setup() {
  pinMode(IR_FRONT_LEFT,  INPUT);    // inputs first, so nothing floats
  ...
  pinMode(LED_FRONT_LEFT, OUTPUT);
  ...
  applyLeds(0);
  stopMotors(false);                 // no twitch while configuring
  randomSeed(micros());
  beginStartup();
}

void loop() {
  updateRobot();
}
```

`setup()` runs once, `loop()` runs forever. `loop()` contains exactly one
statement: the non-blocking update. Everything interesting happens in
`updateRobot()`.

## 10. `AFMotor` and the direction constants

`AFMotor` drives one channel per object. The direction constants come from the
library:

| Constant | Meaning |
|---|---|
| `FORWARD` | run the motor forward for that channel |
| `BACKWARD` | run the motor backward for that channel |
| `RELEASE` | coast — outputs high-impedance, no drive, no braking |
| `setSpeed(n)` | PWM duty for that channel, 0–255 |

The firmware issues all four commands through three small helpers
(`driveAllForward`, `driveAllBackward`, `stopMotors`) and one turn helper
(`turnInPlace`), so no motor command is ever written twice at two different
places.

There is **no per-motor inversion in the software.** Turning a motor the wrong
way is a wiring problem, and it was fixed in the wiring — see
[05 Wiring §4](05_Wiring.md#4-motor-wiring-and-the-leftright-mirror).

## 11. Code-quality checklist applied to the final file

| Check | Result |
|---|---|
| Syntax | balanced braces, every statement terminated, every function defined before use |
| Logical conditions | `frontBlocked()` / `rearBlocked()` use `||` on `bool` values; the front test is evaluated first, so a front obstacle wins over a rear one |
| Motor mapping | M1 FL, M2 BL, M3 BR, M4 FR — one object per channel |
| Sensor mapping | A2 FL, A3 FR, A4 RL, A5 RR, consistent between wiring and code |
| LED mapping | D2 FL, D9 FR, D10 RL, D13 RR |
| Timing logic | every state has exactly one exit condition and one next state |
| Function organisation | one `beginX()` per state; `switch` in one place |
| `millis()` | used for the poll, the blink and every state deadline |
| Blocking delays | none — no `delay()`, no `delayMicroseconds()` |
| Unused variables | none; every declared identifier is read or written |
| Boolean expressions | no `=` where `==` is meant; no implicit truthiness bugs |
| Pin conflicts | checked against the AFMotor source — see [12 §7](12_Final_Implementation.md#7-pin-conflict--verification) |
| Library compatibility | `AFMotor` with an Uno, shield V1 pinout |

## 12. Where to look next

* The behaviour itself: [08 Algorithm](08_Algorithm.md)
* The flowchart: [`diagrams/algorithm_flowchart.png`](../diagrams/algorithm_flowchart.png)
* The final, normative description: [12 Final Implementation](12_Final_Implementation.md)
