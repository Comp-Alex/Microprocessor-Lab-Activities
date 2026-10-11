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

#define trigFront A2;
#define echoFront A3   
#define trigLeft  A0;
#define echoLeft  A1   
#define trigRight A4   
#define echoRight A5   

// ================= REFINED PID & FILTERING =================
float Kp = 1.6, Ki = 0.003, Kd = 4.2; 
float previousError = 0, integral = 0;

float smoothedLeft = 15.0; 
const float EMA_ALPHA = 0.3; 

// 🔥 SLOWER SPEEDS FOR BETTER CONTROL
int   baseSpeed  = 120; // Lowered for calmer cruising
int   motorBias  = 10;  

// ================= SYSTEM LIMITS =================
const int FRONT_EMERGENCY  = 8; 
const int FRONT_THRESHOLD  = 22;  
const int LEFT_THRESHOLD   = 15;  
const int NO_WALL_DIST     = 30;  
const int MIN_MOTOR_PWM    = 70; 

int distanceFront, distanceLeft, distanceRight; 

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
  analogWrite(ENA, constrain(lPWM, 0, 255)); 
  analogWrite(ENB, constrain(rPWM, 0, 255)); 
}

void instantBrake() {
  setMotors(false, 180, false, 180); 
  delay(35); 
  allStop(); 
}

void allStop() {
  analogWrite(ENA, 0); analogWrite(ENB, 0); 
  digitalWrite(IN1, LOW); 
  digitalWrite(IN2, LOW); 
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW); 
}

// 🔥 SLOWER TURNS + LONGER DELAYS (To ensure full 90-degree turns)
void moveBackward(int durationMs) { setMotors(false, 80, false, 80); delay(durationMs); allStop(); } 
void turnLeft90()  { setMotors(false, 75, true, 75); delay(480); allStop(); } 
void turnRight90() { setMotors(true, 75, false, 75); delay(480); allStop(); } 

// SENSOR-ACTIVE TURN AROUND
void turnAroundSmart() {
  setMotors(true, 80, false, 80); 
  delay(400); 
  
  long turnStartTime = millis(); 
  while (getDistance(trigFront, echoFront) < 30) { 
    if (millis() - turnStartTime > 2500) break; 
    delay(10); 
  }
  instantBrake(); 
} 

// ================= THE "STOP AND THINK" BRAIN =================
// Only called when the front is blocked!
void analyzeAndDecide() {
  instantBrake();
  delay(300); // Pause to let momentum settle

  // Take fresh, highly accurate readings while standing still
  int checkFront = getDistance(trigFront, echoFront);
  int checkLeft  = getDistance(trigLeft, echoLeft);
  int checkRight = getDistance(trigRight, echoRight);

  // EMERGENCY BACKUP: If it stopped too close to the front wall
  if (checkFront <= FRONT_EMERGENCY) {
    moveBackward(250);
    checkFront = getDistance(trigFront, echoFront); 
  }

  // 🧠 TURN DECISION TREE (Left-Hand Priority)
  if (checkLeft > NO_WALL_DIST) {
    turnLeft90();
  } 
  else if (checkRight > NO_WALL_DIST) {
    turnRight90();
  } 
  else {
    turnAroundSmart();
  }
}

// ================= PID WALL   FOLLOW =================
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
  
  leftSpeed  = constrain(leftSpeed, MIN_MOTOR_PWM, 200); 
  rightSpeed = constrain(rightSpeed, MIN_MOTOR_PWM, 200); 

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
  delay(2000); 
} 

// ================= MAIN LOOP =================
void loop() {
  // 1. SCAN SENSORS
  distanceFront = getDistance(trigFront, echoFront);
  int rawLeft   = getDistance(trigLeft, echoLeft);
  distanceRight = getDistance(trigRight, echoRight);

  if (rawLeft > 0 && rawLeft < 80) { 
    smoothedLeft = (EMA_ALPHA * rawLeft) + ((1.0 - EMA_ALPHA) * smoothedLeft); 
  } 
  distanceLeft = (int)smoothedLeft; 

  // 🛑 2. IS THE FRONT BLOCKED? 
  if (distanceFront < FRONT_THRESHOLD) { 
    analyzeAndDecide(); // Stop and figure out where to turn
    return; // Restart the loop after turning
  }

  // 🟢 3. IF FRONT IS CLEAR -> GO STRAIGHT!
  // If there is a left wall nearby, use PID to smoothly follow it.
  if (distanceLeft <= NO_WALL_DIST) { 
    pidWallFollow(); 
  } 
  // If there are no walls at all (wide open space), just drive perfectly straight.
  else {
    setMotors(true, baseSpeed - motorBias, true, baseSpeed); 
  }
}