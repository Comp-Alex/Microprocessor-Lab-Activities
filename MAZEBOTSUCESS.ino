// ====================================================================
// CONFIGURATION: GAMIT ANG IYONG MGA PINS AT ADJUSTED VALUES
// ====================================================================

// --- Motor Pins (L298N) ---
#define ENA  5  // PWM Pin para sa bilis ng Kaliwang Motor
#define IN1  4  // Direksyon ng Kaliwang Motor
#define IN2  3  // Direksyon ng Kaliwang Motor
#define ENB  6  // PWM Pin para sa bilis ng Kanang Motor
#define IN3  7  // Direksyon ng Kanang Motor
#define IN4  8  // Direksyon ng Kanang Motor

// --- Ultrasonic Pins ---
#define trigLeft  A0
#define echoLeft  A1
#define trigFront A2
#define echoFront A3

// --- Motor Speed Calibration (IBINABA PARA HINDI SOBRA ANG LAKAS) ---
const int BASE_SPEED_LEFT  = 110;  // Mula 120, ginawang 100 para mas kontrolado
const int BASE_SPEED_RIGHT = 110;  // Mula 120, ginawang 100 para mas kontrolado

// --- Thresholds (Naka-centimeter) ---
const int STUCK_DIST      = 6;     // Sukat kapag sumadsad (Emergency Atras)
const int MIN_WALL_DIST   = 15;    // Masyadong malapit sa kaliwang pader
const int MAX_WALL_DIST   = 25;    // Sakto lang na layo mula sa kaliwa
const int OPEN_WALL_DIST  = 35;    // Lampas dito, itinuturing na "open space"
const int FRONT_WALL_DIST = 15;    // Layo sa harap bago mag-180 turn

// --- Timing para sa Liko (BINAWASAN PARA HINDI SOBRA ANG LIKO) ---
const int TURN_90_TIME  = 120;    // Mula 200, binawasan para saktong 90 degrees
const int TURN_180_TIME = 220;    // Mula 300, binawasan para saktong 180 degrees

// --- ANTI-IDLE / STUCK VARIABLES ---
unsigned long lastMoveTime = 0;
long lastDistSum = 0;
const long STUCK_TIMEOUT = 5000;   // 5 Seconds Anti-Stuck

void setup() {
  Serial.begin(9600);

  // Setup Ultrasonic Pins
  pinMode(trigLeft, OUTPUT);  pinMode(echoLeft, INPUT);
  pinMode(trigFront, OUTPUT); pinMode(echoFront, INPUT);

  // Setup Motor Driver Pins
  pinMode(ENA, OUTPUT); pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(ENB, OUTPUT); pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  
  lastMoveTime = millis(); 
}

void loop() {
  // 1. Basahin ang distansya mula sa sensors
  long distLeft  = getDistance(trigLeft, echoLeft);
  long distFront = getDistance(trigFront, echoFront);

  // Debugging sa Serial Monitor
  Serial.print("Left: "); Serial.print(distLeft);
  Serial.print("cm | Front: "); Serial.println(distFront);

  // --- ANTI-STUCK LOGIC (TIMER CHECK) ---
  if (abs((distLeft + distFront) - lastDistSum) > 1) {
    lastMoveTime = millis(); 
    lastDistSum = distLeft + distFront;
  }
  if (millis() - lastMoveTime > STUCK_TIMEOUT) {
    stopMotors();
    delay(200);
    // Emergency Atras-Kabig
    analogWrite(ENA, BASE_SPEED_LEFT);
    digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
    analogWrite(ENB, BASE_SPEED_RIGHT - 40);
    digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
    delay(600);
    stopMotors();
    lastMoveTime = millis();
    return;
  }

  // 2. LOGIC NG LEFT-WALL FOLLOWER

  // KASO A: EMERGENCY BACKUP (Harap)
  if (distFront < STUCK_DIST && distFront > 0) {
    stopMotors();
    delay(100);
    analogWrite(ENA, BASE_SPEED_LEFT);
    digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH); 
    analogWrite(ENB, BASE_SPEED_RIGHT - 20); 
    digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH); 
    delay(500); 
    stopMotors();
    delay(100);
    return; 
  }

  // KASO A.1: EMERGENCY BACKUP (Dikit sa Kaliwang Pader)
  if (distLeft < STUCK_DIST && distLeft > 0) {
    stopMotors();
    delay(100);
    analogWrite(ENA, BASE_SPEED_LEFT - 30); 
    digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH); 
    analogWrite(ENB, BASE_SPEED_RIGHT);     
    digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH); 
    delay(450); 
    stopMotors();
    delay(100);
    return;
  }

  // KASO B: DEAD END
  if (distFront < FRONT_WALL_DIST && distFront > 0) {
    stopMotors();
    delay(200);
    turn180Right();
    return; 
  }

  // KASO C: BUKAS ANG KALIWA
  if (distLeft > OPEN_WALL_DIST) {
    moveForward(BASE_SPEED_LEFT, BASE_SPEED_RIGHT); 
    delay(250); 
    turn90Left();
    moveForward(BASE_SPEED_LEFT, BASE_SPEED_RIGHT); 
    delay(400);
    return;
  }

  // KASO D: NORMAL WALL FOLLOWING (Correction)
  if (distLeft < MIN_WALL_DIST && distLeft > 0) {
    moveForward(BASE_SPEED_LEFT, BASE_SPEED_RIGHT - 35);
  } 
  else if (distLeft > MAX_WALL_DIST) {
    moveForward(BASE_SPEED_LEFT - 35, BASE_SPEED_RIGHT);
  } 
  else {
    moveForward(BASE_SPEED_LEFT, BASE_SPEED_RIGHT);
  }

  delay(30); 
}

// ====================================================================
// MGA FUNCTIONS PARA SA SENSORS AT MOTOR MOVEMENT (NO LOGIC CHANGES)
// ====================================================================

long getDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH, 25000); 
  long distance = duration * 0.034 / 2;
  if (distance <= 0) return 999; 
  return distance;
}

void moveForward(int speedLeft, int speedRight) {
  analogWrite(ENA, speedLeft);
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  analogWrite(ENB, speedRight);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
}

void turn90Left() {
  analogWrite(ENA, BASE_SPEED_LEFT);
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH); 
  analogWrite(ENB, BASE_SPEED_RIGHT);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);  
  delay(TURN_90_TIME);
  stopMotors();
}

void turn180Right() {
  analogWrite(ENA, BASE_SPEED_LEFT);
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);  
  analogWrite(ENB, BASE_SPEED_RIGHT);
  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH); 
  delay(TURN_180_TIME);
  stopMotors();
}

void stopMotors() {
  digitalWrite(IN1, LOW);   digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);   digitalWrite(IN4, LOW);
  analogWrite(ENA, 0);      analogWrite(ENB, 0);
}