// ==========================================================
// ULTRASONIC SENSOR TEST - LEFT, FRONT, RIGHT
// ==========================================================

// Ultrasonic Sensor Pins
const int LEFT_TRIG  = A0;
const int LEFT_ECHO  = A1;
const int FRONT_TRIG = A2;
const int FRONT_ECHO = A3;
const int RIGHT_TRIG = A4;
const int RIGHT_ECHO = A5;

// Threshold (cm)
int wallThreshold = 18;

void setup() {
  Serial.begin(9600);
  Serial.println("=== SENSOR TEST MODE ===");

  // Sensor pins setup
  pinMode(LEFT_TRIG, OUTPUT);  pinMode(LEFT_ECHO, INPUT);
  pinMode(FRONT_TRIG, OUTPUT); pinMode(FRONT_ECHO, INPUT);
  pinMode(RIGHT_TRIG, OUTPUT); pinMode(RIGHT_ECHO, INPUT);
}

void loop() {
  // Read distances
  float distLeft  = getDistance(LEFT_TRIG, LEFT_ECHO);
  float distFront = getDistance(FRONT_TRIG, FRONT_ECHO);
  float distRight = getDistance(RIGHT_TRIG, RIGHT_ECHO);

  // Print results
  Serial.print("Left: ");  Serial.print(distLeft, 0);  Serial.print(" cm");
  Serial.print(" | Front: "); Serial.print(distFront, 0); Serial.print(" cm");
  Serial.print(" | Right: "); Serial.print(distRight, 0); Serial.println(" cm");

  // Simple wall detection
  if (distFront < wallThreshold) Serial.println("⚠️ Wall ahead!");
  if (distLeft  < wallThreshold) Serial.println("⚠️ Wall on left!");
  if (distRight < wallThreshold) Serial.println("⚠️ Wall on right!");

  delay(300); // Slow down printout
}

// ========================================
// SENSOR FUNCTION
// ========================================
float getDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);  delayMicroseconds(5);
  digitalWrite(trigPin, HIGH); delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 20000); // 20ms timeout
  if (duration < 100 || duration > 20000) return 999.0;

  float distance = duration * 0.034 / 2;
  return (distance > 400) ? 999.0 : distance; // cap at 4m
}
