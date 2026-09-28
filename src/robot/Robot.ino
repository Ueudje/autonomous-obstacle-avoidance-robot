/* =====================================================================
 * Autonomous Obstacle-Avoidance Robot  --  FINAL IMPLEMENTATION
 * ---------------------------------------------------------------------
 * Platform : Arduino Uno (ATmega328P)
 * Driver   : L293D / Adafruit Motor Shield V1 compatible shield
 * Library  : AFMotor (Adafruit Motor Shield Library)
 *
 * Hardware (final, see docs/12_Final_Implementation.md):
 *   4x DC geared motors   M1 = front-left, M2 = back-left,
 *                         M3 = back-right, M4 = front-right
 *   4x FC-51 IR obstacle sensors (digital OUT)  front/rear x left/right
 *   4x indicator LEDs    2 red (front), 2 white (rear), series resistors
 *   2x 3.7 V 18650 cells in series -> 7.4 V motor rail (to be verified)
 *
 * BEHAVIOUR (matches docs/08_Algorithm.md exactly)
 *   no obstacle     -> drive forward, front LEDs steady, rear LEDs off
 *   front obstacle  -> stop (all 4 LEDs on) -> reverse (rear LEDs blink)
 *                      -> pick a random turn direction (the two LEDs of
 *                      that side blink) -> turn -> resume forward
 *   rear obstacle   -> stop -> drive forward -> stop -> resume normal
 *
 * TIMING MODEL
 *   The whole robot is a non-blocking state machine driven by millis().
 *   This file contains no delay() call at all, so the sensor scan, the
 *   LED blink task and the motor task can never block each other.
 * ===================================================================== */

#include <AFMotor.h>

/* ---------------------------------------------------------------------
 * 1. MOTOR OBJECTS
 *    Channel numbers follow the silkscreen printed on the shield.
 * ------------------------------------------------------------------- */
AFMotor motorFL(1);   // M1 - front left
AFMotor motorBL(2);   // M2 - back  left
AFMotor motorBR(3);   // M3 - back  right
AFMotor motorFR(4);   // M4 - front right

/* Motor speed on the 0..255 scale. Final value used in the project. */
const uint8_t MOTOR_SPEED = 150;

/* ---------------------------------------------------------------------
 * 2. PIN MAP
 *    AFMotor already owns 3, 4, 5, 6, 7, 8, 11 and 12, so none of the
 *    pins below collide with the motor driver.  Read the
 *    "Pin Conflict / Verification" section of
 *    docs/12_Final_Implementation.md before changing any of them.
 *
 *      3 / 5 / 6 / 11   motor PWM for M2 / M4 / M3 / M1
 *      4 / 7 / 8 / 12   74HC595 shift register  CLK / EN / DATA / LATCH
 *
 *    The IR sensors sit on the ANALOG header because A2..A5 are also
 *    digital-capable GPIO pins on the ATmega328P. They are used as
 *    DIGITAL inputs here through digitalRead(); analogRead() is not
 *    used anywhere in this final implementation.
 * ------------------------------------------------------------------- */
#define IR_FRONT_LEFT   A2   // digital pin 16
#define IR_FRONT_RIGHT  A3   // digital pin 17
#define IR_REAR_LEFT    A4   // digital pin 18
#define IR_REAR_RIGHT   A5   // digital pin 19

#define LED_FRONT_LEFT  2
#define LED_FRONT_RIGHT 9
#define LED_REAR_LEFT   10
#define LED_REAR_RIGHT  13

/* ---------------------------------------------------------------------
 * 3. SENSOR ELECTRICAL INTERFACE
 *    Measured assumption for a standard FC-51 / LM393 digital module:
 *    OUT is driven LOW while a reflecting obstacle is in range.
 *    Confirm on your own board with docs/09_Testing.md step 4, then set
 *    SENSOR_ACTIVE_LOW to 0 if your module reports the opposite.
 * ------------------------------------------------------------------- */
#define SENSOR_ACTIVE_LOW  1

/* ---------------------------------------------------------------------
 * 4. LED INDICATOR GROUPS
 *    One bit per LED keeps the indicator logic short and readable.
 * ------------------------------------------------------------------- */
const uint8_t LED_FL_BIT = 0x01;
const uint8_t LED_FR_BIT = 0x02;
const uint8_t LED_RL_BIT = 0x04;
const uint8_t LED_RR_BIT = 0x08;

const uint8_t LED_FRONT_PAIR = LED_FL_BIT | LED_FR_BIT;
const uint8_t LED_REAR_PAIR  = LED_RL_BIT | LED_RR_BIT;
const uint8_t LED_LEFT_SIDE  = LED_FL_BIT | LED_RL_BIT;
const uint8_t LED_RIGHT_SIDE = LED_FR_BIT | LED_RR_BIT;
const uint8_t LED_ALL        = LED_FRONT_PAIR | LED_REAR_PAIR;

/* ---------------------------------------------------------------------
 * 5. TIMING CONSTANTS (milliseconds)
 * ------------------------------------------------------------------- */
const unsigned long STARTUP_DELAY_MS = 3000;   // safety delay at power-on
const unsigned long OBSTACLE_POLL_MS = 50;     // sensor scan period
const unsigned long STOP_HOLD_MS    = 500;     // "stop + all LEDs on"
const unsigned long REVERSE_MS      = 700;     // backing away
const unsigned long TURN_MS         = 600;     // in-place turn
const unsigned long REAR_PAUSE_MS   = 300;     // rear reaction: stop
const unsigned long REAR_CREEP_MS   = 600;     // rear reaction: forward creep
const unsigned long LED_BLINK_MS    = 250;     // LED toggle half-period

/* ---------------------------------------------------------------------
 * 6. ROBOT STATE
 * ------------------------------------------------------------------- */
enum RobotState {
  STATE_STARTUP,      // safety delay, all four LEDs blink
  STATE_FORWARD,      // normal autonomous forward drive
  STATE_STOP_HOLD,    // full stop, all four LEDs on (front obstacle)
  STATE_REVERSE,      // reversing away, rear LEDs blink
  STATE_TURN,         // turning in place, one side's LEDs blink
  STATE_REAR_BRAKE,   // rear obstacle: stop
  STATE_REAR_CREEP,   // rear obstacle: move forward
  STATE_REAR_SETTLE   // rear obstacle: stop again
};

RobotState state = STATE_STARTUP;

/* millis() is an unsigned long and it wraps around after ~49.7 days.
 * Every interval test below is written as "now - previous >= interval"
 * so that the wrap-around is handled correctly. */
unsigned long stateStartedAt  = 0;    // millis() when the state was entered
unsigned long previousPollMs  = 0;    // last sensor scan
unsigned long previousBlinkMs = 0;    // last LED toggle

bool    obstacleFrontLeft  = false;
bool    obstacleFrontRight = false;
bool    obstacleRearLeft   = false;
bool    obstacleRearRight  = false;

uint8_t blinkMask    = 0;   // 0 = no blinking group is active
bool    blinkLevel   = false;

/* =====================================================================
 * LED TASK
 * ---------------------------------------------------------------------
 * An LED has an anode and a cathode. The current-limiting resistor may
 * be placed in series on EITHER side of the LED - electrically it makes
 * no difference. This firmware drives each LED from an Arduino pin to
 * GND, so the anode goes to the pin and the cathode goes to GND.
 * ===================================================================== */

void setLed(uint8_t pin, bool on = true)
{
  digitalWrite(pin, on ? HIGH : LOW);
}

void applyLeds(uint8_t mask)
{
  setLed(LED_FRONT_LEFT,  (mask & LED_FL_BIT) != 0);
  setLed(LED_FRONT_RIGHT, (mask & LED_FR_BIT) != 0);
  setLed(LED_REAR_LEFT,   (mask & LED_RL_BIT) != 0);
  setLed(LED_REAR_RIGHT,  (mask & LED_RR_BIT) != 0);
}

/* Steady indication: this exact group stays on, blinking is cancelled. */
void setLeds(uint8_t mask)
{
  blinkMask = 0;
  applyLeds(mask);
}

/* Blinking indication: only this group blinks, every other LED is off. */
void startBlink(uint8_t mask, bool levelOn = true)
{
  blinkMask      = mask;
  blinkLevel     = levelOn;
  previousBlinkMs = millis();
  applyLeds(levelOn ? mask : 0);
}

/* Non-blocking toggle. The classic test is
 *     millis() - previousBlinkMs >= LED_BLINK_MS                     */
void updateBlink()
{
  if (blinkMask == 0) {
    return;
  }
  if (millis() - previousBlinkMs >= LED_BLINK_MS) {
    previousBlinkMs = millis();
    blinkLevel = !blinkLevel;
    applyLeds(blinkLevel ? blinkMask : 0);
  }
}

/* =====================================================================
 * SENSOR TASK
 * ===================================================================== */

bool isObstacle(uint8_t pin)
{
  int level = digitalRead(pin);
#if SENSOR_ACTIVE_LOW
  return level == LOW;
#else
  return level == HIGH;
#endif
}

void readSensors()
{
  obstacleFrontLeft  = isObstacle(IR_FRONT_LEFT);
  obstacleFrontRight = isObstacle(IR_FRONT_RIGHT);
  obstacleRearLeft   = isObstacle(IR_REAR_LEFT);
  obstacleRearRight  = isObstacle(IR_REAR_RIGHT);
}

bool frontBlocked()
{
  return obstacleFrontLeft || obstacleFrontRight;
}

bool rearBlocked()
{
  return obstacleRearLeft || obstacleRearRight;
}

/* =====================================================================
 * MOTOR TASK  (RELEASE / FORWARD / BACKWARD through AFMotor)
 * ===================================================================== */

void setMotorSpeed(uint8_t speed)
{
  motorFL.setSpeed(speed);
  motorBL.setSpeed(speed);
  motorBR.setSpeed(speed);
  motorFR.setSpeed(speed);
}

void driveAllForward()
{
  motorFL.run(FORWARD);
  motorBL.run(FORWARD);
  motorBR.run(FORWARD);
  motorFR.run(FORWARD);
}

void driveAllBackward()
{
  motorFL.run(BACKWARD);
  motorBL.run(BACKWARD);
  motorBR.run(BACKWARD);
  motorFR.run(BACKWARD);
}

/* stopMotors(true)  -> release the motors AND show the STOP indication
 * stopMotors(false) -> release the motors, leave the LEDs as they are  */
void stopMotors(bool indicateStop)
{
  motorFL.run(RELEASE);
  motorBL.run(RELEASE);
  motorBR.run(RELEASE);
  motorFR.run(RELEASE);
  setMotorSpeed(0);
  if (indicateStop) {
    setLeds(LED_ALL);
  }
}

/* Turn on the spot.  turnLeft = true  -> left wheels reverse, right
 * wheels advance, so the robot rotates anticlockwise.                */
void turnInPlace(bool turnLeft)
{
  if (turnLeft) {
    motorFL.run(BACKWARD);
    motorBL.run(BACKWARD);
    motorBR.run(FORWARD);
    motorFR.run(FORWARD);
  } else {
    motorFL.run(FORWARD);
    motorBL.run(FORWARD);
    motorBR.run(BACKWARD);
    motorFR.run(BACKWARD);
  }
}

/* =====================================================================
 * STATE MACHINE HELPERS
 * ===================================================================== */

void changeState(RobotState next)
{
  state = next;
  stateStartedAt = millis();
}

bool stateElapsed(unsigned long durationMs)
{
  return (millis() - stateStartedAt) >= durationMs;
}

/* =====================================================================
 * BEHAVIOUR
 * ===================================================================== */

void beginStartup()
{
  stopMotors(false);
  setMotorSpeed(MOTOR_SPEED);
  startBlink(LED_ALL);
  changeState(STATE_STARTUP);
}

void beginNormalForward()
{
  setMotorSpeed(MOTOR_SPEED);
  driveAllForward();
  setLeds(LED_FRONT_PAIR);
  changeState(STATE_FORWARD);
}

/* Front obstacle - step 1: stop and indicate the stop. */
void beginStopHold()
{
  stopMotors(true);
  changeState(STATE_STOP_HOLD);
}

/* Front obstacle - step 2: reverse, rear LEDs blink together. */
void beginReverse()
{
  setMotorSpeed(MOTOR_SPEED);
  driveAllBackward();
  startBlink(LED_REAR_PAIR);
  changeState(STATE_REVERSE);
}

/* Front obstacle - step 3: random side choice, then turn. The two LEDs
 * belonging to the chosen side blink so the turn is visible.         */
void beginTurn()
{
  bool turnLeft = (random(0, 2) == 0);
  turnInPlace(turnLeft);
  if (turnLeft) {
    startBlink(LED_LEFT_SIDE);
  } else {
    startBlink(LED_RIGHT_SIDE);
  }
  changeState(STATE_TURN);
}

/* Rear obstacle - step 1: stop. */
void beginRearBrake()
{
  stopMotors(false);
  setLeds(LED_FRONT_PAIR);
  changeState(STATE_REAR_BRAKE);
}

/* Rear obstacle - step 2: move forward. */
void beginRearCreep()
{
  setMotorSpeed(MOTOR_SPEED);
  driveAllForward();
  startBlink(LED_REAR_PAIR);
  changeState(STATE_REAR_CREEP);
}

/* Rear obstacle - step 3: stop again, then resume. */
void beginRearSettle()
{
  stopMotors(false);
  setLeds(LED_FRONT_PAIR);
  changeState(STATE_REAR_SETTLE);
}

/* One non-blocking pass through the state machine. */
void updateRobot()
{
  updateBlink();

  switch (state) {

    /* ---- startup: safety delay, all four LEDs blink ----------- */
    case STATE_STARTUP:
      if (stateElapsed(STARTUP_DELAY_MS)) {
        readSensors();
        beginNormalForward();
      }
      break;

    /* ---- normal forward driving ------------------------------- */
    case STATE_FORWARD:
      if (millis() - previousPollMs >= OBSTACLE_POLL_MS) {
        previousPollMs = millis();
        readSensors();
        if (frontBlocked()) {
          beginStopHold();
        } else if (rearBlocked()) {
          beginRearBrake();
        }
      }
      break;

    /* ---- stop, all four LEDs on ------------------------------- */
    case STATE_STOP_HOLD:
      if (stateElapsed(STOP_HOLD_MS)) {
        beginReverse();
      }
      break;

    /* ---- reverse, rear LEDs blinking -------------------------- */
    case STATE_REVERSE:
      if (stateElapsed(REVERSE_MS)) {
        beginTurn();
      }
      break;

    /* ---- turn in place, chosen side blinking ------------------ */
    case STATE_TURN:
      if (stateElapsed(TURN_MS)) {
        beginNormalForward();
      }
      break;

    /* ---- rear obstacle: stop ---------------------------------- */
    case STATE_REAR_BRAKE:
      if (stateElapsed(REAR_PAUSE_MS)) {
        beginRearCreep();
      }
      break;

    /* ---- rear obstacle: move forward -------------------------- */
    case STATE_REAR_CREEP:
      if (stateElapsed(REAR_CREEP_MS)) {
        beginRearSettle();
      }
      break;

    /* ---- rear obstacle: stop, then resume --------------------- */
    case STATE_REAR_SETTLE:
      if (stateElapsed(REAR_PAUSE_MS)) {
        beginNormalForward();
      }
      break;
  }
}

/* =====================================================================
 * SETUP / LOOP
 * ===================================================================== */

void setup()
{
  /* Inputs first, so a pin is never left floating. */
  pinMode(IR_FRONT_LEFT,  INPUT);
  pinMode(IR_FRONT_RIGHT, INPUT);
  pinMode(IR_REAR_LEFT,   INPUT);
  pinMode(IR_REAR_RIGHT,  INPUT);

  pinMode(LED_FRONT_LEFT,  OUTPUT);
  pinMode(LED_FRONT_RIGHT, OUTPUT);
  pinMode(LED_REAR_LEFT,   OUTPUT);
  pinMode(LED_REAR_RIGHT,  OUTPUT);
  applyLeds(0);

  /* Motors stay released until beginStartup() releases them too, so the
   * robot cannot twitch while the pins are still being configured. */
  stopMotors(false);

  /* randomSeed(micros()) makes the left/right choice differ from one
   * power-up to the next instead of repeating a fixed sequence. */
  randomSeed(micros());

  beginStartup();
}

void loop()
{
  updateRobot();
}
