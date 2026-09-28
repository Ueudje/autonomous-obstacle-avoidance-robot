# 08 — Algorithm

The behaviour that is actually implemented in
[`src/robot/Robot.ino`](../src/robot/Robot.ino) — not an idealised robot, and
not a different robot from the one in the history pages.

![Algorithm flowchart](../diagrams/algorithm_flowchart.png)

---

## 1. Concise pseudocode

```text
setup:
    configure the four IR pins as inputs
    configure the four LED pins as outputs
    all LEDs off, motors released
    randomSeed(micros())
    enter STARTUP

loop:                                   -- runs thousands of times per second
    update the active LED blink group   -- non-blocking, 250 ms half-period

    switch (state):

      STARTUP:
          if 3 s have passed:
              read the four sensors
              go FORWARD

      FORWARD:
          if 50 ms have passed since the last scan:
              read the four sensors
              if front left OR front right sees an obstacle:  go STOP_HOLD
              else if rear left OR rear right sees one:      go REAR_BRAKE

      STOP_HOLD:                        -- all four LEDs on
          if 500 ms have passed:  go REVERSE

      REVERSE:                          -- rear LEDs blink
          if 700 ms have passed:  go TURN

      TURN:                             -- LEDs of the chosen side blink
          pick a direction at random:  turnLeft = (random(0, 2) == 0)
          if turnLeft:  left wheels backward, right wheels forward
          else:         left wheels forward,  right wheels backward
          if 600 ms have passed:  go FORWARD

      REAR_BRAKE:                       -- front LEDs on, motors released
          if 300 ms have passed:  go REAR_CREEP

      REAR_CREEP:                       -- driving forward, rear LEDs blink
          if 600 ms have passed:  go REAR_SETTLE

      REAR_SETTLE:                      -- front LEDs on, motors released
          if 300 ms have passed:  go FORWARD
```

The `pick a direction` line in `TURN` is the only decision that is randomised,
and it is taken **once**, when the state is entered — not on every pass through
the loop. The random choice therefore holds for the whole 600 ms turn.

## 2. The state table

| State | Motors | LEDs | Exit condition | Next |
|---|---|---|---|---|
| `STATE_STARTUP` | released | all four blink | `STARTUP_DELAY_MS` 3000 | `STATE_FORWARD` |
| `STATE_FORWARD` | all forward, speed 150 | front pair on | front obstacle | `STATE_STOP_HOLD` |
| | | | rear obstacle (front still clear) | `STATE_REAR_BRAKE` |
| | | | otherwise | stays |
| `STATE_STOP_HOLD` | released | **all four on** | `STOP_HOLD_MS` 500 | `STATE_REVERSE` |
| `STATE_REVERSE` | all backward, speed 150 | rear pair blink | `REVERSE_MS` 700 | `STATE_TURN` |
| `STATE_TURN` | turn in place, speed 150 | left **or** right side blinks | `TURN_MS` 600 | `STATE_FORWARD` |
| `STATE_REAR_BRAKE` | released | front pair on | `REAR_PAUSE_MS` 300 | `STATE_REAR_CREEP` |
| `STATE_REAR_CREEP` | all forward, speed 150 | rear pair blink | `REAR_CREEP_MS` 600 | `STATE_REAR_SETTLE` |
| `STATE_REAR_SETTLE` | released | front pair on | `REAR_PAUSE_MS` 300 | `STATE_FORWARD` |

Timing constants in the sketch:

| Constant | Value | Meaning |
|---|---|---|
| `STARTUP_DELAY_MS` | 3000 | safety delay at power-on |
| `OBSTACLE_POLL_MS` | 50 | how often the four sensors are read |
| `STOP_HOLD_MS` | 500 | visible "stopped" indication |
| `REVERSE_MS` | 700 | backing away from the obstacle |
| `TURN_MS` | 600 | in-place turn |
| `REAR_PAUSE_MS` | 300 | the two rear-sequence stops |
| `REAR_CREEP_MS` | 600 | the rear-sequence forward creep |
| `LED_BLINK_MS` | 250 | blink half-period (on and off) |

## 3. Front-obstacle sequence

![Testing - reaction timeline](../images/testing.png)

1. **Stop.** `beginStopHold()` calls `stopMotors(true)`, which issues `RELEASE`
   on all four channels, sets the speed to 0 and lights **all four LEDs**. The
   500 ms hold makes the stop visible from outside the robot.
2. **Reverse.** `beginReverse()` sets the speed back to 150 and runs all four
   motors `BACKWARD`. The rear pair blinks at 250 ms per half-period.
3. **Choose a direction.** `random(0, 2) == 0` decides left or right, once.
4. **Turn.** `turnInPlace()` reverses the left pair and runs the right pair
   forward for a left turn, and the opposite for a right turn. The two LEDs of
   the chosen side blink, so the turn is visible.
5. **Resume.** After 600 ms the robot returns to `STATE_FORWARD`.

The loop itself keeps running at full speed throughout — a `case` in a state
machine does not "pause" anything; only the *action* is different. What does
**not** happen is a re-evaluation of the sensors during the manoeuvre: as
[§7](#7-why-the-sensor-read-is-inside-the-forward-case) explains, `readSensors()`
is called only in `STATE_FORWARD` (and once, at the end of the 3 s startup
delay). The LED blink task does keep running at 250 ms throughout, so a stop, a
reverse and a turn are all still visible.

## 4. Which side blinks, and why

| Turn | Blinking LEDs | Why |
|---|---|---|
| left | front-left **and** rear-left | a left turn is produced by the two left wheels moving in opposite directions to the two right wheels, so the indicator has to be on the side the robot is rotating *towards*. The pair — one red at the front, one white at the rear — spans the whole left side of the robot. |
| right | front-right **and** rear-right | mirror of the above. |

The indicator therefore names the direction of the turn, not the wheels that
drive it. That matches the requirement that "the two LEDs belonging to the
selected side blink together".

## 5. Rear-obstacle sequence

The rear case is deliberately **not** a mirror of the front case. A rear
obstacle does not require the robot to escape — it requires it to find out
whether it can move forward again:

1. `REAR_BRAKE` — stop for 300 ms. The front pair stays on, because the robot
   is still facing forward and the indication is "ready to go forward".
2. `REAR_CREEP` — drive forward for 600 ms. If the obstacle was a person who
   has just walked past, the robot gets moving again; the rear pair blinks to
   show that something was detected behind.
3. `REAR_SETTLE` — stop for 300 ms, front pair on again.
4. Back to `STATE_FORWARD`.

If the rear sensor still reports an obstacle 50 ms after returning to
`STATE_FORWARD`, the whole sequence simply runs again. That is acceptable: it
costs one stop-and-creep cycle, and it is what the final design specifies.

`stopMotors(false)` is used in the rear sequence precisely so that the front
pair keeps its own indication instead of being overwritten by the all-LEDs-on
stop pattern.

## 6. Priority when both ends are blocked

```cpp
if (frontBlocked()) {
  beginStopHold();
} else if (rearBlocked()) {
  beginRearBrake();
}
```

The front test is evaluated first, so a robot boxed in at both ends performs the
front sequence. The rationale is simple: the front obstacle is the one that can
stop the robot from doing damage, and the front sequence is the one that
actively creates space.

## 7. Why the sensor read is inside the `FORWARD` case

Only `STATE_FORWARD` calls `readSensors()` in its own `case`. The other states
act on the readings that brought them there. This is a deliberate simplification:

* the robot is committed to a 500 ms stop, a 700 ms reverse and a 600 ms turn —
  a total of 1.8 s during which the obstacle picture is not re-evaluated;
* re-evaluating mid-sequence would require extra states ("abort the reverse if
  the front is suddenly clear"), which is a behaviour change, not an
  optimisation;
* therefore this is listed as future work rather than being quietly added —
  see [13 Future Improvements](13_Future_Improvements.md#3-re-evaluate-obstacles-mid-sequence).

## 8. No blocking delay anywhere

There is no `delay()` and no `delayMicroseconds()` in the final sketch. Every
duration in the table above is a deadline compared against `millis()`:

```cpp
bool stateElapsed(unsigned long durationMs) {
  return (millis() - stateStartedAt) >= durationMs;
}
```

The consequence is that a single `loop()` serves the sensor scan, the LED blink
and the motor commands simultaneously, which is the entire reason the
behaviour is legible as a state machine. See
[07 Software Architecture §4](07_Software_Architecture.md#4-timing-with-millis).

## 9. Complexity, honestly

The whole firmware is 8 states, 1 transition per state exit, 4 inputs and 4
outputs. It is a reaction machine, not a planner: it has no memory of the room,
no goal beyond "keep moving", and no way to tell a wall from a chair. That is
the documented design, and improving it is listed in
[13 Future Improvements](13_Future_Improvements.md).
