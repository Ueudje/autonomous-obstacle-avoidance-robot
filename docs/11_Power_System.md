# 11 — Power System

Why a four-motor robot needs a power source that is judged on **current**, not
on the number printed on the cell. This page also records the battery
experiments of the project as history, not as a general claim about 18650 cells.

---

## 1. The four quantities that are constantly confused

| Quantity | Symbol | What it is | What it decides | How to check it |
|---|---|---|---|---|
| **Voltage** | V | the electrical potential a source holds | whether the motor is driven hard enough to turn | multimeter, unloaded and loaded |
| **Current capability** | A (max) | the largest current the source can deliver *without collapsing* | whether the rail sags when four motors start | datasheet `max continuous discharge`, or a load test |
| **Capacity** | mAh / Ah | how much charge the source stores | how long it can supply that current | label, confirmed by a capacity test |
| **Startup / stall current** | A (peak) | the current a motor draws when it is *not* turning — it is back-driven | whether the source survives a start from standstill | motor datasheet, or a current probe in series |

A cell can be excellent on capacity and still be useless on current capability,
and vice versa. This project ran into exactly that.

## 2. The "6800 mAh" problem

The project tested **generic WZS 18650 cells labelled 6800 mAh** and they did
**not** give satisfactory robot performance. The recorded symptoms were:

* the robot slowed or stopped when all four motors started from a standstill;
* the rear pair started less often than the front pair;
* the pack behaved inconsistently between charges, and between two packs that
  were supposed to be identical;
* the Uno occasionally restarted under load — the 3 s startup blink appeared
  again mid-run.

**What this does and does not mean.**

* It does **not** mean that 18650 cells are unsuitable, and it does **not** say
  anything about any other brand, model or batch. A genuine high-current cell
  with a low internal resistance is a good choice for this robot.
* It does mean that a **printed capacity number is not a measurement**. Cells
  sold as 6800 mAh may hold a fraction of that, and their internal resistance
  — which is what sets the current behaviour — may be high.
* Capacity and current capability are independent: a cell can store a lot of
  charge and still fold up under a 20 A load, and a cell with modest capacity
  can still deliver a high current if its internal resistance is low.

**Diagnosis used on this project:** the observed behaviour is a classic
high-internal-resistance signature — voltage collapses under current, the
symptom gets worse as the pack empties, and two "identical" packs behave
differently. The way to tell it apart from a wiring fault is the test in §3:
command all four motors from a standstill, on blocks *and* on the floor, and
watch the rail voltage. Wiring faults do not depend on mechanical load.

## 3. Why a motor robot needs a strong source

A DC motor's speed is proportional to the applied voltage minus its own
resistive drop. When the motor is turning, back EMF reduces the current. When
the motor is **not** turning — at the instant a command is given, or when it is
loaded heavily — there is no back EMF and the motor draws its full startup
current from the source.

Multiply that by four motors starting at the same instant:

```
motor current (free running)   <<  motor startup current (stalled)  <<  the peak
                                                                     the source
                                                                     must supply
```

Consequences observed on this build:

* **Voltage sag.** The rail dips at the moment of starting; if it dips below
  what the motors need, they do not start at all.
* **Brown-out of the logic side.** The Uno and the 74HC595 need a stable 5 V.
  If the pack collapses far enough, the ATmega328P resets and the whole
  firmware restarts — which is why the fault *looks* like a sensor problem.
* **Asymmetric behaviour.** A motor near the end of its travel has to work
  harder, draws more current, and makes the rail sag more, so the motor next to
  it is the one that fails. This is exactly the "rear motors sometimes failing
  to move" symptom in
  [10 Troubleshooting §2](10_Troubleshooting.md#2-motors-do-not-move-together).

The design response in this project is to keep motor current **out of the logic
path entirely**: the pack feeds the shield's `VIN` directly, the Uno's regulator
only supplies the small logic load, and motor and sensor wiring are routed
apart. See [04 Electrical Architecture](04_Electrical_Architecture.md).

## 4. The final power arrangement

| Element | Value | Status |
|---|---|---|
| Cells | 2 × 3.7 V 18650 Li-ion | given by the brief |
| Arrangement | 2 in series | given by the brief |
| Nominal motor rail | 7.4 V (≈ 8.4 V full, ≈ 6.0 V empty) | **to be verified** with a meter |
| Connection | battery `+` → shield `VIN`, battery `−` → common ground | [05 Wiring §1](05_Wiring.md#1-power-first) |
| Logic supply | Uno on-board 5 V regulator, taken from the shield stack | [04 §2](04_Electrical_Architecture.md#2-the-5-v-rail) |
| Cell capacity | **to be verified** | do not trust the label |
| Max continuous current | **to be verified** | datasheet or load test |
| Motor startup current | **to be verified** | datasheet or measurement |

![Wiring diagram](../diagrams/wiring_diagram.png)

## 5. What the L293D costs

The L293D in the shield is a **bipolar** driver with a saturation drop of
roughly 0.5–1.4 V depending on current. The motors therefore never see the full
pack voltage. That is worth knowing when a motor "seems slow" — the pack is at
6.0 V and the motor sees less. **The exact drop on this build is to be
verified** by measuring the voltage across a motor while it runs.

## 6. Earlier experiments (project history)

AA batteries were tried first, before the 18650 cells. That attempt and its
outcome are written up as history in
[history/power_testing.md](../history/power_testing.md) — it is not part of the
final design.

## 7. Safety practices

* Use a purpose-made 2-cell 18650 holder. **Do not solder directly onto bare
  cells** and do not spot-weld them; the heat is a hazard and a shorted cell can
  vent.
* Never charge loose cells on the bench, and never charge them in the robot.
  Remove the pack before charging.
* Do not reverse the pack. Check polarity with a meter *before* connecting.
* Protect the pack from mechanical damage — a 18650 cell that is dented or
  punctured is a fire risk; do not build a robot that can roll over onto one.
* Keep the pack away from heat and out of direct sun.
* If a cell becomes hot, swollen or smells unusual, stop using it.
* Add an inline fuse once the current is known (see
  [13 Future Improvements](13_Future_Improvements.md#6-power-and-measurement-improvements)).

## 8. How to characterise a pack before trusting it

1. Measure the open-circuit voltage with a meter. A 2S pack of healthy cells is
   well above 6.0 V and below 8.4 V.
2. Load it with a known resistor or a motor and watch the voltage under load. A
   large collapse means high internal resistance.
3. Compare two packs from the same batch. Large differences mean the cells are
   not what the label says.
4. Only after 1–3, run the robot.

## 9. Summary

* Judge a source on **current capability** and **internal resistance** first.
* **Capacity** decides runtime; it says nothing about current.
* **Startup current** is the worst case, and four motors start together.
* The 6800 mAh cells tested on this project were not good enough; that is a
  statement about those cells, not about 18650 cells in general.
* Keep motor current out of the logic supply, and keep the wiring apart.
