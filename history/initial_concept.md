# History — Initial Concept

> **This is project history, not the final design.**
> The final firmware is [`src/robot/Robot.ino`](../src/robot/Robot.ino) and the
> final hardware is described in
> [12 — Final Implementation](../docs/12_Final_Implementation.md). Where this
> page disagrees with that document, the document is right.

---

## 1. The first idea

The starting point was the simplest possible statement: *a robot that drives
around a room by itself and does not hit anything.*

The first list of behaviours, written before any component was chosen:

1. drive forward on its own;
2. use infrared light to know whether something is in the way;
3. be able to go forward **and** backward;
4. when blocked, choose a left or a right turn **at random**;
5. after turning, carry on driving.

That list is, essentially, the final behaviour — stop, reverse, turn, resume —
with one difference: the very first version had no LEDs and no indication of
anything. The robot moved, and you had to watch it to know what it was doing.

## 2. Why infrared

The alternatives considered and rejected at this stage:

| Option | Why not |
|---|---|
| ultrasonic (HC-SR04) | needs a trigger pulse and an echo time; gives a distance, which is more than the first version needed, and it is blind to low, dark or angled surfaces |
| a single camera | far beyond the scope, and the platform has no camera |
| bump switches | too late — the robot would already be touching the obstacle |
| ultrasonic **and** IR | better coverage, but a second sensor type, more pins, more code, for a first build |

IR won because it is a single digital output per module: cheap, available, and
trivial to read.

## 3. The first pseudocode

```text
loop:
    read the front sensors
    if an obstacle is in front:
        stop
        reverse
        pick left or right at random
        turn
    else:
        drive forward
```

Note what is missing even here: anything about the sensors *behind* the robot.
The rear pair was added later, once the robot had actually been built and it
became obvious that a robot that reverses without looking behind is a robot that
reverses into things.

## 4. The first firmware: the version that used `delay()`

The first working sketch was a direct transcription of the pseudocode:

```cpp
// FIRST VERSION - not the final firmware, kept here as history
void loop() {
  int left  = digitalRead(IR_FRONT_LEFT);
  int right = digitalRead(IR_FRONT_RIGHT);

  if (left == HIGH || right == HIGH) {   // note: also the wrong polarity
    stopMotors();
    delay(500);

    driveBackward();
    delay(700);

    if (random(0, 2) == 0) turnLeft(); else turnRight();
    delay(600);
  } else {
    driveForward();
  }
}
```

It worked — on a table, with the robot unplugged most of the time. It had three
real problems:

1. **`delay()` freezes everything.** During those 1.8 s the sketch read no
   sensors, so the robot could not notice anything that changed. Recorded in
   [10 Troubleshooting §7](../docs/10_Troubleshooting.md#7-the-robot-goes-deaf-while-reversing).
2. **The sensor polarity was wrong.** It treated `HIGH` as "obstacle" on a
   module whose `OUT` goes LOW on an obstacle.
3. **No rear sensing at all**, so a reversing robot was a blind reversing robot.

## 5. What changed, and why

| Problem | What it was replaced with | Shipped? | Where it is documented |
|---|---|---|---|
| `delay()` froze the loop | an 8-state machine driven by `millis()` | **no** — the loop still blocks | [13 §1.1](../docs/13_Future_Improvements.md#11-make-the-loop-non-blocking) |
| wrong sensor polarity | one place that decides it: `SENSOR_ACTIVE_LOW` | **no** — no switch, two literal `200` tests | [07 §6](../docs/07_Software_Architecture.md#6-why-the-sensors-are-read-with-analogread) |
| no rear sensing | stop → forward creep → stop | **yes**, as a blocking sequence | [08 §6](../docs/08_Algorithm.md#6-case-2--obstacle-behind) |
| nothing visible about the state | 4 indicator LEDs, 2 red front / 2 white rear | **yes**, but on four pins the shield owns | [12 §5](../docs/12_Final_Implementation.md#5-indicator-behaviour) |
| no signal at power-on | 2 s: all four LEDs on, then off | **yes** | [12 §8](../docs/12_Final_Implementation.md#8-final-robot-behaviour) |

## 6. The lesson from this stage

The behaviour described in the first sketch is still the behaviour in the final
one. What changed was **how** it is executed: not *what* the robot does, but
that the timing became non-blocking, the polarity became explicit, the rear was
covered, and the state became visible from outside the robot.

Next: [analog_sensor_experiment.md](analog_sensor_experiment.md).
