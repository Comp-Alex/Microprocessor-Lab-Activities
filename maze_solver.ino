// =============================================================================
//  AUTONOMOUS MAZE SOLVER — LEFT-HAND RULE + PID WALL FOLLOWING
//  Target Hardware : Arduino Uno + L298N + 3× HC-SR04 (V-mount, ±25°)
//  Maze Cell       : 40 × 40 cm  |  Robot: ~17 cm wide, ~26 cm long
// =============================================================================

// ---------------------------------------------------------------------------
// PIN ASSIGNMENTS
// ---------------------------------------------------------------------------
// -- Left Motor --
#define ENA  5   // PWM speed
#define IN1  4   // direction 1
#define IN2  3   // direction 2

// -- Right Motor --
#define ENB  6   // PWM speed
#define IN3  7   // direction 1
#define IN4  8   // direction 2

// -- Ultrasonic Sensors --
#define LEFT_TRIG   A0
#define LEFT_ECHO   A1
#define FRONT_TRIG  A2
#define FRONT_ECHO  A3
#define RIGHT_TRIG  A4
#define RIGHT_ECHO  A5

// ---------------------------------------------------------------------------
// NAVIGATION THRESHOLDS  (all values in centimetres)
// ---------------------------------------------------------------------------
#define WALL_AHEAD_CM        22.0f   // front blocked → trigger turn logic
#define EMERGENCY_BACKUP_CM   8.0f   // front critical → reverse immediately
#define OPEN_SIDE_CM         30.0f   // side "open" → can turn that way
#define PID_TARGET_CM        15.0f   // desired distance from left wall

// ---------------------------------------------------------------------------
// MOTOR PARAMETERS
// ---------------------------------------------------------------------------
#define BASE_SPEED      145   // straight-line PWM  (0–255)
#define MOTOR_BIAS       10   // right motor trim to go straight
#define MIN_MOTOR_PWM    70   // anti-stall floor
#define MAX_MOTOR_PWM   255   // ceiling

// ---------------------------------------------------------------------------
// PID GAINS
// ---------------------------------------------------------------------------
#define KP  1.6f
#define KI  0.003f
#define KD  4.2f
#define PID_DEADBAND  1.5f   // ±cm — no correction inside this band

// ---------------------------------------------------------------------------
// EMA (EXPONENTIAL MOVING AVERAGE) FILTER
// ---------------------------------------------------------------------------
#define EMA_ALPHA  0.30f     // 0 = very smooth / slow, 1 = raw / fast

// ---------------------------------------------------------------------------
// TIMING CONSTANTS
// ---------------------------------------------------------------------------
#define SENSOR_STAGGER_MS    20     // min gap between sensor pulses (crosstalk)
#define US_TIMEOUT_US      3000     // ~51 cm max range (2 × 51 cm / 34300 cm·s⁻¹)
#define LOOP_TIMEOUT_MS    2500     // failsafe for any while() loop
#define BRAKE_DURATION_MS    60     // how long to hold brake before a turn
#define BACKUP_DURATION_MS  300     // reverse duration for emergency backup

// ---------------------------------------------------------------------------
// TURN TIMING  (coarse open-loop — tune on your surface)
// ---------------------------------------------------------------------------
#define TURN_90_MS   480    // ms to spin ~90° in place
#define TURN_180_MS  960    // ms to spin ~180° in place

// =============================================================================
//  STATE & GLOBALS
// =============================================================================

// EMA-smoothed sensor readings
float distLeft  = 40.0f;
float distFront = 40.0f;
float distRight = 40.0f;

// PID internals
float pidIntegral    = 0.0f;
float pidLastError   = 0.0f;
unsigned long pidLastTime = 0;

// Action label for Serial debug
char currentAction[24] = "INIT";

// =============================================================================
//  SETUP
// =============================================================================
void setup() {
  Serial.begin(115200);

  // Motor driver pins
  pinMode(ENA, OUTPUT);  pinMode(IN1, OUTPUT);  pinMode(IN2, OUTPUT);
  pinMode(ENB, OUTPUT);  pinMode(IN3, OUTPUT);  pinMode(IN4, OUTPUT);

  // Ultrasonic pins
  pinMode(LEFT_TRIG,  OUTPUT);  pinMode(LEFT_ECHO,  INPUT);
  pinMode(FRONT_TRIG, OUTPUT);  pinMode(FRONT_ECHO, INPUT);
  pinMode(RIGHT_TRIG, OUTPUT);  pinMode(RIGHT_ECHO, INPUT);

  motorBrake();   // safe start

  // Warm-up: prime EMA with a few real readings
  for (uint8_t i = 0; i < 8; i++) {
    readAllSensors();
    delay(40);
  }

  pidLastTime = millis();
  Serial.println(F("=== MAZE SOLVER READY ==="));
}

// =============================================================================
//  MAIN LOOP
// =============================================================================
void loop() {
  readAllSensors();
  printDebug();
  navigate();
}

// =============================================================================
//  SENSOR LAYER
// =============================================================================

// ---------------------------------------------------------------------------
// Single HC-SR04 pulse — returns distance in cm, or 99.0 on timeout
// ---------------------------------------------------------------------------
float pingCm(uint8_t trigPin, uint8_t echoPin) {
  // Ensure trigger is LOW before pulse
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  unsigned long duration = pulseIn(echoPin, HIGH, US_TIMEOUT_US);
  if (duration == 0) return 99.0f;   // timeout → treat as open / far

  return (float)duration * 0.01716f; // cm  (÷ 2, × 0.0343 cm/µs)
}

// ---------------------------------------------------------------------------
// Read all three sensors with staggered firing (crosstalk prevention)
// Update EMA-smoothed globals
// ---------------------------------------------------------------------------
void readAllSensors() {
  float rawLeft  = pingCm(LEFT_TRIG,  LEFT_ECHO);
  delay(SENSOR_STAGGER_MS);
  float rawFront = pingCm(FRONT_TRIG, FRONT_ECHO);
  delay(SENSOR_STAGGER_MS);
  float rawRight = pingCm(RIGHT_TRIG, RIGHT_ECHO);
  delay(SENSOR_STAGGER_MS);

  // Apply EMA: new = alpha × raw + (1−alpha) × previous
  distLeft  = EMA_ALPHA * rawLeft  + (1.0f - EMA_ALPHA) * distLeft;
  distFront = EMA_ALPHA * rawFront + (1.0f - EMA_ALPHA) * distFront;
  distRight = EMA_ALPHA * rawRight + (1.0f - EMA_ALPHA) * distRight;
}

// =============================================================================
//  NAVIGATION LOGIC  (strict priority order)
// =============================================================================
void navigate() {

  // --- PRIORITY 1: Emergency backup ---
  if (distFront <= EMERGENCY_BACKUP_CM) {
    strcpy(currentAction, "EMERGENCY BACKUP");
    emergencyBackup();
    return;
  }

  // --- PRIORITY 2: Wall ahead ---
  if (distFront < WALL_AHEAD_CM) {
    strcpy(currentAction, "WALL AHEAD");
    motorBrake();
    delay(BRAKE_DURATION_MS);

    if (distLeft > OPEN_SIDE_CM) {
      strcpy(currentAction, "TURN LEFT");
      turnLeft90();
    } else if (distRight > OPEN_SIDE_CM) {
      strcpy(currentAction, "TURN RIGHT");
      turnRight90();
    } else {
      strcpy(currentAction, "U-TURN");
      smartUTurn();
    }
    return;
  }

  // --- PRIORITY 3: Both sides open → go straight, no PID ---
  if (distLeft > OPEN_SIDE_CM && distRight > OPEN_SIDE_CM) {
    strcpy(currentAction, "STRAIGHT (open)");
    motorForward(BASE_SPEED, BASE_SPEED + MOTOR_BIAS);
    return;
  }

  // --- PRIORITY 4: PID wall-follow on left wall ---
  strcpy(currentAction, "PID FOLLOW");
  pidWallFollow();
}

// =============================================================================
//  PID WALL FOLLOWING
// =============================================================================
void pidWallFollow() {
  float error = distLeft - PID_TARGET_CM;  // positive → too far, negative → too close

  // Deadband: ignore tiny errors to prevent jitter
  if (error > -PID_DEADBAND && error < PID_DEADBAND) {
    error = 0.0f;
  }

  unsigned long now = millis();
  float dt = (float)(now - pidLastTime) / 1000.0f;
  if (dt <= 0.0f) dt = 0.001f;
  pidLastTime = now;

  pidIntegral  += error * dt;
  float derivative = (error - pidLastError) / dt;
  pidLastError  = error;

  float correction = KP * error + KI * pidIntegral + KD * derivative;

  // Positive correction → robot too far from left → steer left (reduce right motor)
  int leftPWM  = (int)(BASE_SPEED - correction);
  int rightPWM = (int)(BASE_SPEED + correction + MOTOR_BIAS);

  leftPWM  = constrain(leftPWM,  MIN_MOTOR_PWM, MAX_MOTOR_PWM);
  rightPWM = constrain(rightPWM, MIN_MOTOR_PWM, MAX_MOTOR_PWM);

  motorForward(leftPWM, rightPWM);
}

// =============================================================================
//  TURN PRIMITIVES
// =============================================================================

// ---------------------------------------------------------------------------
// Instant brake — kills momentum before a turn
// ---------------------------------------------------------------------------
void motorBrake() {
  // Short active brake: both sides forward simultaneously (H-bridge brake mode)
  digitalWrite(IN1, HIGH); digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, HIGH);
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);
}

// ---------------------------------------------------------------------------
// Forward with individual PWM for each motor
// ---------------------------------------------------------------------------
void motorForward(int leftPWM, int rightPWM) {
  // Left motor forward
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  analogWrite(ENA, leftPWM);

  // Right motor forward
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
  analogWrite(ENB, rightPWM);
}

// ---------------------------------------------------------------------------
// Reverse both motors
// ---------------------------------------------------------------------------
void motorReverse(int leftPWM, int rightPWM) {
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
  analogWrite(ENA, leftPWM);

  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
  analogWrite(ENB, rightPWM);
}

// ---------------------------------------------------------------------------
// Spin left in place (left back, right forward)
// ---------------------------------------------------------------------------
void spinLeft(int pwm) {
  digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH);
  analogWrite(ENA, pwm);

  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
  analogWrite(ENB, pwm);
}

// ---------------------------------------------------------------------------
// Spin right in place (left forward, right back)
// ---------------------------------------------------------------------------
void spinRight(int pwm) {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  analogWrite(ENA, pwm);

  digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH);
  analogWrite(ENB, pwm);
}

// ---------------------------------------------------------------------------
// 90° left pivot with failsafe timeout
// ---------------------------------------------------------------------------
void turnLeft90() {
  motorBrake();
  delay(BRAKE_DURATION_MS);
  pidIntegral  = 0.0f;  // reset PID state after a turn
  pidLastError = 0.0f;

  unsigned long start = millis();
  spinLeft(BASE_SPEED);
  while (millis() - start < TURN_90_MS) { /* timed spin */ }
  motorBrake();
}

// ---------------------------------------------------------------------------
// 90° right pivot with failsafe timeout
// ---------------------------------------------------------------------------
void turnRight90() {
  motorBrake();
  delay(BRAKE_DURATION_MS);
  pidIntegral  = 0.0f;
  pidLastError = 0.0f;

  unsigned long start = millis();
  spinRight(BASE_SPEED);
  while (millis() - start < TURN_90_MS) { /* timed spin */ }
  motorBrake();
}

// ---------------------------------------------------------------------------
// Smart U-turn: spin until front sensor clears WALL_AHEAD_CM
// Falls back to fixed time if sensor never clears (failsafe)
// ---------------------------------------------------------------------------
void smartUTurn() {
  motorBrake();
  delay(BRAKE_DURATION_MS);
  pidIntegral  = 0.0f;
  pidLastError = 0.0f;

  unsigned long start = millis();
  spinRight(BASE_SPEED);   // default: U-turn to the right

  while (millis() - start < LOOP_TIMEOUT_MS) {
    // Sample front sensor mid-turn (no EMA — raw speed matters here)
    float rawFront = pingCm(FRONT_TRIG, FRONT_ECHO);
    if (rawFront >= WALL_AHEAD_CM) break;   // facing clear direction
    delay(10);
  }

  motorBrake();
  // Refresh EMA after a long spin so stale values don't mislead PID
  for (uint8_t i = 0; i < 4; i++) {
    readAllSensors();
  }
}

// ---------------------------------------------------------------------------
// Emergency backup: reverse then reassess
// ---------------------------------------------------------------------------
void emergencyBackup() {
  pidIntegral  = 0.0f;
  pidLastError = 0.0f;

  unsigned long start = millis();
  motorReverse(BASE_SPEED, BASE_SPEED + MOTOR_BIAS);

  while (millis() - start < BACKUP_DURATION_MS) {
    // Back up for fixed time — front is too close to read reliably
  }

  motorBrake();
  delay(BRAKE_DURATION_MS);
  // Re-read sensors after reversing
  readAllSensors();
}

// =============================================================================
//  SERIAL DEBUG OUTPUT
// =============================================================================
void printDebug() {
  Serial.print(F("L:"));    Serial.print(distLeft,  1);
  Serial.print(F("  F:"));  Serial.print(distFront, 1);
  Serial.print(F("  R:"));  Serial.print(distRight, 1);
  Serial.print(F("  >> ")); Serial.println(currentAction);
}

// =============================================================================
//  END OF FILE
// =============================================================================
