# 10 — Troubleshooting

The real faults from this project, in the order they were found. Each entry
follows the same chain: **Problem → Possible Cause → Test → Observation → Fix →
Lesson Learned**.

The historical narrative is in [`history/`](../history/); this page is the
reference version of the same events.

---

## 1. Front motors move in the opposite direction

**Problem.** After the first wiring attempt, the robot was placed on the floor
and, with a single `run(FORWARD)` command on all four channels, the **front
motors turned one way and the rear motors the other**. The robot twisted on the
spot instead of driving forward.

**Possible cause.**

* The motors on one side are mounted as a mirror image of the other side, so
  the same terminal order produces opposite rotation.
* Red was treated as "always the positive terminal" and black as "always the
  negative terminal" — true for a sensor, **false** for a motor.
* A single pair of motor wires was connected to the shield terminals the wrong
  way round.

**Test.**

1. Robot on blocks, so no wheel touches the floor.
2. One channel at a time: `motorFL.run(FORWARD)`, `motorBL.run(FORWARD)`,
   `motorBR.run(FORWARD)`, `motorFR.run(FORWARD)`, 1 s each.
3. Watch the *rotation direction* of each wheel, not the wiring colours.
4. Repeat with `BACKWARD` and confirm each motor reverses.

**Observation.** M1 (front left) and M2 (back left) rotated one way; M3 (back
right) and M4 (front right) rotated the other. The left and right terminal
orders were identical on the shield; the motors themselves are mounted
mirrored, so identical orders must produce opposite rotation.

**Fix.** Mirror the terminal order between the left and the right motors at the
shield:

| Channel | Terminal 1 | Terminal 2 |
|---|---|---|
| M1, M2 (left) | red | black |
| M3, M4 (right) | black | red |

**No firmware change was made.** There is deliberately no per-motor inversion in
[`src/robot/Robot.ino`](../src/robot/Robot.ino).

**Lesson learned.** A DC motor has two terminals and no permanent polarity; the
H-bridge decides direction. Left/right mirroring is a *wiring* problem, and
fixing it in code would hide a mechanical fact that the next person to build
this robot has to rediscover. Red/black are an organisational convention only.

## 2. Motors do not move together

**Problem.** Individually, each motor spun. Together, on the floor, the robot
jerked, stopped, and sometimes only two wheels turned.

**Possible cause.**

* Motor current peaks from four motors starting at the same instant exceed what
  the power source can deliver, so the rail sags below the motor's working
  voltage at the moment of starting.
* A partially depleted battery pack.
* A motor with extra mechanical load (see §3).

**Test.**

1. All four channels commanded forward **on blocks**: the current demand is the
   same, but the mechanical load is not — all four spin freely.
2. Same command **on the floor**: the load appears and the failure returns.
3. Measure the rail voltage at the shield `VIN` while commanding the start.
4. Swap to a different, known-good pack and repeat step 2.

**Observation.** On blocks all four started together. On the floor, the rear
pair frequently failed to start, and the rail voltage dipped during the start
attempt. With a pack in better condition the same command worked every time.

**Fix.** Power the motors from a source that can deliver their starting current —
see [11 Power System §3](11_Power_System.md#3-why-a-motor-robot-needs-a-strong-source)
— and keep the mechanical load low so the motors are not fighting the floor.

**Lesson learned.** "It works on blocks" and "it works on the floor" are
different tests. A supply that is adequate for a spinning motor is not
necessarily adequate for a *starting* motor under load.

## 3. A single wheel turns more slowly than the others

**Problem.** The robot drove, but pulled to one side in a straight line.

**Possible cause.**

* One motor's gearbox is tighter, or one wheel is tighter on its shaft.
* A motor lifting a slightly greater share of the load runs slower.
* Unrelated to electronics entirely.

**Test.** Lift the chassis so all four wheels are free and spin each wheel by
hand; compare the coast-down. Repeat the straight-line test on a smooth floor.

**Observation.** One wheel coasted noticeably further than the others — it had
more inertia, i.e. more rolling resistance.

**Fix.** Corrected the wheel fit; no software or wiring change.

**Lesson learned.** Mechanical asymmetry shows up as an electrical symptom.
Before touching the firmware, check that all four wheels are the same and turn
freely.

## 4. Wheels coming loose on the shaft

**Problem.** Twice during testing the robot started driving in a circle that had
nothing to do with the code.

**Possible cause.** The wheel was only pressed onto the motor shaft; friction was
the only thing holding it.

**Test.** Spin each wheel by hand and check for movement along the shaft.

**Observation.** Play on two of the four wheel fits.

**Fix.** Mechanical retention: a grub screw or a locking collar wherever the
motor allows one, and a proper press fit on the rest.

**Lesson learned.** This fault is genuinely dangerous for the diagnosis: a robot
driving in circles looks exactly like a wrong-direction fault, and it sent the
investigation down the wrong path for a while.

## 5. The IR sensors report obstacles that are not there

**Problem.** The robot reacted as if an obstacle were present on an empty floor.

**Possible cause.**

* The FC-51 trimpot is set too sensitive, or too close to the module's limit.
* Motor current transients are coupled into the sensor `OUT` wires.
* No common ground between the sensors and the Uno.

**Test.**

1. Motors off, robot stationary: does the false trigger still happen?
2. Motors on, robot on blocks (so there is no mechanical vibration): does it
   still happen?
3. Measure the `OUT` level at rest with a multimeter.
4. Re-tune the trimpot and repeat.

**Observation.** With the motors off the false triggers disappeared; with the
motors running they returned. The `OUT` lines were routed alongside the motor
cables.

**Fix.**

* Re-tuned the trimpots on the actual floor material.
* Moved the sensor cables away from the motor cable route.
* Verified the common ground between the shield, the Uno and all four modules.

**Lesson learned.** The two-level chassis helps here: the sensors are 1.8 cm
above the floor on the *lower* deck, away from the wheels and the motor leads.

## 6. A sensor that never reports an obstacle

**Problem.** One of the four sensors seemed to do nothing.

**Possible cause.**

* The module's trimpot is at the end of its range.
* The `OUT` wire is disconnected or on the wrong pin.
* A front and rear module were swapped in the wiring.
* The wrong build of the module is fitted (some 4-pin versions have a different
  `OUT` behaviour).

**Test.** Bring an obstacle to each module **in turn** and watch the LED pair
that belongs to that corner; then confirm the pin assignment with a multimeter
on the `OUT` line.

**Observation.** The rear-right module's `OUT` line was connected to the
front-right header position.

**Fix.** Corrected the wiring, and the pin-to-corner table in
[05 Wiring §2](05_Wiring.md#2-sensors-the-four-ir-inputs) was written down so the
same swap cannot be repeated silently.

**Lesson learned.** Four identical modules wired to four identical-looking pins
will not tell you which one is wrong by looking. Write the mapping down.

## 7. The robot "goes deaf" while reversing

**Problem.** With the first version of the firmware, the robot reversed for the
full programmed time without reacting to anything, even if a box was placed in
front of it during the reverse.

**Possible cause.** The sequence was written with `delay()`:

```cpp
// first version - see history/initial_concept.md
stopMotors(true);
delay(500);
driveAllBackward();
delay(700);
turnInPlace(true);
delay(600);
```

`delay()` stops the entire program. During those 1.8 s the sketch read no
sensors at all, and the LED blink could not run either.

**Test.** Put an obstacle in front of the robot during the reverse phase and
watch whether anything happens before the turn ends. Then repeat with the
`millis()` version.

**Observation.** The `delay()` version ignored the obstacle completely. The
state-machine version is not *blind* in the same way — its loop keeps running,
so the LED blink continues and the manoeuvre stays interruptible — but the
sensors are still not re-read until the robot is back in `STATE_FORWARD`, so an
obstacle that appears during the manoeuvre is likewise not acted on. This
limitation is stated in
[08 §7](08_Algorithm.md#7-why-the-sensor-read-is-inside-the-forward-case) and
[13 §3](13_Future_Improvements.md#3-re-evaluate-obstacles-mid-sequence), and is
test T7 in [09 §9](09_Testing.md#9-stage-g--the-complete-robot).

**Fix.** Replace the blocking sequence with the eight-state machine in
[`src/robot/Robot.ino`](../src/robot/Robot.ino). Every duration became a
deadline compared against `millis()`; nothing waits.

**Lesson learned.** On a robot, time spent waiting is time not spent sensing.
`millis() - previousMillis >= interval` lets several tasks share one loop.

## 8. The sensor logic is inverted

**Problem.** The robot drove straight into obstacles, or refused to move at all.

**Possible cause.** `OUT` is LOW on an obstacle — the opposite of the naive
assumption. A firmware that reads `digitalRead(pin) == HIGH` as "obstacle" is
exactly backwards.

**Test.** Serial Monitor plus a `Serial.println()` of one sensor flag, with and
without an obstacle in front of the module
([09 Testing step 4](09_Testing.md#step-4--lowhigh-sensor-logic)).

**Observation.** The flag went to `1` when the obstacle **left** the field.

**Fix.** `#define SENSOR_ACTIVE_LOW 1`, with the comparison done in exactly one
place:

```cpp
bool isObstacle(uint8_t pin) {
  int level = digitalRead(pin);
#if SENSOR_ACTIVE_LOW
  return level == LOW;
#else
  return level == HIGH;
#endif
}
```

**Lesson learned.** One place decides the polarity. Four copies of the
comparison is four chances to get it wrong, and a build with a different module
batch should be fixable with a one-character change.

## 9. The robot stops unexpectedly / brown-outs

**Problem.** Under load the robot slowed, the Uno restarted, and the LED
indicators went dark.

**Possible cause.** Supply voltage collapsing below the Uno's brown-out
threshold, because the pack could not supply the combined load.

**Test.** Monitor the rail voltage while commanding the worst case: all four
motors starting from standstill on the floor.

**Observation.** The rail collapsed, and the Uno reset — the 3 s startup
sequence restarted, which was the clue that it was a reset and not a sensor
fault.

**Fix.** Use a source with adequate current capability and a state of charge
that supports the load; see [11 Power System](11_Power_System.md) and
[history/power_testing.md](../history/power_testing.md).

**Lesson learned.** A restart that always looks like "the startup blink again"
is a power symptom. Recognising the brown-out pattern saved a long and entirely
fruitless search through the sensor code.

## 10. Misleading conclusions to avoid

| Tempting conclusion | Why it is wrong here |
|---|---|
| "The firmware has a logic bug" | Every fault in this list looked like a firmware bug until it was tested on blocks. |
| "Red is the positive terminal" | Only true for a module with a marked pinout. A motor has no polarity of its own. |
| "A 18650 is a 18650" | Capacity, internal resistance and cell quality differ between cells with the same printed capacity. See [11 Power System](11_Power_System.md#2-the-6800-mah-problem). |
| "The shield's spare pins are obvious" | They are not; eight Arduino pins are consumed by the shield. See [12 §7](12_Final_Implementation.md#7-pin-conflict--verification). |
| "Two sensor heights means two sensor decks" | The final build puts sensors low and LEDs high. The two-sensor-level idea is history: [history/analog_sensor_experiment.md](../history/analog_sensor_experiment.md). |

## 11. Quick reference

| Symptom | First check | Section |
|---|---|---|
| Robot spins on the spot | left/right motor terminal mirror | §1 |
| Robot veers | wheel fit, rolling resistance | §3 |
| Two motors do not start on the floor | supply current capability | §2, §9 |
| Phantom obstacles | trimpot, cable routing, common ground | §5 |
| One sensor does nothing | pin-to-corner mapping | §6 |
| Robot ignores a new obstacle during a manoeuvre | `delay()` still in the sketch? | §7 |
| Robot drives into obstacles | `SENSOR_ACTIVE_LOW` | §8 |
| Uno restarts, startup blink repeats | supply voltage under load | §9 |
