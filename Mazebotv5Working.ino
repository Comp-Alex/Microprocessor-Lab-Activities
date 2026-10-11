#include <Arduino.h>

// ================================================================
//  MAZE BOT — MASTER ENGINEER V5 
//  *Slowed Down, Analyze First, Go Straight Until Blocked*
// ================================================================

// ================= ROBOT DIMENSIONS & PINS =================
#define ENA  5   
#define IN1  4   
#define IN2  3   
#define ENB  6   
#define IN3  7   
#define IN4  8

#define trigFront A2   
#define echoFront A3   
#define trigLeft  A0   
#define echoLeft  A1   
#define trigRight A4   
#define echoRight A5   

// ================= REFINED PID & FILTERING =================
float Kp = 1.6, Ki = 0.003, Kd = 4.2; 
float previousError = 0, integral = 0;

float smoothedLeft = 15.0; 
const float EMA_ALPHA = 0.3; 

// SPEED TYPES FOR DIFFERENT STATES
enum SpeedType {
  SPEED_TYPE_STOP,
  SPEED_TYPE_SLOW,
  SPEED_TYPE_CAUTIOUS,
  SPEED_TYPE_CRUISE,
  SPEED_TYPE_FAST
};

const int SPEED_STOP      = 0;
const int SPEED_SLOW      = 95;   // slightly faster slow
const int SPEED_CAUTIOUS  = 115;  // cautious approach speed
const int SPEED_CRUISE    = 155;  // increased cruising speed
const int SPEED_FAST      = 200;  // higher max speed (use with caution)
const int TURN_SPEED      = 95;   // stronger turn PWM
const int SPEED_MARGIN    = 18;

int   baseSpeed  = SPEED_CRUISE; // default cruising speed
int   motorBias  = 10;  

// ================= SYSTEM LIMITS =================
const int FRONT_EMERGENCY  = 8;    // too close to the wall, must back up
const int FRONT_CLOSE      = 14;   // still near a wall, slow down and prepare
const int FRONT_BLOCKED    = 22;   // wall ahead, time to decide a turn
const int FRONT_WARNING    = 30;   // approaching a wall, reduce speed and stay cautious
const int LEFT_THRESHOLD   = 15;   // target wall-follow distance on the left
const int NO_WALL_DIST     = 30;   // no wall in range on either side
const int MIN_MOTOR_PWM    = 70; 

int distanceFront, distanceLeft, distanceRight; 

// ================ SENSOR TIMING CONFIG ================
// Polling intervals (ms) for each sensor to avoid blocking reads
const unsigned long SENSOR_POLL_MS_FRONT = 40UL; // front sensor faster
const unsigned long SENSOR_POLL_MS_SIDE  = 60UL; // side sensors slightly slower

unsigned long lastSensorRead = 0UL; // last time any sensor was polled
unsigned long lastFrontRead  = 0UL; // last front sensor timestamp
unsigned long lastLeftRead   = 0UL; // last left sensor timestamp
unsigned long lastRightRead  = 0UL; // last right sensor timestamp

// Threshold for considering a reading stale (ms)
const unsigned long SENSOR_STALE_MS = 200UL;

// ================= SERIAL DEBUG (DIAGNOSTICS) =================
const bool DEBUG_SERIAL = true;            // set false to disable diagnostic prints
const unsigned long DIAG_POLL_MS = 250UL; // how often to print diagnostics
unsigned long lastDiagTime = 0UL;

// Track last motor commands for diagnostics
int lastLeftPWMCmd = 0;
int lastRightPWMCmd = 0;
bool lastLeftDir = true;
bool lastRightDir = true;


// Front zone enumeration must be declared before functions that use it
enum FrontZone {
  FRONT_SAFE,
  FRONT_WARNING_ZONE,
  FRONT_BLOCKED_ZONE,
  FRONT_CLOSE_ZONE,
  FRONT_EMERGENCY_ZONE
};

const char* frontZoneName(FrontZone z) {
  switch (z) {
    case FRONT_SAFE: return "SAFE";
    case FRONT_WARNING_ZONE: return "WARN";
    case FRONT_BLOCKED_ZONE: return "BLOCK";
    case FRONT_CLOSE_ZONE: return "CLOSE";
    case FRONT_EMERGENCY_ZONE: return "EMERG";
  }
  return "?";
}

void printDiagnostics() {
  if (!DEBUG_SERIAL) return;
  unsigned long t = millis();
  FrontZone z = classifyFront(distanceFront);
  Serial.print("t="); Serial.print(t);
  Serial.print(" F="); Serial.print(distanceFront);
  Serial.print(" L="); Serial.print(distanceLeft);
  Serial.print(" R="); Serial.print(distanceRight);
  Serial.print(" sL="); Serial.print(smoothedLeft,1);
  Serial.print(" zone="); Serial.print(frontZoneName(z));
  Serial.print(" base="); Serial.print(baseSpeed);
  Serial.print(" cmdL="); Serial.print(lastLeftDir?'+':'-'); Serial.print(lastLeftPWMCmd);
  Serial.print(" cmdR="); Serial.print(lastRightDir?'+':'-'); Serial.print(lastRightPWMCmd);
  Serial.println();
}


// ================= FAST & CLEAN SENSOR READ =================
long getDistance(int trig, int echo) {
  digitalWrite(trig, LOW);
  delayMicroseconds(2);
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW); 
  
  long duration = pulseIn(echo, HIGH, 12000); 
  if (duration == 0) return 200; 
  return duration * 0.034 / 2; 
}

// ================= MOTOR CONTROL =================
void setMotors(bool lFwd, int lPWM, bool rFwd, int rPWM) {
  digitalWrite(IN1, lFwd ? HIGH : LOW);
  digitalWrite(IN2, lFwd ? LOW  : HIGH); 
  digitalWrite(IN3, rFwd ? HIGH : LOW); 
  digitalWrite(IN4, rFwd ? LOW  : HIGH); 
  // record last commanded values for diagnostics
  lastLeftDir = lFwd;
  lastRightDir = rFwd;
  lastLeftPWMCmd = constrain(lPWM, 0, 255);
  lastRightPWMCmd = constrain(rPWM, 0, 255);
  analogWrite(ENA, lastLeftPWMCmd);
  analogWrite(ENB, lastRightPWMCmd);
}

void instantBrake() {
  setMotors(false, 180, false, 180); 
  delay(15); 
  allStop(); 
}

void allStop() {
  analogWrite(ENA, 0); analogWrite(ENB, 0); 
  digitalWrite(IN1, LOW); 
  digitalWrite(IN2, LOW); 
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW); 
}

//  SLOWER TURNS + LONGER DELAYS (To ensure full 90-degree turns)
void moveBackward(int durationMs) { setMotors(false, 100, false, 100); delay(durationMs); allStop(); } 
void turnLeft90()  { setMotors(false, TURN_SPEED, true, TURN_SPEED); delay(360); allStop(); } 
void turnRight90() { setMotors(true, TURN_SPEED, false, TURN_SPEED); delay(360); allStop(); } 

// SENSOR-ACTIVE TURN AROUND
void turnAroundSmart() {
  setMotors(true, speedForType(SPEED_TYPE_CRUISE) - motorBias, false, speedForType(SPEED_TYPE_CRUISE) - motorBias); 
  delay(250); 
  
  long turnStartTime = millis(); 
  while (getDistance(trigFront, echoFront) < 30) { 
    if (millis() - turnStartTime > 1800) break; 
    delay(8); 
  }
  instantBrake(); 
} 

FrontZone classifyFront(int dist) {
  if (dist <= FRONT_EMERGENCY) return FRONT_EMERGENCY_ZONE;
  if (dist <= FRONT_CLOSE)     return FRONT_CLOSE_ZONE;
  if (dist <= FRONT_BLOCKED)   return FRONT_BLOCKED_ZONE;
  if (dist <= FRONT_WARNING)   return FRONT_WARNING_ZONE;
  return FRONT_SAFE;
}

int speedForType(SpeedType type) {
  switch (type) {
    case SPEED_TYPE_STOP:     return SPEED_STOP;
    case SPEED_TYPE_SLOW:     return SPEED_SLOW;
    case SPEED_TYPE_CAUTIOUS: return SPEED_CAUTIOUS;
    case SPEED_TYPE_CRUISE:   return SPEED_CRUISE;
    case SPEED_TYPE_FAST:     return SPEED_FAST;
  }
  return SPEED_CRUISE;
}

bool handleFrontZone(FrontZone zone) {
  switch (zone) {
    case FRONT_SAFE:
      return false; // no special handling needed

    case FRONT_WARNING_ZONE:
      // Approaching a wall: slow down and nudge forward cautiously.
      setMotors(true, speedForType(SPEED_TYPE_CAUTIOUS) - motorBias, true, speedForType(SPEED_TYPE_CAUTIOUS));
      return true;

    case FRONT_BLOCKED_ZONE:
      instantBrake();
      analyzeAndDecide();
      return true;

    case FRONT_CLOSE_ZONE:
        moveBackward(55);
      analyzeAndDecide();
      return true;

    case FRONT_EMERGENCY_ZONE:
        moveBackward(85);
      analyzeAndDecide();
      return true;
  }
  return false;
}

// ================= THE "STOP AND THINK" BRAIN =================
// Only called when the front is blocked!
void analyzeAndDecide() {
  instantBrake();
  delay(150); // short settle

  // Fresh readings
  int checkFront = getDistance(trigFront, echoFront);
  int checkLeft  = getDistance(trigLeft, echoLeft);
  int checkRight = getDistance(trigRight, echoRight);

  // Emergency backup if too close
  if (checkFront <= FRONT_EMERGENCY) {
    moveBackward(170);
    checkFront = getDistance(trigFront, echoFront);
    checkLeft  = getDistance(trigLeft, echoLeft);
    checkRight = getDistance(trigRight, echoRight);
  }

  // Left preference if available
  if (checkLeft > NO_WALL_DIST) {
    turnLeft90();
    return;
  }
  if (checkRight > NO_WALL_DIST) {
    turnRight90();
    return;
  }

  // --- SAFETY: all sides reported blocked. Try a sequence to escape instead of a long blocking wait. ---
  // 1) small reverse and rescan
  moveBackward(100);
  delay(60);
  checkLeft  = getDistance(trigLeft, echoLeft);
  checkRight = getDistance(trigRight, echoRight);
  if (checkLeft > NO_WALL_DIST) { turnLeft90(); return; }
  if (checkRight > NO_WALL_DIST) { turnRight90(); return; }

  // 2) nudge forward slowly to clear sensor occlusion, then rescan
  setMotors(true, speedForType(SPEED_TYPE_SLOW) - motorBias, true, speedForType(SPEED_TYPE_SLOW));
  delay(80);
  allStop();
  checkLeft  = getDistance(trigLeft, echoLeft);
  checkRight = getDistance(trigRight, echoRight);
  if (checkLeft > NO_WALL_DIST) { turnLeft90(); return; }
  if (checkRight > NO_WALL_DIST) { turnRight90(); return; }

  // 3) final attempt: perform a guarded turnaround (turnAroundSmart has its own timeout)
  turnAroundSmart();
}

// ================= PID WALL FOLLOW =================
void pidWallFollow() {
  float error = smoothedLeft - LEFT_THRESHOLD; 
  
  if (abs(error) <= 1.5) { 
    error = 0; 
  }

  integral += error; 
  integral = constrain(integral, -30, 30); 
  
  float derivative = error - previousError; 
  float correction = (Kp * error) + (Ki * integral) + (Kd * derivative); 
  previousError = error; 
  
  int leftSpeed  = baseSpeed - (int)correction - motorBias; 
  int rightSpeed = baseSpeed + (int)correction; 
  
  leftSpeed  = constrain(leftSpeed, MIN_MOTOR_PWM, 230); 
  rightSpeed = constrain(rightSpeed, MIN_MOTOR_PWM, 230); 

  setMotors(true, leftSpeed, true, rightSpeed); 
} 

// ================= SETUP =================
void setup() {
  pinMode(ENA, OUTPUT); pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT); 
  pinMode(ENB, OUTPUT); pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT); 
  pinMode(trigFront, OUTPUT); pinMode(echoFront, INPUT); 
  pinMode(trigLeft,  OUTPUT); pinMode(echoLeft,  INPUT); 
  pinMode(trigRight, OUTPUT); pinMode(echoRight, INPUT); 

  Serial.begin(9600); 
  allStop(); 
  delay(1000); 
} 

// ================= MAIN LOOP =================
void loop() {
  // 1. SCHEDULED SENSOR POLLING (non-blocking)
  unsigned long now = millis();

  // Front sensor (polled more frequently)
  if (now - lastFrontRead >= SENSOR_POLL_MS_FRONT) {
    distanceFront = getDistance(trigFront, echoFront);
    lastFrontRead = now;
  }

  // Left sensor (for wall-following & smoothing)
  if (now - lastLeftRead >= SENSOR_POLL_MS_SIDE) {
    int rawLeft = getDistance(trigLeft, echoLeft);
    if (rawLeft > 0 && rawLeft < 80) {
      smoothedLeft = (EMA_ALPHA * rawLeft) + ((1.0 - EMA_ALPHA) * smoothedLeft);
    }
    distanceLeft = (int)smoothedLeft;
    lastLeftRead = now;
  }

  // Right sensor
  if (now - lastRightRead >= SENSOR_POLL_MS_SIDE) {
    distanceRight = getDistance(trigRight, echoRight);
    lastRightRead = now;
  }

  // update lastSensorRead for diagnostics
  lastSensorRead = now;

  // ================= SENSOR STALENESS HANDLING =================
  // If the front sensor hasn't been updated recently, slow to safe speed
  if (now - lastFrontRead > SENSOR_STALE_MS) {
    Serial.println(" Front sensor stale; slowing down");
    setMotors(true, speedForType(SPEED_TYPE_SLOW) - motorBias, true, speedForType(SPEED_TYPE_SLOW));
    return; // wait until a fresh reading arrives
  }

  // Periodic diagnostics print
  if (DEBUG_SERIAL && (now - lastDiagTime >= DIAG_POLL_MS)) {
    printDiagnostics();
    lastDiagTime = now;
  }

  //  2. HANDLE MULTI-STAGE FRONT THRESHOLDS
  if (handleFrontZone(classifyFront(distanceFront))) {
    return; // Either turned, slowed, or re-decided already
  }

  //  3. IF FRONT IS CLEAR -> GO STRAIGHT!
  // If there is a left wall nearby, use PID to smoothly follow it.
  if (distanceLeft <= NO_WALL_DIST) { 
    pidWallFollow(); 
  } 
  // If there are no walls at all (wide open space), just drive perfectly straight.
  else {
    setMotors(true, speedForType(SPEED_TYPE_CRUISE) - motorBias, true, speedForType(SPEED_TYPE_CRUISE)); 
  }
}