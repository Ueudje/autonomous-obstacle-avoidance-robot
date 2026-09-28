# History — Power Testing

> **This is project history, not the final design.**
> The final power arrangement is in
> [11 Power System](../docs/11_Power_System.md) and
> [04 Electrical Architecture §1](../docs/04_Electrical_Architecture.md#1-the-two-domains).
> The conclusions here are about the cells that were actually tested, not about
> 18650 cells in general.

---

## 1. Where it started: AA batteries

The first power source was a holder of AA cells. It powered the Arduino and the
electronics perfectly well. It could not power the robot.

| Observation | Meaning |
|---|---|
| The Uno and the LEDs ran for a long time | the *logic* load is tiny |
| The motors were slow and weak | the voltage per cell and the current capability were both marginal |
| The robot recovered when the pack was nearly flat, then got worse | a pack near its floor voltage has almost no margin left |
| Brown-outs were frequent | the cells could not hold up the rail when all four motors drew current |

The AA pack was abandoned. The problem was not the *capacity* of the pack — it
was the **voltage and the current capability**.

## 2. Moving to 18650 cells

The project moved to 18650 Li-ion cells, which is where it still is: **two
3.7 V cells in series**, a 7.4 V nominal motor rail, which matches what the
L293D-based shield expects for its `VIN`.

That change fixed the voltage problem immediately: the motors had real voltage
headroom and the robot drove properly instead of crawling.

## 3. The "6800 mAh" cells that did not work

Generic **WZS 18650 cells labelled 6800 mAh** were then tested. They did **not**
give satisfactory performance.

### Symptoms

* the robot slowed noticeably when all four motors started from a standstill;
* the rear pair failed to start more often than the front pair;
* two packs of the same type behaved differently from each other;
* the pack got noticeably worse as it discharged;
* on the worst occasion the Uno restarted mid-run — visible as the 3 s startup
  LED blink happening again in the middle of a run.

### Diagnosis

The pattern — *voltage collapse under current, worsening as the pack empties,
differing between two supposedly identical packs* — is the signature of **high
internal resistance**, not of low capacity.

The decisive test was to compare the same motor command with the robot **on
blocks** (no mechanical load) and **on the floor** (full load):

| Test | Result |
|---|---|
| Four motors, on blocks | all start, every time |
| Four motors, on the floor | rear pair often does not start |
| Rail voltage measured during the start | dips sharply |

A wiring fault does not care about mechanical load. This one did, so the fault
was in the source.

### What was concluded — and what was not

| Conclusion | Correct? |
|---|---|
| The printed "6800 mAh" was not a usable capacity figure for these cells | yes — it was never verified, and the behaviour is inconsistent with a healthy 6800 mAh cell |
| The cells had high internal resistance and could not supply four motors' starting current | yes, and that is the whole explanation |
| 18650 cells are unsuitable for this robot | **no** — a genuine high-current cell is a good choice; that is exactly what the final build expects |
| Any cell with a high mAh number will work | **no** — capacity and current capability are independent properties |

The general lesson is written up in
[11 Power System §2](../docs/11_Power_System.md#2-the-6800-mah-problem).

## 4. The brown-out that looked like a firmware bug

One failure is worth recording in full, because it wasted the most time.

**Symptom.** The robot, driving normally, would suddenly stop; the four LEDs
would run the startup blink for 3 s; then it would drive again, as if it had
just been powered up.

**First hypothesis.** A software fault — a sensor reading that put the firmware
into `STATE_STARTUP`, or a bad state transition.

**Why that hypothesis was wrong.** `STATE_STARTUP` is only ever entered from
`setup()`. There is no path from any other state back into it. The firmware
cannot restart itself.

**Test.** Watch what the LEDs did immediately *before* the blink. They were all
dark for a moment — including the front pair that should have been lit while
driving. A state transition would have changed the LED pattern; a reset blanks
everything first.

**Observation.** The rail voltage collapsed far enough for the ATmega328P's
brown-out detector to reset the chip.

**Lesson.** "The startup blink happened again" is a **power** symptom, not a
firmware symptom. Recognising the pattern stopped a long and completely fruitless
search through the sensor code.

## 5. What was measured, and what was not

| Item | Status |
|---|---|
| Rail voltage, unloaded, at both ends of the discharge curve | measured with a multimeter |
| Rail voltage during a four-motor start | observed as an obvious dip, **not quantified** |
| Motor startup current | **not measured** — no current probe available |
| Average running current | **not measured** |
| Cell capacity | **not measured** — the label was not trusted |
| Cell internal resistance | inferred from behaviour, not measured directly |
| Runtime | **not measured** |

The gaps are listed in
[09 Testing §10](../docs/09_Testing.md#10-what-was-not-measured) rather than
filled with estimates.

## 6. Safety decisions taken during these experiments

* The pack was only ever connected through a proper 2-cell holder.
* No cell was ever soldered to directly.
* No loose cell was charged on the bench, and the pack was always removed from
  the robot before charging.
* Polarity was checked with a meter before every first connection.
* The cells used in the experiment were kept away from heat and from direct
  sun, and any cell that became hot, dented or swollen was retired.

These are restated in [11 Power System §7](../docs/11_Power_System.md#7-safety-practices).

## 7. The final arrangement

```
2 x 18650 (3.7 V, 2S, 7.4 V nominal)
        |
        + ------> shield VIN          motor domain
        |
        - ------> common GND rail ---- Uno GND, shield GND,
                                        4 x FC-51 GND, 4 x LED cathodes

Uno 5 V regulator ------> 4 x FC-51 VCC, 4 x LED branch resistors
```

Motor current never passes through the Uno's regulator, and the motor wiring is
routed away from the sensor wiring. That separation, more than the choice of
cells, is what makes the logic side behave.

## 8. What the next build should measure first

1. Motor startup current for one motor, then for all four together.
2. The rail voltage under that load, with a scope if possible.
3. The cell's internal resistance, and the difference between two cells of the
   same type.
4. Then, and only then, pick a fuse size and a battery.

Until those numbers exist, this project makes no claim about runtime, current
draw or cell suitability beyond what was observed.

Next: [final_changes.md](final_changes.md).
