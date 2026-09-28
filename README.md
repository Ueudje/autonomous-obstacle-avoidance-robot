# Autonomous Obstacle-Avoidance Robot

A four-wheel Arduino robot that drives itself around a room and avoids
obstacles, documented end to end: the hardware, every wire, the firmware, the
tests that were run, the faults that were found, and the limits of what was
actually measured.

![Robot overview](images/robot_overview.png)

The robot is finished and the documentation is complete. Where a value was
never measured, it says **to be verified** instead of carrying an invented
number.

---

## What it does

| Situation | What the robot does |
|---|---|
| Power-up | 3 s safety delay, all four LEDs blinking |
| Clear path | drives forward, front LEDs on, rear LEDs off |
| Obstacle in front | all LEDs on, 500 ms → reverse 700 ms (rear LEDs blink) → turn 600 ms (that side's LEDs blink) → forward again |
| Obstacle behind | front LEDs stay on, 300 ms → drive forward 600 ms (rear LEDs blink) → settle 300 ms → forward again |
| Turn direction | left or right, chosen at random **once** when the turn begins |

## The robot

![Side view](diagrams/robot_side_view.png)
![Top view](diagrams/robot_top_view.png)

| | |
|---|---|
| Controller | Arduino Uno (ATmega328P) |
| Motor driver | L293D / Adafruit Motor Shield V1 compatible, driven by the `AFMotor` library |
| Motors | 4 × DC geared motors, `MOTOR_SPEED = 150` |
| Sensors | 4 × FC-51 IR module, digital `OUT`, one per corner, at ≈ 1.8 cm |
| Indicators | 4 × LED — 2 red at the front, 2 white at the rear — at ≈ 5.2 cm |
| Power | 2 × 18650 in series, 7.4 V nominal to the motor rail |

**Motors:** `M1` front-left, `M2` back-left, `M3` back-right, `M4` front-right.
The left and right pairs are wired as a **mirror** — the right-hand motors have
their terminal order swapped — which is why the firmware contains no per-motor
direction inversion.

**Sensors:** `A2` front-left, `A3` front-right, `A4` rear-left, `A5` rear-right.
A standard FC-51 pulls `OUT` **LOW** when it sees an obstacle; the firmware
handles that with one switch, `SENSOR_ACTIVE_LOW`.

**Pin map:**

| Pin | Use |
|---|---|
| `A2` `A3` `A4` `A5` | the four IR inputs |
| `D2` `D9` `D10` `D13` | the four LEDs |
| `D3`–`D8`, `D11`, `D12` | reserved by the motor shield |

The pin budget is fully spent: exactly eight digital-capable pins are free with
this shield, and the design needs eight. `D13` is the Uno's built-in LED pin, so
the onboard LED mirrors the rear-right indicator — that side effect, and the
check it came from, are documented in
**[12 — Final Implementation §7](docs/12_Final_Implementation.md#7-pin-conflict--verification)**.

![Pin map](diagrams/pin_map_diagram.png)

## How it is built

```text
             +---------------------+
  FC-51 x4   |     Arduino Uno     |   LEDs x4
  A2 A3 A4 A5|                     |   D2 D9 D10 D13
  ---------->|                     |---------------> 2 red front
  GND        |   AF Motor Shield   |                 2 white rear
  5V         |   (L293D + 74HC595) |
  <----------|  M1 M2 M3 M4       |----> 4 DC geared motors
             +---------------------+
                        ^
                        |
             2 x 18650 in series (7.4 V nominal)
```

![System block diagram](diagrams/system_block_diagram.png)
![Wiring diagram](diagrams/wiring_diagram.png)

Motor current never passes through the Uno's 5 V regulator: the pack feeds the
shield's `VIN` directly, and only the sensors and LEDs use the logic rail.

## The firmware

[`src/robot/Robot.ino`](src/robot/Robot.ino) — one file, no blocking delays.

```text
STARTUP → FORWARD ─┬─ front blocked → STOP_HOLD → REVERSE → TURN ─┐
                  └─ rear blocked  → REAR_BRAKE → REAR_CREEP → REAR_SETTLE ─┤
                                                                              │
                                     ◄─────────────────────────────────────────┘
```

* eight named states, each with one timed exit
* every duration is a `millis()` deadline: **no `delay()` anywhere**
* `readSensors()` only in `STATE_FORWARD`, on a 50 ms poll; front test first
* `updateBlink()` runs on every pass, so an indicator never freezes mid-blink
* LED behaviour is expressed as bit masks, not four scattered `digitalWrite` calls

![Algorithm flowchart](diagrams/algorithm_flowchart.png)

## Build it

Hardware assembly: **[06 — Mechanical Construction](docs/06_Mechanical_Construction.md)** (14 steps)
Wiring: **[05 — Wiring](docs/05_Wiring.md)** · Safety checklist: **[12 §11](docs/12_Final_Implementation.md#11-safety-checklist-before-powering)**

Firmware — **Arduino IDE 1.8.19**:

1. **Sketch → Include Library → Manage Libraries…** → install **Adafruit Motor
   Shield Library** by Adafruit (it provides `AFMotor.h`; it is not bundled
   with the IDE).
2. **File → Open…** → `src/robot/Robot.ino`.
3. **Tools → Board → Arduino Uno**; **Tools → Port** → your board.
4. **Verify** (✓), then **Upload** (→).

Full instructions, including the two most common upload errors, are in
**[12 §9](docs/12_Final_Implementation.md#9-compile-and-upload--arduino-ide-1819)**.

## Test it

Proceed in stages — **one motor at a time, on blocks, before the robot ever
touches the floor.** The order and the expected readings are in
**[09 — Testing](docs/09_Testing.md)**; when something behaves wrongly, start at
**[10 — Troubleshooting](docs/10_Troubleshooting.md)**.

## Documentation

### The design

| Document | Contents |
|---|---|
| [01 — Project Overview](docs/01_Project_Overview.md) | what it is, why it exists, doc map |
| [02 — Requirements](docs/02_Requirements.md) | functional / non-functional requirements, constraints |
| [03 — Hardware](docs/03_Hardware.md) | every component, and the *to be verified* list |
| [04 — Electrical Architecture](docs/04_Electrical_Architecture.md) | the two power domains, the 5 V rail, the pin header |
| [05 — Wiring](docs/05_Wiring.md) | every connection, the left/right motor mirror |
| [06 — Mechanical Construction](docs/06_Mechanical_Construction.md) | the 14 build steps |
| [07 — Software Architecture](docs/07_Software_Architecture.md) | loop, constants, each function, LED masks |
| [08 — Algorithm](docs/08_Algorithm.md) | pseudocode, the eight states, all timings |
| [09 — Testing](docs/09_Testing.md) | stages A–G, and what was **not** measured |
| [10 — Troubleshooting](docs/10_Troubleshooting.md) | faults, their causes, and their fixes |
| [11 — Power System](docs/11_Power_System.md) | the "6800 mAh" problem, brown-outs, cell safety |
| **[12 — Final Implementation](docs/12_Final_Implementation.md)** | **the normative document: the robot as it is** |
| [13 — Future Improvements](docs/13_Future_Improvements.md) | what it does not do, honestly |

### How it got here

Historical documents record the *approach* and what replaced it. They are not
specifications — where they disagree with document 12, document 12 is right.

| Stage | What happened |
|---|---|
| [Initial concept](history/initial_concept.md) | the first idea and the first `delay()` sketch |
| [Analog sensor experiment](history/analog_sensor_experiment.md) | why `analogRead()` on an FC-51 was abandoned |
| [Motor testing](history/motor_testing.md) | the robot twisted on the spot, and the left/right mirror fix |
| [Power testing](history/power_testing.md) | the "6800 mAh" cells, and the brown-out that looked like a bug |
| [Final changes](history/final_changes.md) | the 13 changes that produced the final design |

## Diagrams and images

| File | Shows |
|---|---|
| `diagrams/system_block_diagram.png` | the four subsystems and their interfaces |
| `diagrams/algorithm_flowchart.png` | the eight-state machine with every timing |
| `diagrams/robot_top_view.png` | sensor and LED positions, the pin letters |
| `diagrams/robot_side_view.png` | the two deck heights |
| `diagrams/robot_front_view.png` | front and rear LED layout, the two IR corners |
| `diagrams/wiring_diagram.png` | the complete wiring, as drawn |
| `diagrams/pin_map_diagram.png` | the shield's reserved pins vs. the eight that remain |
| `diagrams/robot_final_concept.png` | the final machine, seen from the side |
| `images/robot_overview.png` | the whole robot, isometric |
| `images/hardware_setup.png` | the Uno, the shield, and the battery |
| `images/chassis_assembly.png` | the two-level chassis |
| `images/testing.png` | the bench test sequence |

## Known limits

Stated here so nothing is discovered the hard way:

* **No runtime figure.** The pack was never characterised under load.
* **No current measurement.** No current probe was available.
* **The "6800 mAh" label was not trusted** — and the cells that carried it
  under-performed for reasons documented in
  [11 §2](docs/11_Power_System.md#2-the-6800-mah-problem).
* **The 1.8 s manoeuvre is not interrupted.** Sensors are read in
  `STATE_FORWARD` only; an obstacle that appears mid-sequence is not acted on
  ([08 §7](docs/08_Algorithm.md#7-why-the-sensor-read-is-inside-the-forward-case)).
* **The turn is timed, not measured.** `TURN_MS = 600` covers a different angle
  on a different floor.
* **The robot drifts.** No encoders, so equal PWM is assumed to mean equal speed.
* **`D13` also lights the Uno's onboard LED**, and `D9`/`D10` are unusable for
  LEDs if a servo is ever added.
* **Detection range per surface was never characterised** — the FC-51 trimpots
  are adjusted by hand, against whatever is in front of the robot.

## License

MIT — see [LICENSE](LICENSE).
