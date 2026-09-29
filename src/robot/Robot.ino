#include <AFMotor.h>
#define IR_FRONT_LEFT A0
#define IR_FRONT_RIGHT A1
#define IR_BACK_LEFT A2
#define IR_BACK_RIGHT A3
#define LED_FRONT_LEFT 6
#define LED_FRONT_RIGHT 0
#define LED_BACK_LEFT 12
#define LED_BACK_RIGHT 3
AF_DCMotor motor1(1); // Front Left
AF_DCMotor motor2(2); // Back Left
AF_DCMotor motor3(3); // Back Right
AF_DCMotor motor4(4); // Front Right
// Blink timing
const unsigned long blinkInterval = 200;
void setup() {
pinMode(IR_FRONT_LEFT, INPUT);
pinMode(IR_FRONT_RIGHT, INPUT);
pinMode(IR_BACK_LEFT, INPUT);
pinMode(IR_BACK_RIGHT, INPUT);
pinMode(LED_FRONT_LEFT, OUTPUT);
pinMode(LED_FRONT_RIGHT, OUTPUT);
pinMode(LED_BACK_LEFT, OUTPUT);
pinMode(LED_BACK_RIGHT, OUTPUT);
motor1.setSpeed(150);
  motor2.setSpeed(150);
  motor3.setSpeed(150);
  motor4.setSpeed(150);
  randomSeed(analogRead(5));
  // Startup indicator
  digitalWrite(LED_FRONT_LEFT, HIGH);
  digitalWrite(LED_FRONT_RIGHT, HIGH);
  digitalWrite(LED_BACK_LEFT, HIGH);
  digitalWrite(LED_BACK_RIGHT, HIGH);
  delay(1000);
  digitalWrite(LED_FRONT_LEFT, LOW);
  digitalWrite(LED_FRONT_RIGHT, LOW);
  digitalWrite(LED_BACK_LEFT, LOW);
  digitalWrite(LED_BACK_RIGHT, LOW);
  delay(1000);
}
void stopMotors(bool allLedsOn = false) {
  motor1.run(RELEASE);
  motor2.run(RELEASE);
  motor3.run(RELEASE);
  motor4.run(RELEASE);
  if (allLedsOn) {
    digitalWrite(LED_FRONT_LEFT, HIGH);
    digitalWrite(LED_FRONT_RIGHT, HIGH);
    digitalWrite(LED_BACK_LEFT, HIGH);
    digitalWrite(LED_BACK_RIGHT, HIGH);
  } else {
    digitalWrite(LED_FRONT_LEFT, LOW);
    digitalWrite(LED_FRONT_RIGHT, LOW);
    digitalWrite(LED_BACK_LEFT, LOW);
    digitalWrite(LED_BACK_RIGHT, LOW);
  }
}
void moveForward() {
  motor1.run(FORWARD);
  motor2.run(FORWARD);
  motor3.run(FORWARD);
  motor4.run(FORWARD);
  digitalWrite(LED_FRONT_LEFT, HIGH);
  digitalWrite(LED_FRONT_RIGHT, HIGH);
  digitalWrite(LED_BACK_LEFT, LOW);
  digitalWrite(LED_BACK_RIGHT, LOW);
}
void moveBackward() {
  unsigned long moveStart = millis();
  unsigned long lastBlink = millis();
  bool blinkState = false;
  while (millis() - moveStart < 2000) {
    if (millis() - lastBlink >= blinkInterval) {
      lastBlink = millis();
      blinkState = !blinkState;
    }
    motor1.run(BACKWARD);
    motor2.run(BACKWARD);
    motor3.run(BACKWARD);
    motor4.run(BACKWARD);
    digitalWrite(LED_FRONT_LEFT, LOW);
    digitalWrite(LED_FRONT_RIGHT, LOW);
    digitalWrite(LED_BACK_LEFT, blinkState);
    digitalWrite(LED_BACK_RIGHT, blinkState);
    delay(10);
  }
}
void turnLeft() {
  unsigned long turnStart = millis();
  unsigned long lastBlink = millis();
  bool blinkState = false;
  while (millis() - turnStart < 1000) {
    if (millis() - lastBlink >= blinkInterval) {
      lastBlink = millis();
      blinkState = !blinkState;
    }
    // Left motors go backward, Right motors go forward
    motor1.run(BACKWARD);
    motor2.run(BACKWARD);
    motor3.run(FORWARD);
    motor4.run(FORWARD);
    digitalWrite(LED_FRONT_LEFT, LOW);
    digitalWrite(LED_BACK_LEFT, LOW);
    digitalWrite(LED_FRONT_RIGHT, blinkState);
    digitalWrite(LED_BACK_RIGHT, blinkState);
    delay(10);
  }
  stopMotors(true);
}
void turnRight() {
  unsigned long turnStart = millis();
  unsigned long lastBlink = millis();
  bool blinkState = false;
  while (millis() - turnStart < 1000) {
    if (millis() - lastBlink >= blinkInterval) {
      lastBlink = millis();
      blinkState = !blinkState;
    }
    // Left motors go forward, Right motors go backward
    motor1.run(FORWARD);
    motor2.run(FORWARD);
    motor3.run(BACKWARD);
    motor4.run(BACKWARD);
    digitalWrite(LED_FRONT_RIGHT, LOW);
    digitalWrite(LED_BACK_RIGHT, LOW);
    digitalWrite(LED_FRONT_LEFT, blinkState);
    digitalWrite(LED_BACK_LEFT, blinkState);
    delay(10);
  }
  stopMotors(true);
}
void loop() {
  int frontLeft = analogRead(IR_FRONT_LEFT);
  int frontRight = analogRead(IR_FRONT_RIGHT);
  int backLeft = analogRead(IR_BACK_LEFT);
  int backRight = analogRead(IR_BACK_RIGHT);
  if (frontLeft < 200 || frontRight < 200) {
    stopMotors(true);
    delay(1000);
    moveBackward();
    stopMotors(true);
    delay(1000);
    if (random(2) == 0) {
      turnLeft();
    } else {
      turnRight();
    }
  } else if (backLeft < 200 || backRight < 200) {
    stopMotors(true);
    delay(1000);
    moveForward();
    delay(500);
    stopMotors(true);
    delay(300);
  } else {
    moveForward();
  }
}
