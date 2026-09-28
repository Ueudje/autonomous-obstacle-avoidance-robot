# 04 — Electrical Architecture

This page describes the electrical *layers* of the robot: who powers what, which
grounds are shared, and at what level each signal lives. Connection-by-connection
detail is in [05 Wiring](05_Wiring.md).

![Wiring diagram](../diagrams/wiring_diagram.png)

---

## 1. The two domains

The robot contains two electrically different things, and keeping them apart is
the central design decision:

| Domain | Carries | Fed by |
|---|---|---|
| **Motor power** | 4 DC motors, 7.4 V nominal, high current peaks | the 18650 pack directly into the shield's `VIN` |
| **Logic power** | Uno, shield logic, 4 IR modules, 4 LEDs | the Uno's on-board 5 V regulator |

The two domains **share a common ground** and nothing else. Motor current must
not flow through the sensor wiring, or the IR readings become noise.

```
        2 x 18650 (2S, 7.4 V nominal)
                 |
        +--------+---------+
        |                  |
     +--+                GND rail  (common to everything)
     |  |
  shield VIN                          Uno GND
  (L293D motor supply)                 shield GND
        |                              4 x FC-51 GND
     4 x motors                       4 x LED cathodes
                                      Uno 5 V out
                                        |         |
                                  4 x FC-51 VCC   LED branch resistors
```

## 2. The 5 V rail

The Uno's on-board regulator produces 5 V for the logic side. The shield takes
its *logic* supply from the Uno header, and the shield's *motor* supply from
`VIN`. This is why the battery must not be wired to the 5 V pin.

The 5 V rail feeds:

* the four FC-51 `VCC` pins;
* the anode end of each LED branch, through its current-limiting resistor.

Total logic current is small compared with the motor load, which is the point:
the regulator was never asked to supply motor current.

## 3. The GND rail

One common ground connects:

* the battery negative terminal,
* the shield's `GND`,
* the Uno's `GND`,
* all four FC-51 `GND` pins,
* all four LED cathodes.

A daisy chain is acceptable; a star from the battery holder is better. What is
*not* acceptable is a separate "signal ground" that is not tied to the motor
return, because then the IR outputs float relative to the supply the shield is
switching.

## 4. Motor power path

| Element | Role |
|---|---|
| 2 × 18650 in series | energy source, 7.4 V nominal |
| Shield `VIN` / `GND` | motor supply input on the L293D side of the shield |
| L293D (×2 on the shield) | dual H-bridge, 4 half-bridges = 4 motors |
| 74HC595 | latches 4 direction bits + enable into the driver |
| Arduino PWM pins D3/D5/D6/D11 | speed command per channel |

The L293D is a bipolar driver with a significant voltage drop, so the motors see
less than the pack voltage. **The exact drop is to be verified** on the real
hardware before quoting a motor working voltage.

Current peaks matter more than the average: see
[11 Power System](11_Power_System.md#3-why-a-motor-robot-needs-a-strong-source).

## 5. Signal levels

All logic is 5 V CMOS. There is no 3.3 V part and no level shifter in this build.

| Signal | Driver | Receiver | Level |
|---|---|---|---|
| FC-51 `OUT` → `A2..A5` | LM393 comparator output | ATmega328P GPIO with internal pull-up | 0 V / ≈ 5 V |
| Uno `D2/D9/D10/D13` → LED anode | ATmega328P GPIO | LED + series resistor | 0 V / ≈ 5 V |
| Uno `D3/D5/D6/D11` → shield PWM | ATmega328P PWM | L293D enable | 0 V / 5 V |
| Uno `D4/D7/D8/D12` → shield | ATmega328P GPIO | 74HC595 | 0 V / 5 V |

`digitalRead()` on `A2..A5` reads those pins as ordinary digital inputs; the
ATmega328P enables the input buffer on the analog pins when they are used as
GPIO. `pinMode(pin, INPUT)` is still called in `setup()` so the intent is
explicit even though the internal pull-ups are left disabled — the FC-51 output
stage drives the line.

## 6. Grounding and noise

* Motor wires are routed away from the IR sensor wires, and are twisted where
  practical.
* The breadboard carries only low-current signal wiring; no motor current passes
  through it.
* The Uno is the single reference point for all logic.
* The shield sits directly on the Uno headers, which is also the shortest
  possible path for the motor-control signals.

## 7. The physical pin header is not the electrical role

An important distinction that the project documentation makes repeatedly:

* A2–A5 are **printed** in the ANALOG header of the Uno.
* A2–A5 are **ordinary GPIO pins** (digital 16, 17, 18, 19) and can be used with
  `digitalRead()`, `digitalWrite()` and `pinMode()`.
* A0 and A1 cannot: on the ATmega328P they are analog-input-only and
  `digitalRead()`/`digitalWrite()` do not work on them.

This is why the four IR modules are on A2–A5 and not on A0/A1. The full check is
in [12 Final Implementation §7](12_Final_Implementation.md#7-pin-conflict--verification).

## 8. The complete connection list

| From | To | Function |
|---|---|---|
| Battery `+` | shield `VIN` | 7.4 V nominal motor supply |
| Battery `−` | common GND rail | return for the whole robot |
| Uno `GND` | GND rail | logic reference |
| Shield `GND` | GND rail | logic reference |
| Shield `M1` | front-left motor | channel 1 |
| Shield `M2` | back-left motor | channel 2 |
| Shield `M3` | back-right motor | channel 3 |
| Shield `M4` | front-right motor | channel 4 |
| Uno `5V` | FC-51 `VCC` ×4 | module supply |
| FC-51 `GND` ×4 | GND rail | module return |
| FC-51 `OUT` ×4 | Uno `A2, A3, A4, A5` | obstacle input |
| Uno `D2` | resistor → `LED_FRONT_LEFT` anode | indicator |
| Uno `D9` | resistor → `LED_FRONT_RIGHT` anode | indicator |
| Uno `D10` | resistor → `LED_REAR_LEFT` anode | indicator |
| Uno `D13` | resistor → `LED_REAR_RIGHT` anode | indicator |
| LED cathodes ×4 | GND rail | indicator return |

Next: [05 Wiring](05_Wiring.md) shows how these are physically routed.
