# History — Final Changes

> **This is project history: the list of changes that turned the working robot
> into the documented one.** The result of applying them is the final
> implementation in
> [`src/robot/Robot.ino`](../src/robot/Robot.ino) and
> [12 — Final Implementation](../docs/12_Final_Implementation.md).

---

## 1. Change list

| # | Change | From | To | Why |
|---|---|---|---|---|
| 1 | Timing model | `delay()` | `millis()` state machine | the robot was blind for 1.8 s during every manoeuvre |
| 2 | State count | 1 implicit path | 8 explicit states | every behaviour became nameable, testable and documented |
| 3 | Sensor reading | `digitalRead()` with `HIGH` meaning obstacle | `digitalRead()` with `SENSOR_ACTIVE_LOW` | a standard FC-51 pulls `OUT` LOW on an obstacle |
| 4 | Polarity decision | duplicated per sensor | one function, `isObstacle()` | a different module is now a one-character change |
| 5 | Rear sensing | none | `REAR_BRAKE` / `REAR_CREEP` / `REAR_SETTLE` | the robot used to reverse without looking behind |
| 6 | Stop indication | none | 4 LEDs, 2 red front / 2 white rear | the robot's state became visible from outside |
| 7 | Startup | immediate motion | 3 s delay, all four LEDs blinking | a safety window to put the robot down |
| 8 | Turn indication | none | the two LEDs of the turning side blink | a turn is now visible, and the indicator names its direction |
| 9 | LED logic | four separate `digitalWrite` calls per state | bit masks (`LED_FRONT_PAIR`, `LED_LEFT_SIDE`, …) | the LED behaviour reads as intention, not as pin juggling |
| 10 | Motor direction | in the wiring only | *unchanged* — deliberately | a motor direction is a wiring fact, not a code branch |
| 11 | Blink | blocking `delay(250)` toggle | non-blocking toggle in `updateBlink()` | blinking must not stop the sensor scan |
| 12 | Dead code | an unused `stopBlink()` helper | removed | nothing was defined and never called |
| 13 | Documentation | scattered notes | 13 documents + this history folder | the build has to be reproducible by someone else |

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
[08 §7](../docs/08_Algorithm.md#7-why-the-sensor-read-is-inside-the-forward-case)
and the change itself is listed in
[13 §3](../docs/13_Future_Improvements.md#3-re-evaluate-obstacles-mid-sequence).

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
| debounce the sensors | it changes the documented timing; it belongs in [13 §1](../docs/13_Future_Improvements.md#1-reading-the-sensor-more-than-once-per-poll) |
| re-evaluate obstacles mid-sequence | it changes the observable sequence; [13 §3](../docs/13_Future_Improvements.md#3-re-evaluate-obstacles-mid-sequence) |
| invert a motor in the code | a direction problem is a wiring problem; the mirror is the fix |
| raise the speed above 150/255 | the power source is the limiting factor, not the duty cycle |
| remove the startup delay | it is a safety feature |
| move an LED to A0/A1 with `analogWrite()` | A0/A1 cannot do `digitalRead`; it is a behaviour change, so it is documented as an option, not applied |
| add the second sensor height | no digital-capable pin is left; [13 §2](../docs/13_Future_Improvements.md#2-two-sensor-heights-as-originally-intended) |

## 7. Verification performed on the final firmware

| Check | Result |
|---|---|
| Syntax and structure | compiles as a single `.ino`; `setup()` / `loop()` / one `switch` |
| `delay()` / `delayMicroseconds()` | none present |
| `analogRead()` | not used — the sensors are digital |
| Blocking calls in any task | none; LED, sensor and motor tasks are all non-blocking |
| Unused variables | none — every declared identifier is used |
| Unused functions | none — an unused `stopBlink()` helper was removed |
| Every state reachable | all 8 states are entered from at least one other state |
| Every state has an exit | yes, each with exactly one timed exit condition |
| Motor mapping consistent with the wiring | M1 FL, M2 BL, M3 BR, M4 FR, with the left/right mirror in the wiring |
| Sensor mapping consistent with the wiring | A2 FL, A3 FR, A4 RL, A5 RR |
| LED mapping consistent with the wiring | D2 FL, D9 FR, D10 RL, D13 RR |
| Pin conflict with the shield | checked against the `AFMotor` source — no conflict; the D13 caveat is documented |
| `AFMotor` / Uno compatibility | library install steps in [12 §9](../docs/12_Final_Implementation.md#9-compile-and-upload--arduino-ide-1819) |

## 8. Where the project stands

The robot in [12 — Final Implementation](../docs/12_Final_Implementation.md) is
the final one. The gaps in it — runtime, current, detection range per surface,
mid-sequence re-evaluation — are listed in
[09 Testing §10](../docs/09_Testing.md#10-what-was-not-measured) and
[13 — Future Improvements](../docs/13_Future_Improvements.md), and are recorded
as gaps rather than filled with estimates.
