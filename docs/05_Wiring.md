# 05 — Wiring

Every connection in the final build, and the reasoning behind the two places
where a beginner usually gets it wrong: the left/right motor mirror and the LED
resistor side.

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

| FC-51 | VCC | GND | OUT → |
|---|---|---|---|
| front left | 5 V rail | GND rail | `A2` (digital 16) |
| front right | 5 V rail | GND rail | `A3` (digital 17) |
| rear left | 5 V rail | GND rail | `A4` (digital 18) |
| rear right | 5 V rail | GND rail | `A5` (digital 19) |

All four modules are wired the same way. The VCC/GND pair may be daisy-chained
along a rail; the OUT wires are individual signal wires and are kept short and
away from the motor wiring.

> `OUT` is **LOW when an obstacle is present** on a standard FC-51 / LM393
> module. That is what `SENSOR_ACTIVE_LOW = 1` in the sketch encodes. If your
> module reports the opposite, change that one `#define` — see
> [09 Testing step 4](09_Testing.md#step-4--lowhigh-sensor-logic).

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

| LED | Pin | Resistor | Anode | Cathode |
|---|---|---|---|---|
| front left (red) | `D2` | in series | to resistor | GND rail |
| front right (red) | `D9` | in series | to resistor | GND rail |
| rear left (white) | `D10` | in series | to resistor | GND rail |
| rear right (white) | `D13` | in series | to resistor | GND rail |

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

### 3.1 Why the LEDs are on D2, D9, D10 and D13

The motor shield already owns D3, D4, D5, D6, D7, D8, D11 and D12. That leaves
exactly eight digital-capable pins — D2, D9, D10, D13 on the digital header,
plus A2, A3, A4, A5 in the analog header — but A2–A5 are
needed by the four IR sensors, so the four LEDs take D2, D9, D10 and D13. The
budget therefore has nothing spare, and the
choice of D13 is a consequence of the pin budget, not a preference; its
side-effect (the Uno's built-in LED mirrors the rear-right indicator) is
documented in
[12 Final Implementation §7](12_Final_Implementation.md#7-pin-conflict--verification).

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
| 11 | FC-51 front-left `OUT` | Uno `A2` | signal |
| 12 | FC-51 front-right `OUT` | Uno `A3` | signal |
| 13 | FC-51 rear-left `OUT` | Uno `A4` | signal |
| 14 | FC-51 rear-right `OUT` | Uno `A5` | signal |
| 15 | Uno `D2` | resistor → front-left LED anode | signal |
| 16 | Uno `D9` | resistor → front-right LED anode | signal |
| 17 | Uno `D10` | resistor → rear-left LED anode | signal |
| 18 | Uno `D13` | resistor → rear-right LED anode | signal |
| 19 | LED cathodes ×4 | GND rail | short black |

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
