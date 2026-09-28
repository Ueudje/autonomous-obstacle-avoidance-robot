# History — The Analog Sensor Experiment

> **This is project history, not the final design.**
> The final firmware reads the FC-51 modules with `digitalRead()` and never calls
> `analogRead()` — see
> [07 Software Architecture §6](../docs/07_Software_Architecture.md#6-why-the-sensors-are-read-digitally).
> The two-sensor-height idea explored here is also history.

---

## 1. The starting assumption

The first plan was to treat each IR sensor as an **analog** measurement:

```cpp
// EXPLORED, NOT IMPLEMENTED
int readDistance(int pin) {
  return analogRead(pin);          // 0..1023
}
bool obstacleNear(int pin, int threshold) {
  return analogRead(pin) > threshold;
}
```

The reasoning was reasonable at the time: an IR emitter's reflected signal
weakens with distance, so a raw reading "should" carry distance information,
and a threshold would turn it into a yes/no.

## 2. What was explored with the analog idea

| Idea | What it was supposed to do |
|---|---|
| **Threshold-based detection** | one fixed threshold per sensor; above it = obstacle |
| **Averaging** | take several `analogRead()` samples and average them, to smooth the noise |
| **Debouncing** | require the reading to hold for a few scans before acting, to reject a single glitch |
| **Calibration** | record a "clear floor" value at start-up and compare against it |
| **Periodic recalibration with `millis()`** | re-record the clear-floor value every few seconds, because ambient light changes |

That last one is the most interesting: it used exactly the non-blocking timing
pattern that the final firmware uses everywhere, months before the state
machine was written.

## 3. Why it was dropped

The FC-51 module has an **LM393 comparator** on its output. The analog input is
already behind the comparator, so the pin the Arduino sees is a **digital**
output: LOW or HIGH, nothing in between. The plan of reading a proportional
distance from that pin was never going to work — the information is destroyed
inside the module before the wire reaches the Arduino.

Once that was understood, the consequences followed:

| Analog idea | What it becomes with a digital output |
|---|---|
| threshold | not needed — the module's own trimpot **is** the threshold, and it is adjustable in hardware |
| averaging | not needed for a digital level; if a single sample is unreliable, the fix is debouncing, not averaging |
| calibration of a "clear floor" value | not needed; there is no proportional value to calibrate |
| periodic recalibration with `millis()` | not needed for the same reason |

The trimpot replaced the threshold, the firmware became simpler, and the
"calibration" work disappeared rather than being maintained.

**The timing knowledge was not wasted.** The `millis()` pattern that was
originally needed for periodic recalibration is the same pattern that now drives
the sensor poll, the LED blink and every state deadline.

## 4. What the digital approach required to get right

Switching to `digitalRead()` introduced one genuinely new problem: **which level
means "obstacle"?**

```cpp
// final firmware
#define SENSOR_ACTIVE_LOW  1
bool isObstacle(uint8_t pin) {
  int level = digitalRead(pin);
#if SENSOR_ACTIVE_LOW
  return level == LOW;          // standard FC-51: LOW while blocked
#else
  return level == HIGH;
#endif
}
```

The comparison lives in exactly one function so that a module with the opposite
behaviour is a one-character change, not an edit in four places. The bench test
that decides it is in
[09 Testing step 4](../docs/09_Testing.md#step-4--lowhigh-sensor-logic).

## 5. A2–A5: the pin question this raised

Once the sensors were digital inputs, they needed digital-capable pins. On an
Uno with the motor shield fitted, only a handful are free. The conclusion:

* A2, A3, A4 and A5 are printed in the **analog header** but are ordinary GPIO
  pins (digital 16–19) — they work with `pinMode()`, `digitalRead()` and
  `digitalWrite()`.
* A0 and A1 (digital 14 and 15) are **analog input only** on the ATmega328P;
  `digitalRead()` and `digitalWrite()` do not work on them.

So the four sensors went on A2–A5, and A0/A1 were left alone. The "physical pin
type vs. how software uses it" distinction is called out explicitly in
[04 §7](../docs/04_Electrical_Architecture.md#7-the-physical-pin-header-is-not-the-electrical-role)
and [07 §6](../docs/07_Software_Architecture.md#6-why-the-sensors-are-read-digitally).

## 6. The second sensor height — an idea the pin budget killed

The original mechanical concept was **two sensor levels**: a low row to catch
small obstacles and a higher row to catch something taller. The layout the
robot ended up with is also two levels, but the upper one carries the **LEDs**,
not a second row of sensors.

![Robot side view](../diagrams/robot_side_view.png)

| What the brief allows | Count |
|---|---|
| Digital-capable pins free after the motor shield | D2, D9, D10, D13, A2, A3, A4, A5 — **8** |
| Needed by: 4 IR sensors + 4 LEDs | **8** |
| Left over for a second sensor row | **0** |

So the second sensor height is a good idea that this controller cannot afford. It
is written up as open work, with the options for making room, in
[13 Future Improvements §2](../docs/13_Future_Improvements.md#2-two-sensor-heights-as-originally-intended).

## 7. What survived from this stage

| Kept | Dropped |
|---|---|
| `digitalRead()` on all four FC-51 modules | `analogRead()` distance measurement |
| `SENSOR_ACTIVE_LOW` as a single switch | threshold-based detection |
| debouncing as a *future* option (never needed in practice) | averaging, calibration, periodic recalibration |
| A2–A5 for the sensors | A0/A1 for anything digital |
| the two-level **chassis** | two sensor **levels** |

Next: [motor_testing.md](motor_testing.md).
