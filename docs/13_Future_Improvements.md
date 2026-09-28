# 13 — Future Improvements

Everything on this page is **deliberately not part of the final design**. The
robot in [12 Final Implementation](12_Final_Implementation.md) is complete and
documented; this page is the honest list of what it does not do, ordered by how
much it would improve the machine.

Nothing here has been implemented, and none of it has been quietly added to the
firmware.

---

## 1. Reading the sensor more than once per poll

**Now:** the four FC-51 modules are read once every 50 ms, and the value is
taken as-is.

**Why it matters:** a single sample of a reflective sensor is noisy. A speck of
dust, a shadow, or a wheel-splash can produce one false reading — and one false
front reading triggers a whole 1.8 s stop-reverse-turn cycle.

**Option:** require `N` consecutive identical readings before acting, e.g. 3 of 3
at 50 ms. Cost: up to 150 ms more reaction time, and roughly 15 lines of code.

**Note:** averaging and debouncing were already explored during the analog
approach — see [history/analog_sensor_experiment.md](../history/analog_sensor_experiment.md).
It is not implemented because it changes the documented timing, so it is a
design decision, not an oversight.

## 2. Two sensor heights, as originally intended

**Now:** the lower deck carries the four IR modules at ≈ 1.8 cm; the upper deck
carries the four LEDs at ≈ 5.2 cm.

**Originally:** the idea was two *sensor* levels, to catch obstacles of
different heights — a low one for small obstacles, a higher one for something
taller than the chassis can see past.

**Blocked by:** the pin budget. Eight pins are already used by sensors and LEDs
out of the digital-capable pins the shield leaves free; there is no ninth
digital-capable pin on an Uno with this shield.

**Options if it is ever built:**

* move to a board with more GPIO (a Mega, or an I/O expander on the I²C bus);
* use A0/A1 with `analogWrite()` as PWM inputs… but A0/A1 cannot do `digitalRead()`,
  so a digital-output sensor cannot use them;
* use an I/O expander such as a PCF8574 for the second sensor row.

This is the clearest example in the project of a good idea that the chosen
controller cannot afford.

## 3. Re-evaluate obstacles mid-sequence

**Now:** only the `FORWARD` state reads the sensors. Once the robot commits to
the stop-reverse-turn sequence it completes all 1.8 s of it regardless of what
it sees — see [08 §7](08_Algorithm.md#7-why-the-sensor-read-is-inside-the-forward-case).

**Consequence, tested:** an obstacle that moves into the robot's path during the
reverse is not noticed (test T7).

**Option:** add an abort condition to `STATE_REVERSE` — for example, if both
front sensors are clear for 100 ms, skip the turn and resume forward. This adds
two states and changes the observable sequence, so it belongs here and not in
the final firmware.

## 4. Turn by angle instead of by time

**Now:** `TURN_MS = 600`. The robot rotates for a fixed time, so the angle
covered depends on the floor, the battery voltage and the load.

**Option:** drive the turn until one of the front sensors sees a clear path, or
until an encoder-based angle is reached. Neither exists on this build: there are
no encoders, and the sensors are at the corners rather than in front of the
turn axis.

## 5. Encoders and closed-loop speed control

**Now:** every motor gets the same PWM value and is assumed to turn at the same
speed. The robot therefore drifts to one side on a rough floor, as observed in
[10 Troubleshooting §3](10_Troubleshooting.md#3-a-single-wheel-turns-more-slowly-than-the-others).

**Option:** encoders on the motor shafts, with a per-motor PID so that
`run(FORWARD)` really means the same linear speed on all four wheels, and a
line-following line on the IMU. This is the single largest change to the
mechanical and electrical design, and the one that most improves reliability.

## 6. Power and measurement improvements

| Item | Why |
|---|---|
| Measure the current | no current figure in this project is measured; the rail behaviour under load is described, not quantified |
| Inline fuse | sized once the current is known — a cheap way to make the pack safe |
| Battery voltage monitoring | an analog input reading the pack would let the firmware refuse to start on a flat cell |
| A pack that is characterised | capacity, internal resistance and maximum current, measured rather than read from a label — see [11 Power System §8](11_Power_System.md#8-how-to-characterise-a-pack-before-trusting-it) |
| Better cells | a genuine high-current cell would fix the "rear motors sometimes do not start" symptom at its root |

## 7. Replace the fixed-`TURN_MS` behaviour with a map-free escape strategy

**Now:** a random left/right choice, once, when the turn begins.

**Weakness:** two runs in the same room can repeat the same dead end, and a
random turn does not prefer the side with more free space.

**Options:** bias the random choice with the number of consecutive failed
escapes; remember the last successful turn direction; or add a "creep forward
while turning" state so the robot slides along a wall instead of turning in
place. Each is a behaviour change and is therefore not in the final firmware.

## 8. Communications and telemetry

**Not implemented at all:** no Bluetooth, no Wi-Fi, no serial reporting of state
in the final firmware.

**Options:** print the current state over the serial port at 9600 baud during
development; add a Bluetooth module on the software-serial pins; add a small
OLED to show the state without opening a laptop.

Useful, but it does not make the robot better at avoiding obstacles — it makes
it easier to debug.

## 9. Robustness improvements

| Improvement | Effect |
|---|---|
| Watchdog timer | the robot recovers from a firmware hang instead of sitting still |
| Filter obviously impossible readings (both rear sensors triggered while both front ones are clear) | rejects some electrical noise |
| Hysteresis on the sensor threshold | fewer stop/creep cycles on a marginal surface |
| Light-dependent auto-calibration of the trimpots | needs a servo or a digital pot — not applicable to a plain FC-51 |
| Mute the turn blink or make it dimmer | cosmetic |

## 10. Improvements that are explicitly *not* recommended without more work

* **Increasing the speed above 150/255.** More speed means more current and less
  time to react; the current pack is already the limiting factor.
* **Removing the startup delay.** The 3 s delay is a safety feature: it lets you
  put the robot down and take your hand away before it moves.
* **Swapping the final code for a library-based line follower.** That is a
  different project, and the reaction behaviour documented in
  [08 Algorithm](08_Algorithm.md) would no longer describe the machine.

## 11. If you build this yourself

The order that works:

1. Build the mechanical chassis and check the wheels by hand.
2. One motor at a time, on blocks — [09 Testing stage C](09_Testing.md#5-stage-c--one-motor-at-a-time).
3. All four together — [stage D](09_Testing.md#6-stage-d--all-four-motors-together).
4. Characterise the pack — [11 Power System §8](11_Power_System.md#8-how-to-characterise-a-pack-before-trusting-it).
5. One sensor at a time, including the LOW/HIGH check — [stage E](09_Testing.md#7-stage-e--one-ir-sensor-at-a-time).
6. LEDs, then the whole robot — [stages F and G](09_Testing.md#8-stage-f--sensors-and-leds-together).
7. Only then consider anything from this page.
