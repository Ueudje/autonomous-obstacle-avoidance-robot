# History — Motor Testing and the Direction Fix

> **This is project history, not the final design.**
> The final motor mapping is in
> [12 Final Implementation §3](../docs/12_Final_Implementation.md#3-motor-mapping).
> The fix described here was made **in the wiring**; the firmware contains no
> per-motor inversion.

---

## 1. The problem that took the longest to diagnose

The first time all four motors were commanded forward together, the robot did
not drive. It **twisted on the spot**: the front pair turned one way and the rear
pair the other.

At that point the symptom is genuinely ambiguous. Four plausible causes:

1. the motors are wired to the wrong shield channels;
2. the left/right motors are wired in different orders;
3. two of the four motor cables are reversed;
4. the firmware drives the wrong channels in the wrong order.

Every one of those produces a robot that does not go straight.

## 2. The isolation procedure that found it

```
1. Lift the robot onto blocks. No wheel touches the floor.
2. One channel at a time, 1 s each:
       motorFL.run(FORWARD)   -> record the ROTATION DIRECTION of M1
       motorBL.run(FORWARD)   -> record M2
       motorBR.run(FORWARD)   -> record M3
       motorFR.run(FORWARD)   -> record M4
3. Repeat with BACKWARD. Each motor must reverse.
4. Repeat with RELEASE. Each motor must coast to a stop.
5. Only then: all four together.
```

The important part is **step 2**: while the robot is on blocks, the rotation
direction of a wheel is a pure function of its wiring and its mounting, with no
mechanical load and no other motor involved. Whatever differs between the four
wheels at that point is the fault.

## 3. What the test showed

| Channel | Position | Observed with `FORWARD` | Correct? |
|---|---|---|---|
| M1 | front left | anticlockwise (seen from outside) | yes |
| M2 | back left | anticlockwise (seen from outside) | yes |
| M3 | back right | **clockwise** | **no** |
| M4 | front right | **clockwise** | **no** |

The two right-hand motors were reversed relative to the two left-hand motors.

## 4. The real cause

A DC motor has **two terminals and no inherent polarity**. The L293D's H-bridge
reverses the polarity across them to change direction. Red and black were being
used as an organisational convention, and the same convention had been applied
to both sides.

That is wrong for a mirrored pair. A left wheel and a right wheel are mounted as
mirror images of each other, so **the same terminal order produces opposite
rotation**. The two sides must be wired in opposite orders for a single
`FORWARD` command to work everywhere.

The moment this was understood, the earlier confusion resolved itself: the two
right-hand motors were not "reversed by mistake" in a random way — they were
correctly wired for a *right-hand* motor and wrongly wired relative to a
*left-hand* one.

## 5. The fix

| Channel | Side | Terminal 1 | Terminal 2 |
|---|---|---|---|
| M1 | front left | red | black |
| M2 | back left | red | black |
| M3 | back right | **black** | **red** |
| M4 | front right | **black** | **red** |

Two terminal blocks were unscrewed, the two leads were exchanged, and the blocks
were refitted. The firmware was not touched.

Re-running the isolation test after the fix: all four motors rotate the same way
on `FORWARD` and all four reverse on `BACKWARD`. On the floor, the robot drove
straight forward.

## 6. A second, similar fault: the rear motors that would not start

A different symptom appeared later: on the floor, the robot would start with the
front pair but the rear pair would often stay still, then join in. The same
channel, the same command, the same firmware.

| Test | Result |
|---|---|
| All four channels forward, **on blocks** | all four start every time |
| All four channels forward, **on the floor** | rear pair frequently fails to start |
| Same command, robot pushed forward by hand first | all four start |
| Rail voltage during the start attempt | dips significantly |

A wiring fault does not depend on mechanical load. A supply fault does: a motor
that is not turning has no back EMF, so it draws its full startup current, and
the rail collapses. Pushing the robot first gave the rear wheels a little
momentum and a little back EMF, which was enough to start them.

This was a **power** problem, not a motor problem. The full analysis is in
[11 Power System](../docs/11_Power_System.md) and the experiments are in
[power_testing.md](power_testing.md).

## 7. A third fault that masqueraded as this one

Twice, the robot drove in a circle for no software reason: a wheel had come
loose on its shaft and was slipping. It looked exactly like a wrong-direction
fault and cost time to rule out. The lesson was to check the wheels by hand
before believing anything the firmware appears to be doing.

## 8. What was actually done, in order

1. Motors mounted, one channel each, checked by hand for free rotation.
2. Wired to the shield in a single consistent terminal order.
3. All four commanded together on blocks — **failed**, front and rear opposite.
4. One channel at a time, rotation direction recorded — **M3 and M4 reversed**.
5. Cause identified: the left/right mirror was missing.
6. Right-hand terminal blocks swapped.
7. Re-tested: all four identical.
8. Powered on the floor: drove straight forward.
9. Later, the "rear motors do not start" symptom was traced to the power supply
   rather than to the wiring — after the wiring had already been proved correct
   in step 7.

## 9. Lessons kept in the final documentation

| Lesson | Where it is written down |
|---|---|
| A motor has no permanent VCC/GND; red/black is a convention | [05 Wiring §4](../docs/05_Wiring.md#4-motor-wiring-and-the-leftright-mirror) |
| Left and right motors are a mirrored pair and must be wired mirrored | [05 Wiring §4](../docs/05_Wiring.md#4-motor-wiring-and-the-leftright-mirror) |
| Fix direction in the wiring, never in the code | [07 §10](../docs/07_Software_Architecture.md#11-afmotor-and-the-direction-constants) |
| Test one channel at a time, on blocks | [09 Testing stage C](../docs/09_Testing.md#5-stage-c--one-motor-at-a-time) |
| "Works on blocks" ≠ "works on the floor" | [10 Troubleshooting §2](../docs/10_Troubleshooting.md#2-motors-do-not-move-together) |
| Check the wheels before believing the firmware | [10 Troubleshooting §4](../docs/10_Troubleshooting.md#4-wheels-coming-loose-on-the-shaft) |

Next: [power_testing.md](power_testing.md).
