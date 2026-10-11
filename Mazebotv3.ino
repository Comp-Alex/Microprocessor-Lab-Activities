// ==========================================================
// MAZE SOLVER ROBOT - LEFT PREFERRED (Professor Optimized)
// STRAIGHT FORWARD until WALL → LEFT priority turns
// Motor Driver: L298N
// ==========================================================

// Motor Driver Pins (L298N)
const int ENA = 5;   // Left motor speed (PWM)
const int IN1 = 4;   // Left motor direction 1
const int IN2 = 3;   // Left motor direction 2
const int ENB = 6;   // Right motor speed (PWM)
const int IN3 = 7;   // Right motor direction 1
const int IN4 = 8;   // Right motor direction 2  

// Ultrasonic Sensor Pins
const int LEFT_TRIG  = A0;
const int LEFT_ECHO  = A1;
const int FRONT_TRIG = A2;
const int FRONT_ECHO = A3;
const int RIGHT_TRIG = A4;
const int RIGHT_ECHO = A5;

// TUNED PARAMETERS (Adjust these for your robot)
int baseSpeedLeft   = 130;  // Left motor slightly faster (voltage compensation)
int baseSpeedRight  = 130;
int turnSpeedLeft   = 150;
int turnSpeedRight  = 150;
int wallThreshold   = 18;   // Distance to detect wall (cm)
int closeThreshold  = 18;   // Distance to consider wall close for pivoting (cm)
int turnDuration90  = 420;  // ms for 90° turn (TUNE THIS: 400-500)
int turnDuration180 = 850;  // ms for 180° U-turn (TUNE THIS: 800-1000)
int forwardDelay    = 180;  // ms between forward checks

// State Machine
int robotState = 0;         // 0=scan, 1=forward, 2=left90, 3=u-turn, 4=right90
unsigned long actionStartTime = 0;
int currentActionDuration = 0;

void setup() {
  Serial.begin(9600);
  Serial.println("=== MAZE BOT v2.0 - LEFT PREFERRED ===");
  Serial.println("Logic: FORWARD → LEFT → RIGHT → U-TURN");

  // Motor control pins
  pinMode(ENA, OUTPUT); pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(ENB, OUTPUT); pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  
  // Sensor pins
  pinMode(LEFT_TRIG, OUTPUT);  pinMode(LEFT_ECHO, INPUT);
  pinMode(FRONT_TRIG, OUTPUT); pinMode(FRONT_ECHO, INPUT);
  pinMode(RIGHT_TRIG, OUTPUT); pinMode(RIGHT_ECHO, INPUT);

  stopMotors();
  delay(1000); // 1 second startup (faster!)
  Serial.println(">>> READY! Place in maze...");
  Serial.println("Tune: turnDuration90=" + String(turnDuration90) + " turnDuration180=" + String(turnDuration180));
}

void loop() {
  // 1. STATE MACHINE - Handle active movements
  if (robotState != 0) {
    if (millis() - actionStartTime >= currentActionDuration) {
      stopMotors();
      robotState = 0;
      Serial.println("--- Action done, scanning ---");
    }
    return;
  }

  // 2. READ SENSORS
  float distLeft  = getDistance(LEFT_TRIG, LEFT_ECHO);
  float distFront = getDistance(FRONT_TRIG, FRONT_ECHO);
  float distRight = getDistance(RIGHT_TRIG, RIGHT_ECHO);

  // 3. DEBUG OUTPUT
  Serial.print("L:");  Serial.print(distLeft, 0);
  Serial.print(" F:"); Serial.print(distFront, 0);
  Serial.print(" R:"); Serial.print(distRight, 0);

  // 4. MAIN LOGIC: STRAIGHT UNTIL WALL!
  if (distFront > wallThreshold) {
    //  NO WALL = GO STRAIGHT with pivoting based on side sensors
    int speedL = baseSpeedLeft;
    int speedR = baseSpeedRight;
    if (distLeft < closeThreshold) { // wall close on left, pivot right
      speedL -= 10;
      speedR += 10;
    }
    if (distRight < closeThreshold) { // wall close on right, pivot left
      speedL += 10;
      speedR -= 10;
    }
    // clamp speeds
    if (speedL < 0) speedL = 0;
    if (speedR < 0) speedR = 0;
    if (speedL > 255) speedL = 255;
    if (speedR > 255) speedR = 255;
    Serial.println(" → FORWARD");
    executeForward(speedL, speedR);
  } 
  else {
    //  WALL DETECTED = DECISION TIME
    Serial.print(" | WALL!");
    
    // PRIORITY 1: LEFT OPEN? (Professor preference)
    if (distLeft > wallThreshold) {
      Serial.println(" → LEFT");
      startAction(2, turnDuration90);
      executeLeftTurn();
    }
    // PRIORITY 2: RIGHT OPEN?
    else if (distRight > wallThreshold) {
      Serial.println(" → RIGHT");
      startAction(4, turnDuration90);
      executeRightTurn();
    }
    // DEAD END: U-TURN 
    else {
      Serial.println(" → U-TURN");
      startAction(3, turnDuration180);
      executeRightTurn();
    }
  }
  
  delay(30); // Fast sensor loop
}

// ========================================
// CORE FUNCTIONS
// ========================================

void startAction(int stateCode, int duration) {
  robotState = stateCode;
  currentActionDuration = duration;
  actionStartTime = millis();
}

float getDistance(int trigPin, int echoPin) {
  // Improved sensor reading
  digitalWrite(trigPin, LOW);  delayMicroseconds(5);
  digitalWrite(trigPin, HIGH); delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  long duration = pulseIn(echoPin, HIGH, 20000); // 20ms timeout
  if (duration < 100 || duration > 20000) return 999.0;
  
  float distance = duration * 0.034 / 2;
  return (distance > 50) ? 999.0 : distance;
}

void executeForward(int speedL, int speedR) {
  // Straight with adjustable speeds for pivoting
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW); analogWrite(ENA, speedL);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW); analogWrite(ENB, speedR);
}

void executeLeftTurn() {
  // Pivot left (left reverse, right forward)
  digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH); analogWrite(ENA, turnSpeedLeft);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);  analogWrite(ENB, turnSpeedRight);
}

void executeRightTurn() {
  // Pivot right (left forward, right reverse)
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);  analogWrite(ENA, turnSpeedLeft);
  digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH); analogWrite(ENB, turnSpeedRight);
}

void stopMotors() {
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);
}