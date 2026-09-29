# History — Final Changes

> **This is project history, and it describes a version of the firmware that
> is not the one in this repository.**
>
> The change list below was written for a non-blocking `millis()` state machine.
> That sketch was **not** the version that ended up in
> [`src/robot/Robot.ino`](../src/robot/Robot.ino). The shipped sketch is the
> blocking-loop version documented in
> [12 — Final Implementation](../docs/12_Final_Implementation.md), and it is
> what every document in `docs/` describes.
>
> This page is kept because the reasoning is still worth reading — especially
> §3, §4 and §5 — and because it is the clearest record of what the project
> tried and then abandoned. **Where this page and document 12 disagree,
> document 12 is right.**

| | This page describes | The repository ships |
|---|---|---|
| Timing model | `millis()` state machine, nothing waits | `millis()` in three loops, `delay()` ten times |
| States | 8 named states | 3 branches in `loop()` |
| Sensor read | `digitalRead()` + `SENSOR_ACTIVE_LOW` | `analogRead()` compared against a literal `200` (four comparisons) |
| Indicators | on `D2`, `D9`, `D10`, `D13` | on `D6`, `D0`, `D12`, `D3` — **all four conflict with the shield** |
| Startup | 3 s, all four blinking | 2 s: all four on 1 s, all off 1 s |
| Turn indication | the pair of the side being turned towards | the **opposite** pair blinks |
| Rear obstacle | stop → creep → settle | stop 1 s → forward 0.5 s → stop 0.3 s |

---

## 1. Change list

**These are the changes that were planned and prototyped. Only the wiring
mirror, the rear-sensing behaviour and the two-level chassis survived into the
shipped sketch.**

| # | Change | From | To | Shipped? |
|---|---|---|---|---|
| 1 | Timing model | `delay()` | `millis()` state machine | the robot was blind during every manoeuvre | **no** — 11 `delay()` calls remain |
| 2 | State count | 1 implicit path | 8 explicit states | every behaviour becomes nameable and testable | **no** — 3 branches |
| 3 | Sensor reading | `digitalRead()` with `HIGH` meaning obstacle | `digitalRead()` with `SENSOR_ACTIVE_LOW` | a standard FC-51 pulls `OUT` LOW on an obstacle | **no** — `analogRead()` |
| 4 | Polarity decision | duplicated per sensor | one function, `isObstacle()` | a different module becomes a one-character change | **no** — no switch exists |
| 5 | Rear sensing | none | `REAR_BRAKE` / `REAR_CREEP` / `REAR_SETTLE` | the robot used to reverse without looking behind | **yes**, as a blocking sequence |
| 6 | Stop indication | none | 4 LEDs, 2 red front / 2 white rear | the robot's state becomes visible from outside | **yes**, on the wrong pins |
| 7 | Startup | immediate motion | 3 s delay, all four LEDs blinking | a safety window to put the robot down | **partly** — 2 s, on 1 s then off |
| 8 | Turn indication | none | the two LEDs of the turning side blink | the indicator names the direction | **no** — the opposite pair blinks |
| 9 | LED logic | four separate `digitalWrite` calls per state | bit masks | the LED behaviour reads as intention | **no** — direct `digitalWrite()` |
| 10 | Motor direction | in the wiring only | *unchanged* — deliberately | a motor direction is a wiring fact, not a code branch | **yes** |
| 11 | Blink | blocking `delay(250)` toggle | non-blocking toggle in `updateBlink()` | blinking must not stop the sensor scan | **yes** — inside the three loops |
| 12 | Dead code | an unused `stopBlink()` helper | removed | nothing defined and never called | **yes** |
| 13 | Documentation | scattered notes | 13 documents + this history folder | the build has to be reproducible by someone else | **yes** |
| 14 | Indicator pins | `D2`, `D9`, `D10`, `D13` | *unchanged* — the budget was already correct | the shield leaves exactly four LED-capable pins | **no** — the shipped sketch uses `D6`, `D0`, `D12`, `D3` |

## 2. The `millis()` rewrite in detail

This was the single largest change, so it is worth recording what it actually
was.

**Before** — a linear sequence with waits:

```cpp
void loop() {
  if (obstacleInFront()) {
    stopMotors(true);
    delay(500);
    driveAllBackward();
    delay(700);
    turnInPlace(random(0, 2) == 0);
    delay(600);
  } else {
    driveAllForward();
  }
}
```

**After** — the same sequence as named states with deadlines:

```cpp
void loop() { updateRobot(); }

void updateRobot() {
  updateBlink();
  switch (state) {
    case STATE_STOP_HOLD:
      if (stateElapsed(STOP_HOLD_MS)) beginReverse();
      break;
    case STATE_REVERSE:
      if (stateElapsed(REVERSE_MS))    beginTurn();
      break;
    case STATE_TURN:
      if (stateElapsed(TURN_MS))       beginNormalForward();
      break;
    ...
  }
}
```

| Property | `delay()` version | `millis()` version |
|---|---|---|
| Loop responsiveness during a manoeuvre | fully frozen | every pass still runs |
| LEDs blink during a manoeuvre | frozen mid-blink | never interrupted |
| Sensors read during a manoeuvre | never | never — the read stays in `STATE_FORWARD` |
| The random choice | recomputed on every pass | taken once, when `TURN` is entered |
| Duration accuracy | good | good |
| Behaviour visible in the source | as a straight line | as named states |
| What happens if a duration is changed | edit the `delay()` argument | edit one constant |

What the rewrite bought is **responsiveness of the loop**, not mid-manoeuvre
sensing: the blink task and every other non-blocking task keep running, so a
1.8 s manoeuvre is no longer 1.8 s of suspended animation. The sensor read,
however, deliberately stayed in `STATE_FORWARD` — the sequence is fixed once it
starts, and re-evaluating it would change the documented behaviour. That
decision is recorded in
[08 §7](../docs/08_Algorithm.md#4-case-1--obstacle-in-front)
and the change itself is listed in
[13 §3](../docs/13_Future_Improvements.md#14-re-evaluate-obstacles-mid-sequence).

## 3. The rear-obstacle decision

The rear behaviour was chosen deliberately to be **different** from the front
behaviour, and it is easy to mistake one for the other:

| | Front obstacle | Rear obstacle |
|---|---|---|
| Intent | create space — the robot is in the way of something | find out whether it can move again |
| Step 1 | stop, all four LEDs on | stop, front LEDs stay on |
| Step 2 | **reverse** | **drive forward** |
| Step 3 | turn in a random direction | stop again |
| Result | leaves the obstacle | returns to normal driving |

The "drive forward" in the middle of the rear sequence looks like a bug at
first reading, and it was questioned several times during the build. It is
intentional: a rear obstacle does not have to be escaped from, it has to be
checked. `stopMotors(false)` is used there precisely so that the stop
indicates "ready to go forward" with the front pair rather than overwriting it
with the all-LEDs-on stop pattern.

## 4. The turn indicator decision

The brief required that "the two LEDs belonging to the selected side blink
together", and asked which side and why. The decision:

* a **left** turn blinks the **front-left and rear-left** LEDs;
* a **right** turn blinks the **front-right and rear-right** LEDs.

The indicator names the direction the robot is rotating *towards*, not the
wheels that are driving it — the left wheels are actually being driven
backwards during a left turn. Making that explicit mattered, because the first
implementation flashed the pair that was running backwards, which is
technically consistent and completely unintuitive to watch.

## 5. The pin budget decision

The last hardware change was the LED pin selection, and it was forced:

| Set | Pins |
|---|---|
| Reserved by the AFMotor / L293D shield | D3, D4, D5, D6, D7, D8, D11, D12 |
| USB serial | D0, D1 |
| Analog-only | A0, A1 |
| **Available** | **D2, D9, D10, D13, A2, A3, A4, A5** |
| Taken by the four IR sensors | A2, A3, A4, A5 |
| **Left for the four LEDs** | **D2, D9, D10, D13** |

D13 was therefore unavoidable. Its side effect — the Uno's built-in LED mirrors
the rear-right indicator — is **flagged rather than hidden**, together with the
alternatives, in
[12 §7](../docs/12_Final_Implementation.md#7-pin-conflict--verification) and in
[13 §4](../docs/13_Future_Improvements.md). Nothing was changed silently.

The same check also flagged that D9 and D10 are the pins a servo library would
claim. This project has no servos, so it is safe **today**; it is recorded so
that adding a servo later does not quietly break an indicator.

## 6. What was deliberately *not* changed

| Tempting change | Why it was not made |
|---|---|
| debounce the sensors | it changes the documented timing; it belongs in [13 §1](../docs/13_Future_Improvements.md#13-re-read-the-sensors-more-than-once-per-poll) |
| re-evaluate obstacles mid-sequence | it changes the observable sequence; [13 §3](../docs/13_Future_Improvements.md#14-re-evaluate-obstacles-mid-sequence) |
| invert a motor in the code | a direction problem is a wiring problem; the mirror is the fix |
| raise the speed above 150/255 | the power source is the limiting factor, not the duty cycle |
| remove the startup delay | it is a safety feature |
| move an LED to A0/A1 with `analogWrite()` | A0/A1 cannot do `digitalRead`; it is a behaviour change, so it is documented as an option, not applied |
| add the second sensor height | no digital-capable pin is left; [13 §2](../docs/13_Future_Improvements.md#22-a-second-sensor-height-never-built) |

## 7. Verification performed on that firmware

**None of the rows below describe the sketch in this repository.** They record
what was checked on the state-machine version. The corresponding checks on the
shipped sketch are in
[07 §12](../docs/07_Software_Architecture.md#12-quality-checklist-applied-to-the-final-file),
and the two differ on the rows marked *fail* there: `delay()` calls present,
`analogRead()` present, and a pin conflict on all four indicators.

| Check | Result |
|---|---|
| Syntax and structure | compiles as a single `.ino`; `setup()` / `loop()` / one `switch` |
| `delay()` / `delayMicroseconds()` | none present — *not true of the shipped sketch, which has ten* |
| `analogRead()` | not used — the sensors are digital — *not true of the shipped sketch, which uses it* |
| Blocking calls in any task | none — *not true of the shipped sketch* |
| Unused variables | none — every declared identifier is used |
| Unused functions | none — an unused `stopBlink()` helper was removed |
| Every state reachable | all 8 states are entered from at least one other state |
| Every state has an exit | yes, each with exactly one timed exit condition |
| Motor mapping consistent with the wiring | M1 FL, M2 BL, M3 BR, M4 FR, with the left/right mirror in the wiring |
| Sensor mapping consistent with the wiring | A2 FL, A3 FR, A4 RL, A5 RR — *the shipped sketch uses A0–A3* |
| LED mapping consistent with the wiring | D2 FL, D9 FR, D10 RL, D13 RR — *the shipped sketch uses D6, D0, D12, D3* |
| Pin conflict with the shield | checked against the `AFMotor` source — no conflict — *the shipped sketch has four* |
| `AFMotor` / Uno compatibility | library install steps in [12 §9](../docs/12_Final_Implementation.md#9-compile-and-upload--arduino-ide-1819) |

## 8. Where the project stands

The robot in [12 — Final Implementation](../docs/12_Final_Implementation.md) is
the one that exists. The gaps in it — the blocking loop, the four pin conflicts,
the turn indicators on the wrong side, runtime, current, detection range per
surface — are listed in
[09 Testing §10](../docs/09_Testing.md#10-what-was-not-measured) and
[13 — Future Improvements](../docs/13_Future_Improvements.md), and are recorded
as gaps rather than filled with estimates.

**The most useful thing this page does now** is record that the state machine in
§2 was written, worked, and was not shipped. Whether that was the right call is
not this documentation's judgement — but the reader should know that the
blocking behaviour in the shipped sketch is a *choice with a documented
alternative*, not an oversight.
