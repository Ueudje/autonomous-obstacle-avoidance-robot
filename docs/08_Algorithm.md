# 08 — Algorithm

What the firmware in [`src/robot/Robot.ino`](../src/robot/Robot.ino) actually
does, in the order it does it. Every number here is in the source.

![Control flow](../diagrams/algorithm_flowchart.png)

---

## 1. Concise pseudocode

```text
setup:
    sensor pins  -> INPUT          (A0, A1, A2, A3)
    LED pins     -> OUTPUT         (D6, D0, D12, D3)
    all motors setSpeed(150)
    randomSeed(analogRead(A5))
    all four LEDs ON,  wait 1000 ms
    all four LEDs OFF, wait 1000 ms

loop, forever:
    read  A0, A1, A2, A3            (analogRead, 0..1023)

    if A0 < 200 or A1 < 200:                        # obstacle in front
        stopMotors(allLedsOn = true)                 # LEDs all on
        wait 1000 ms
        moveBackward()                               # 2000 ms, rear pair blinks
        stopMotors(allLedsOn = true)
        wait 1000 ms
        if random(2) == 0:  turnLeft()               # 1000 ms
        else:                turnRight()             # 1000 ms
    else if A2 < 200 or A3 < 200:                    # obstacle behind
        stopMotors(allLedsOn = true)
        wait 1000 ms
        moveForward()                                # 500 ms
        stopMotors(allLedsOn = true)
        wait 300 ms
    else:
        moveForward()                                # no timing
```

## 2. `setup()` — once, 2 s

| Step | Code | Notes |
|---|---|---|
| 1 | `pinMode(IR_*, INPUT)` | `A0`, `A1`, `A2`, `A3` |
| 2 | `pinMode(LED_*, OUTPUT)` | `D6`, `D0`, `D12`, `D3` — see [12 §7](12_Final_Implementation.md#7-pin-conflict--verification) |
| 3 | `motor1..4.setSpeed(150)` | set once; never changed again |
| 4 | `randomSeed(analogRead(5))` | `A5` is otherwise unused |
| 5 | all LEDs `HIGH`, `delay(1000)` | one second of "all on" |
| 6 | all LEDs `LOW`, `delay(1000)` | one second of "all off" |

The two seconds are a power-on signal, not a safety delay of any consequence:
the robot starts moving the moment the second `delay()` returns, and there is no
opportunity to interrupt it. Anyone putting the robot down by hand needs those
two seconds and no more.

## 3. The sensor read

```cpp
int frontLeft  = analogRead(IR_FRONT_LEFT);    // A0
int frontRight = analogRead(IR_FRONT_RIGHT);   // A1
int backLeft   = analogRead(IR_BACK_LEFT);     // A2
int backRight  = analogRead(IR_BACK_RIGHT);    // A3
```

Four reads, no delay between them, and the values are used only in the two
comparisons below.

**Threshold.** `200`, written inline at both test sites. An FC-51's `OUT` pin
is a comparator output, so a LOW reads as roughly `0` and a HIGH as roughly
`1023`; the threshold only has to sit between those two levels, and the real
adjustment is the module's own trimpot. A module that pulls `OUT` **high** while
blocked would read ≈ `1023` and never trigger — there is no polarity switch in
this firmware, and no hysteresis, so a marginal reading can flip the robot into
a full 5 s sequence on a single sample.

## 4. Case 1 — obstacle in front

Entered when `frontLeft < 200 || frontRight < 200`. Total time: **5000 ms**, and
no sensor is read for any of it.

| # | Call | Duration | Motors | Indicators |
|---|---|---|---|---|
| 1 | `stopMotors(true)` then `delay(1000)` | 1000 ms | released | all four on |
| 2 | `moveBackward()` | 2000 ms | all four `BACKWARD` | rear pair blinking |
| 3 | `stopMotors(true)` then `delay(1000)` | 1000 ms | released | all four on |
| 4 | `turnLeft()` or `turnRight()` | 1000 ms | see below | the *opposite* pair blinking |
| 5 | `stopMotors(true)` at the end of the turn | — | released | all four on |

`random(2) == 0` picks the direction, once, in step 4. The two turn functions
each end by releasing the motors and lighting all four indicators, so the robot
comes out of a turn stationary with everything on until the next `loop()` pass
reaches `moveForward()`.

**Step 2 detail.** `moveBackward()` is a timed loop:

```cpp
while (millis() - moveStart < 2000) {
    if (millis() - lastBlink >= blinkInterval) { lastBlink = millis(); blinkState = !blinkState; }
    motor1..4.run(BACKWARD);
    front pair LOW;  rear pair = blinkState;
    delay(10);
}
```

`blinkInterval` is 200 ms, so a full on-off cycle is 400 ms, and the loop is
paced by `delay(10)` — the duration is accurate to about 10 ms. `blinkState`
starts `false`, so the first 200 ms of the reverse are dark.

## 5. Which motors move, and which LEDs blink

| Turn | `motor1` FL | `motor2` BL | `motor3` BR | `motor4` FR | Blinking LEDs |
|---|---|---|---|---|---|
| `turnLeft()` | `BACKWARD` | `BACKWARD` | `FORWARD` | `FORWARD` | **FR** and **BR** |
| `turnRight()` | `FORWARD` | `FORWARD` | `BACKWARD` | `BACKWARD` | **FL** and **BL** |

In both rows the body rotates the way the function name says: in `turnLeft()` the
left wheels roll backwards while the right wheels roll forwards, which turns the
robot to the left.

**But the indicators are on the other side.** `turnLeft()` blinks the two
right-hand indicators. A watcher cannot tell from the blinking which way the
robot is about to go — the opposite pair is lit. The same inversion applies in
`turnRight()`. This is recorded in
[12 §8](12_Final_Implementation.md#8-final-robot-behaviour) and the one-line fix
is in
[13 §4](13_Future_Improvements.md#4-the-turn-indicators-are-on-the-wrong-side).

## 6. Case 2 — obstacle behind

Entered when the front test is false and `backLeft < 200 || backRight < 200`.
Total time: **1800 ms**.

| # | Call | Duration | Motors | Indicators |
|---|---|---|---|---|
| 1 | `stopMotors(true)` then `delay(1000)` | 1000 ms | released | all four on |
| 2 | `moveForward()` then `delay(500)` | 500 ms | all four `FORWARD` | front pair on, rear pair off |
| 3 | `stopMotors(true)` then `delay(300)` | 300 ms | released | all four on |

This is the case most often misread. The rear reaction is **not** the front
reaction with the words reversed: it stops, drives *forward* for half a second,
stops, and resumes. The rear obstacle is not escaped from — it is checked.

## 7. Case 3 — clear path

`moveForward()`: all four motors `FORWARD`, front pair on, rear pair off, and
no timing at all. The loop then reads the four sensors again immediately, so in
this case the scan rate is bounded only by the time `analogRead()` and the three
`run()` calls take — milliseconds.

## 8. Priority between the two obstacles

The front test is the outer `if`, so a robot with an obstacle in front **and**
behind runs the front sequence and never enters the rear one. There is no
handling for "blocked on both sides at the start of a turn".

## 9. What the state machine would have done

For comparison, and because it is the version this documentation used to
describe, a non-blocking version of the same behaviour keeps the sensor scan
running at a fixed 50 ms through every manoeuvre, and the same sequences become
eight named states with one timed exit each. That version was written and then
superseded; the differences that matter to a reader are:

| | This firmware | A `millis()` state machine |
|---|---|---|
| Longest blind period | 5000 ms | one loop pass |
| Reaction to a new obstacle mid-sequence | none, until it ends | depends on the state |
| Timings editable in one place | no — five separate literals | yes, one constant each |
| Behaviour named in the source | no — three `if` branches | yes, eight states |

See [13 §1](13_Future_Improvements.md#11-make-the-loop-non-blocking) for what the
change would involve, and
[history/final_changes.md](../history/final_changes.md) for how it ended up
where it is.
