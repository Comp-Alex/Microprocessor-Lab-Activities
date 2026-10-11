// ====================================================================
// CONFIGURATION: GAMIT ANG IYONG MGA PINS AT BAGONG SPEED VALUES
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

// --- Motor Speed Calibration ---
// Tuned for a compact 22x16cm mazebot in 40x40cm cells
const int BASE_SPEED_LEFT  = 75;  
const int BASE_SPEED_RIGHT = 75;  

// --- Thresholds (Naka-centimeter) ---
const int STUCK_DIST      = 5;     // Sukat kapag sumadsad (Emergency Atras)
const int MIN_WALL_DIST   = 9;     // Masyadong malapit sa kaliwang pader
const int MAX_WALL_DIST   = 13;    // Sakto lang na layo mula sa kaliwa
const int OPEN_WALL_DIST  = 22;    // Lampas dito, itinuturing na "open space"
const int FRONT_WALL_DIST = 12;    // Layo sa harap bago mag-180 turn

// --- Timing para sa Liko ---
// Increase turn durations so a short, compact bot completes rotations reliably
const int TURN_90_TIME  = 350;    
const int TURN_180_TIME = 700;    

// --- DAGDAG PARA SA 5-SECOND TIMER (ITO LANG ANG DINAGDAG SA CONFIG) ---
unsigned long lastMoveTime = 0;
long lastDistSum = 0;
const long STUCK_TIMEOUT = 5000; // 5 Seconds

void setup() {
  Serial.begin(9600);

  // Setup Ultrasonic Pins
  pinMode(trigLeft, OUTPUT);  pinMode(echoLeft, INPUT);
  pinMode(trigFront, OUTPUT); pinMode(echoFront, INPUT);

  // Setup Motor Driver Pins
  pinMode(ENA, OUTPUT); pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(ENB, OUTPUT); pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  
  lastMoveTime = millis(); // Initialize timer
}

void loop() {
  // 1. Basahin ang distansya mula sa sensors
  long distLeft  = getDistance(trigLeft, echoLeft);
  long distFront = getDistance(trigFront, echoFront);

  // Debugging sa Serial Monitor
  Serial.print("Left: "); Serial.print(distLeft);
  Serial.print("cm | Front: "); Serial.println(distFront);

  // --- ANTI-STUCK LOGIC (ITO LANG ANG DINAGDAG SA LOOP) ---
  if (abs((distLeft + distFront) - lastDistSum) > 1) {
    lastMoveTime = millis(); 
    lastDistSum = distLeft + distFront;
  }
  if (millis() - lastMoveTime > STUCK_TIMEOUT) {
    stopMotors();
    delay(200);
    // Atras-kabig pakanan para maka-recover
    analogWrite(ENA, BASE_SPEED_LEFT);
    digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
    analogWrite(ENB, BASE_SPEED_RIGHT - 40);
    digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
    delay(800);
    stopMotors();
    lastMoveTime = millis();
    return;
  }
  // --- END NG DAGDAG ---

  // 2. LOGIC NG LEFT-WALL FOLLOWER (MAY ANTI-STUCK SYSTEM)

  // KASO A: EMERGENCY BACKUP (Harap - Original Code)
  if (distFront < STUCK_DIST && distFront > 0) {
    stopMotors();
    delay(100);
    
    analogWrite(ENA, BASE_SPEED_LEFT);
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH); 
    
    analogWrite(ENB, BASE_SPEED_RIGHT - 20); 
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, HIGH); 
    
    delay(600); 
    stopMotors();
    delay(100);
    return; 
  }

  // NEW CASE: EMERGENCY BACKUP (Left Wall - Iwas Sadsad)
  if (distLeft < STUCK_DIST && distLeft > 0) {
    stopMotors();
    delay(100);
    analogWrite(ENA, BASE_SPEED_LEFT - 30); 
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH); 
    analogWrite(ENB, BASE_SPEED_RIGHT);     
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, HIGH); 
    delay(500); 
    stopMotors();
    delay(100);
    return;
  }

  // KASO B: DEAD END (May pader sa harap)
  if (distFront < FRONT_WALL_DIST && distFront > 0) {
    stopMotors();
    delay(200);
    turn180Right();
    return; 
  }

  // KASO C: BUKAS ANG KALIWA (Open Wall)
  if (distLeft > OPEN_WALL_DIST) {
    moveForward(BASE_SPEED_LEFT, BASE_SPEED_RIGHT); 
    delay(300); 
    turn90Left();
    moveForward(BASE_SPEED_LEFT, BASE_SPEED_RIGHT); 
    delay(500);
    return;
  }

  // KASO D: NORMAL WALL FOLLOWING
  if (distLeft < MIN_WALL_DIST && distLeft > 0) {
    moveForward(BASE_SPEED_LEFT, BASE_SPEED_RIGHT - 40);
  } 
  else if (distLeft > MAX_WALL_DIST) {
    moveForward(BASE_SPEED_LEFT - 40, BASE_SPEED_RIGHT);
  } 
  else {
    moveForward(BASE_SPEED_LEFT, BASE_SPEED_RIGHT);
  }

  delay(50); 
}

// ====================================================================
// MGA FUNCTIONS (WALA AKONG BINAGO DITO)
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
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  analogWrite(ENB, speedRight);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void turn90Left() {
  analogWrite(ENA, BASE_SPEED_LEFT);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH); 
  analogWrite(ENB, BASE_SPEED_RIGHT);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);  
  delay(TURN_90_TIME);
  stopMotors();
}

void turn180Right() {
  analogWrite(ENA, BASE_SPEED_LEFT);
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);  
  analogWrite(ENB, BASE_SPEED_RIGHT);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH); 
  delay(TURN_180_TIME);
  stopMotors();
}

void stopMotors() {
  digitalWrite(IN1, LOW);   digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);   digitalWrite(IN4, LOW);
  analogWrite(ENA, 0);      analogWrite(ENB, 0);
}