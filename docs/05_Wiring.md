# 05 — Wiring

Every connection in the build, and the reasoning behind the three places where a
beginner usually gets it wrong: the left/right motor mirror, the LED resistor
side, and the indicator pinout.

> **Read §2 and §3.1 together.** The pin numbers below are the ones written in
> [`src/robot/Robot.ino`](../src/robot/Robot.ino), and the supplied firmware puts
> all four indicators on pins the motor shield already owns. That is a fault in
> the firmware as supplied, not a wiring choice: the derivation is in
> [12 §7](12_Final_Implementation.md#7-pin-conflict--verification) and the fix is
> in [13 §2](13_Future_Improvements.md#21-move-the-indicators-off-the-shields-pins).
> Wiring the indicators to `D2`, `D9`, `D10`, `D13` instead, as §3.1 explains, is
> what makes the board work.

![Wiring diagram](../diagrams/wiring_diagram.png)

---

## 1. Power first

```
battery +  ─────────────►  shield VIN          (7.4 V nominal, motor side)
battery −  ─────────────►  GND rail            (common to everything)
shield GND ─────────────►  GND rail
Uno   GND ─────────────►  GND rail
```

The battery is wired **to the shield, not to the Uno's 5 V pin**. The shield
separates the motor supply (L293D side, `VIN`) from the logic supply (taken from
the Uno header). Wiring the pack into the 5 V pin would put 7.4 V into a 5 V
rail and, worse, ask the Uno's regulator to carry motor current.

The Uno must also be powered from the same pack through the shield (or over USB
during programming) so that the shield's 74HC595 has a valid 5 V.

## 2. Sensors: the four IR inputs

The pin numbers in the first column are what the sketch reads. The second
column is what the documentation recommends, and they differ.

| FC-51 | VCC | GND | `OUT` in the sketch | `OUT` recommended |
|---|---|---|---|---|
| front left | 5 V rail | GND rail | `A0` | `A2` (digital 16) |
| front right | 5 V rail | GND rail | `A1` | `A3` (digital 17) |
| rear left | 5 V rail | GND rail | `A2` | `A4` (digital 18) |
| rear right | 5 V rail | GND rail | `A3` | `A5` (digital 19) |

All four modules are wired the same way. The VCC/GND pair may be daisy-chained
along a rail; the OUT wires are individual signal wires and are kept short and
away from the motor wiring.

**The four sketch pins are legal.** `A0`–`A3` are real input pins and the sketch
calls `analogRead()` on them and nothing else, so there is no fault here. What
they cost is the option of switching to `digitalRead()` later: `A0` and `A1` are
analog-only on the ATmega328P and cannot be used as digital pins at all, so a
digital rework needs the sensors moved to `A2`–`A5`. That move is four
`#define` lines — see
[13 §1](13_Future_Improvements.md#12-read-the-sensors-digitally).

> `OUT` is **LOW when an obstacle is present** on a standard FC-51 / LM393
> module, and the supplied sketch relies on that: it calls `analogRead()` and
> tests `value < 200`, which works only because the comparator output is LOW
> while an obstacle is in range. There is **no** `SENSOR_ACTIVE_LOW` switch in
> this firmware, so a module that pulls `OUT` high when blocked will never
> trigger — see
> [08 §3](08_Algorithm.md#3-the-sensor-read). Measuring the actual `OUT` level
> is step 6 of the bench procedure in
> [09 Testing](09_Testing.md#7-stage-e--one-ir-sensor-at-a-time).

## 3. LEDs: anode, cathode, and where the resistor goes

An LED has exactly two terminals:

* **Anode** — the **longer** leg, the **positive** side, marked on the body of
  most 5 mm LEDs by the longer lead and often by a flat spot on the rim marking
  the cathode side.
* **Cathode** — the **shorter** leg, the **negative** side. The flat spot on the
  rim of the LED body is on the cathode side.

**The current-limiting resistor can be placed in series on either side of the
LED.** Electrically, current flows through the LED and the resistor in the same
series loop, so it does not matter which of the two is closer to the pin. What
matters is that a resistor is in the loop, that it is the right value, and that
the LED is not connected directly across 5 V.

This build puts the resistor **between the Arduino pin and the LED anode**:

```
Uno D2  ──[ R ]──▶|── GND          (anode on the pin side, cathode to GND)
              LED
```

| LED | Pin in the sketch | Pin recommended | Resistor | Anode | Cathode |
|---|---|---|---|---|---|
| front left (red) | `D6` | `D2` | in series | to resistor | GND rail |
| front right (red) | `D0` | `D9` | in series | to resistor | GND rail |
| rear left (white) | `D12` | `D10` | in series | to resistor | GND rail |
| rear right (white) | `D3` | `D13` | in series | to resistor | GND rail |

The right-hand column is what a working build uses. The middle column is what
the sketch drives, and three of those four pins belong to the motor shield.

**Resistor value: to be verified.** The brief only says "current-limiting
resistors are used". The practical procedure:

1. Read the LED forward voltage from its datasheet or part marking.
2. Choose a forward current, typically 5–10 mA for a 5 mm indicator.
3. `R = (5 V − Vf) / I` → for a red LED at 2.0 V and 10 mA that is ≈ 300 Ω; a
   330 Ω resistor is the usual next standard value.
4. Confirm brightness by eye and by measuring the voltage drop across the
   resistor with the robot powered.

The four LED wires are routed from the chassis up to the breadboard, where the
resistors are seated, and from the breadboard to the Uno's digital header. The
resistors live on the breadboard, the LED legs are either long enough to reach
it directly or are extended with soldered, heat-shrunk joints.

### 3.1 Why the four indicator pins in the sketch are wrong

`AFMotor` reserves eight pins: the four speed PWM lines `11, 3, 6, 5` for
channels M1–M4, and `12, 4, 8, 7` for the 74HC595 latch, clock, data and
enable. The Uno's `D0` and `D1` are the USB serial lines. Comparing that with
the four LED pins in the sketch:

| Sketch pin | Indicator | Owner | Result |
|---|---|---|---|
| `D6` | front left | M3 speed PWM | **conflict** |
| `D0` | front right | USB serial TX | **conflict** |
| `D12` | rear left | 74HC595 latch | **conflict** |
| `D3` | rear right | M2 speed PWM | **conflict** |

Writing to `D6` or `D3` contends with the shield's own PWM output; writing to
`D12` contends with the shift-register latch the shield drives inside
`latch_tx()`; and `pinMode(0, OUTPUT)` removes the serial port from the board,
so the Serial Monitor dies and the auto-reset upload path can be blocked.

**What is still free:** `D2`, `D9`, `D10`, `D13` — four pins, all LED-capable,
all off the shield. That is exactly the number of indicators needed, so the fix
is four `#define` lines and no hardware change:

| `#define` | Current | Change to |
|---|---|---|
| `LED_FRONT_LEFT` | `6` | `2` |
| `LED_FRONT_RIGHT` | `0` | `9` |
| `LED_BACK_LEFT` | `12` | `10` |
| `LED_BACK_RIGHT` | `3` | `13` |

`D13` has the Uno's built-in LED on it, so the rear-right indicator will also
make the board's own LED light; that is a cosmetic side effect, not a fault.
Note that `D13` is **not** a PWM pin, so if the indicators are ever to be
dimmed rather than switched, `D13` is the one to avoid.

Full verification, with the pin-source citations, is in
[12 §7](12_Final_Implementation.md#7-pin-conflict--verification).

## 4. Motor wiring and the left/right mirror

**A DC motor does not have a permanent "VCC" and "GND" terminal.** Unlike a
sensor, a motor has just two terminals, and the H-bridge inside the L293D
reverses the polarity across them in order to change direction. Whether a motor
turns clockwise or anticlockwise therefore depends on two things at once:

1. which of its two terminals is connected to which H-bridge output, and
2. the direction the wheel is mounted in on the chassis.

Red and black wire colours are used in this project purely as an **organisational
convention** so that the two ends of every motor can be told apart during
assembly. Red is *not* permanently "+5 V" on a motor and black is *not*
permanently ground.

Because a left wheel and a right wheel are mounted as mirror images of each
other, the same physical wire order makes one side spin forward and the other
side spin backward. The fix is to **mirror the wiring between the left and right
motors**:

| Channel | Side | Terminal 1 | Terminal 2 |
|---|---|---|---|
| M1 | front left | red | black |
| M2 | back left | red | black |
| M3 | back right | black | red |
| M4 | front right | black | red |

With that mirror in place, one software command — `motor.run(FORWARD)` on all
four channels — makes the robot drive forward. Without it, the robot spins on
the spot or drives backwards, and the correct fix is **in the wiring, not in the
code**: the firmware contains no per-motor inversion.

This was found by testing, not by reading a datasheet. The full record is in
[history/motor_testing.md](../history/motor_testing.md); the failure and the fix
are also in [10 Troubleshooting §1–§3](10_Troubleshooting.md#1-front-motors-move-in-the-opposite-direction).

## 5. Full connection table

Rows 11–14 and 15–18 give the sketch's pins first, because that is what the
firmware expects, and then the pins to use. **Wiring the second column is what
makes the board work.**

| # | From | To | Cable |
|---|---|---|---|
| 1 | battery `+` | shield `VIN` | heavy red |
| 2 | battery `−` | GND rail | heavy black |
| 3 | shield `GND` | GND rail | short black |
| 4 | Uno `GND` | GND rail | short black |
| 5 | shield `M1` | front-left motor terminal 1 / 2 | red / black |
| 6 | shield `M2` | back-left motor terminal 1 / 2 | red / black |
| 7 | shield `M3` | back-right motor terminal 2 / 1 | red / black (mirrored) |
| 8 | shield `M4` | front-right motor terminal 2 / 1 | red / black (mirrored) |
| 9 | 5 V rail | FC-51 `VCC` ×4 | red |
| 10 | GND rail | FC-51 `GND` ×4 | black |
| 11 | FC-51 front-left `OUT` | Uno `A0` — use `A2` | signal |
| 12 | FC-51 front-right `OUT` | Uno `A1` — use `A3` | signal |
| 13 | FC-51 rear-left `OUT` | Uno `A2` — use `A4` | signal |
| 14 | FC-51 rear-right `OUT` | Uno `A3` — use `A5` | signal |
| 15 | Uno `D6` | resistor → front-left LED anode — use `D2` | signal |
| 16 | Uno `D0` | resistor → front-right LED anode — use `D9` | signal |
| 17 | Uno `D12` | resistor → rear-left LED anode — use `D10` | signal |
| 18 | Uno `D3` | resistor → rear-right LED anode — use `D13` | signal |
| 19 | LED cathodes ×4 | GND rail | short black |

"Use `A2`" and so on means: change the corresponding `#define` in the sketch to
that pin, then wire to it. Those edits are listed in
[13 §1](13_Future_Improvements.md#12-read-the-sensors-digitally) and
[13 §2](13_Future_Improvements.md#21-move-the-indicators-off-the-shields-pins);
neither has been made, because the firmware is published as supplied.

## 6. Cable routing

* Motor cables are the only heavy-gauge wires; they run along the chassis edge
  and are tied down so they cannot touch a wheel.
* Sensor and LED cables are thin hook-up wire, routed on the opposite side of
  the chassis and kept off the motor return path.
* Battery leads go to the shield directly and are not shared with the breadboard.
* Everything is checked again with a continuity test before the robot is powered
  for the first time — the checklist is in
  [12 Final Implementation §11](12_Final_Implementation.md#11-safety-checklist-before-powering).

Next: [06 Mechanical Construction](06_Mechanical_Construction.md).
