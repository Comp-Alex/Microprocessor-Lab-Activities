// ==========================================================
// MAZE SOLVER ROBOT - REVERSED RIGHT-HAND RULE
// Hardware: Arduino Uno, TB6612FNG, 3x HC-SR04
// ==========================================================

// --- Motor Driver Pins (TB6612FNG) ---
const int PWMA = 5;  // Left Motor Speed
const int AIN1 = 4;  // Left Motor Direction 1
const int AIN2 = 3;  // Left Motor Direction 2

const int PWMB = 6;  // Right Motor Speed
const int BIN1 = 7;  // Right Motor Direction 1
const int BIN2 = 8;  // Right Motor Direction 2

// --- Ultrasonic Sensor Pins ---
const int LEFT_TRIG  = A0;
const int LEFT_ECHO  = A1;
const int FRONT_TRIG = A2;
const int FRONT_ECHO = A3;
const int RIGHT_TRIG = A4;
const int RIGHT_ECHO = A5;

// ==========================================================
// 🛠️ CALIBRATION ZONE
// ==========================================================
int baseSpeed       = 120; // Normal forward speed
int turnSpeed       = 140; // Speed during turns
int wallThreshold   = 20;  // Distance in cm to consider "longer gap"
int turnDuration90  = 450; // ms for 90° turn
int turnDuration180 = 900; // ms for 180° turn
int clearWallDelay  = 250; // ms forward before turning
// ==========================================================

// --- State Machine Variables ---
int robotState = 0;       
unsigned long actionStartTime = 0;
int currentActionDuration = 0;

void setup() {
  Serial.begin(9600);

  pinMode(PWMA, OUTPUT);
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(PWMB, OUTPUT);
  pinMode(BIN1, OUTPUT);
  pinMode(BIN2, OUTPUT);

  pinMode(LEFT_TRIG, OUTPUT);
  pinMode(LEFT_ECHO, INPUT);
  pinMode(FRONT_TRIG, OUTPUT);
  pinMode(FRONT_ECHO, INPUT);
  pinMode(RIGHT_TRIG, OUTPUT);
  pinMode(RIGHT_ECHO, INPUT);

  stopMotors();
  delay(2000);
}

void loop() {
  // Handle ongoing timed actions
  if (robotState != 0) {
    if (millis() - actionStartTime >= currentActionDuration) {
      if (robotState == 4) {
        startAction(1, turnDuration90);
        executeRightTurn();
      } else {
        stopMotors();
        robotState = 0;
      }
    }
    return;
  }

  // Read sensors
  float distRight = getDistance(RIGHT_TRIG, RIGHT_ECHO);
  float distFront = getDistance(FRONT_TRIG, FRONT_ECHO);
  float distLeft  = getDistance(LEFT_TRIG, LEFT_ECHO);

  Serial.print("L: "); Serial.print(distLeft);
  Serial.print(" | F: "); Serial.print(distFront);
  Serial.print(" | R: "); Serial.println(distRight);

  // Reversed right-hand rule logic
  if (distRight > wallThreshold) {
    startAction(4, clearWallDelay);
    executeForward();
  } 
  else if (distFront > wallThreshold) {
    executeForward();
  } 
  else if (distLeft > wallThreshold) {
    startAction(2, turnDuration90);
    executeLeftTurn();
  } 
  else {
    startAction(3, turnDuration180);
    executeRightTurn();
  }
}

// ==========================================================
// HELPER FUNCTIONS
// ==========================================================
void startAction(int stateCode, int duration) {
  robotState = stateCode;
  currentActionDuration = duration;
  actionStartTime = millis();
}

float getDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 20000); 
  if (duration == 0) return 999.0;
  return duration * 0.034 / 2;
}

void executeForward() {
  digitalWrite(AIN1, HIGH);
  digitalWrite(AIN2, LOW);
  analogWrite(PWMA, baseSpeed);

  digitalWrite(BIN1, HIGH);
  digitalWrite(BIN2, LOW);
  analogWrite(PWMB, baseSpeed);
}

void executeRightTurn() {
  digitalWrite(AIN1, HIGH);
  digitalWrite(AIN2, LOW);
  analogWrite(PWMA, turnSpeed);

  digitalWrite(BIN1, LOW);
  digitalWrite(BIN2, HIGH);
  analogWrite(PWMB, turnSpeed);
}

void executeLeftTurn() {
  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, HIGH);
  analogWrite(PWMA, turnSpeed);

  digitalWrite(BIN1, HIGH);
  digitalWrite(BIN2, LOW);
  analogWrite(PWMB, turnSpeed);
}

void stopMotors() {
  analogWrite(PWMA, 0);
  analogWrite(PWMB, 0);
}
