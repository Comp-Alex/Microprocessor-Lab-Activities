// ====================================================================
// CONFIGURATION: GAMIT ANG MONG MGA PINS
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

// --- Motor Speed Calibration (I-adjust ito para tumakbo nang STRAIGHT) ---
const int BASE_SPEED_LEFT  = 100;  // Bilis ng kaliwang motor (0-255)
const int BASE_SPEED_RIGHT = 100;  // Bilis ng kanang motor (0-255)
// Paalala: Kung kumakabig pakanan, taasan ang kaliwa o babaan ang kanan.

// --- Thresholds (Naka-centimeter) ---
const int MIN_WALL_DIST   = 12;   // Masyadong malapit sa kaliwang pader
const int MAX_WALL_DIST   = 24;   // Sakto lang na layo mula sa kaliwa (INCREASED)
const int OPEN_WALL_DIST  = 35;   // Lampas dito, itinuturing na "open space" (kanto)
const int FRONT_WALL_DIST = 15;   // Layo sa harap bago sabihing "Dead End"

// --- Timing para sa Liko (I-adjust base sa baterya ng robot mo) ---
const int TURN_90_TIME  = 380;    // Oras (ms) para mag-90 degrees turn pakaliwa (REDUCED)
const int TURN_180_TIME = 720;   // Oras (ms) para mag-180 degrees turn (U-Turn) (REDUCED)
const int TURN_SPEED    = 80;    // Bilis during turns (REDUCED para avoid wobbling)

void setup() {
  Serial.begin(9600);

  // Setup Ultrasonic Pins
  pinMode(trigLeft, OUTPUT);  pinMode(echoLeft, INPUT);
  pinMode(trigFront, OUTPUT); pinMode(echoFront, INPUT);

  // Setup Motor Driver Pins
  pinMode(ENA, OUTPUT); pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(ENB, OUTPUT); pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
}

void loop() {
  // 1. Basahin ang distansya mula sa sensors
  long distLeft  = getDistance(trigLeft, echoLeft);
  long distFront = getDistance(trigFront, echoFront);

  // Debugging sa Serial Monitor
  Serial.print("Left: "); Serial.print(distLeft);
  Serial.print("cm | Front: "); Serial.println(distFront);

  // 2. LOGIC NG LEFT-WALL FOLLOWER

  // KASO A: DEAD END (May pader sa harap) -> Mag-180 degrees turn pakanan
  if (distFront < FRONT_WALL_DIST && distFront > 0) {
    stopMotors();
    delay(200);
    turn180Right();
    return; 
  }

  // KASO B: BUKAS ANG KALIWA (Open Wall) -> Liko 90 degrees pakaliwa
  if (distLeft > OPEN_WALL_DIST) {
    moveForward(BASE_SPEED_LEFT, BASE_SPEED_RIGHT); // Abante saglit para lumampas sa kanto
    delay(250); 
    turn90Left();
    moveForward(BASE_SPEED_LEFT, BASE_SPEED_RIGHT); // Pasok sa bagong lane
    delay(400);
    return;
  }

  // KASO C: NORMAL WALL FOLLOWING (Pagpapanatili ng tuwid na takbo)
  if (distLeft < MIN_WALL_DIST && distLeft > 0) {
    // Masyadong malapit sa kaliwa -> Liko pakanan ng bahagya (Bawasan ang kanang motor)
    moveForward(BASE_SPEED_LEFT, BASE_SPEED_RIGHT - 50);
  } 
  else if (distLeft > MAX_WALL_DIST) {
    // Masyadong malayo sa kaliwa -> Liko pakaliwa ng bahagya (Bawasan ang kaliwang motor)
    moveForward(BASE_SPEED_LEFT - 50, BASE_SPEED_RIGHT);
  } 
  else {
    // Sakto lang ang distansya -> Deretso at tuwid ang takbo
    moveForward(BASE_SPEED_LEFT, BASE_SPEED_RIGHT);
  }

  delay(30); // Maikling delay para sa katatagan ng sensor readings
}

// ====================================================================
// MGA FUNCTIONS PARA SA SENSORS AT MOTOR MOVEMENT
// ====================================================================

// Function para sa pagbasa ng HC-SR04 Ultrasonic Sensor
long getDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  long duration = pulseIn(echoPin, HIGH, 25000); // 25ms timeout
  long distance = duration * 0.034 / 2;
  
  if (distance == 0) return 999; // Kung walang basang bounce, ituring na walang pader
  return distance;
}

// Abante (May kontrol sa bilis ng bawat motor para sa tuwid na takbo)
void moveForward(int speedLeft, int speedRight) {
  analogWrite(ENA, speedLeft);
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  
  analogWrite(ENB, speedRight);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

// Sharp 90 Degrees Turn Pakaliwa (In place turn)
void turn90Left() {
  analogWrite(ENA, TURN_SPEED);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH); // Kaliwang motor paatras
  
  analogWrite(ENB, TURN_SPEED);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);  // Kanang motor paabante
  
  delay(TURN_90_TIME);
  stopMotors();
}

// Sharp 180 Degrees U-Turn Pakanan (In place turn para sa Dead End)
void turn180Right() {
  analogWrite(ENA, TURN_SPEED);
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);  // Kaliwang motor paabante
  
  analogWrite(ENB, TURN_SPEED);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH); // Kanang motor paatras
  
  delay(TURN_180_TIME);
  stopMotors();
}

// Hinto
void stopMotors() {
  digitalWrite(IN1, LOW);   digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);   digitalWrite(IN4, LOW);
  analogWrite(ENA, 0);      analogWrite(ENB, 0);
}