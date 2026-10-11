// ============================================================
//  Mazebot — Pre-Planned Path + Left Wall Following
//  Maze: 30x30 cm  |  ~6 cells wide, ~6 cm per cell
//
//  White path from image (top-left to bottom-right):
//  FORWARD → RIGHT → FORWARD → FORWARD →
//  LEFT → FORWARD → RIGHT → FORWARD → FORWARD →
//  LEFT → FORWARD → RIGHT → FORWARD → STOP
//
//  Sensors:
//    Ultrasonics  → measure wall distances (front/left/right)
//    IR sensors   → confirm side walls at close range
// ============================================================

// --- Motor Driver (L298N) ---
#define ENA  5
#define IN1  4
#define IN2  3
#define ENB  6
#define IN3  7
#define IN4  8

// --- Ultrasonic Sensors ---
#define trigFront A2
#define echoFront A3
#define trigLeft  A0
#define echoLeft  A1
#define trigRight A4
#define echoRight A5

// --- IR Side Wall Sensors (LOW = wall detected) ---
#define irLeftPin  2
#define irRightPin 9

// ============================================================
//  Tunable Parameters — adjust for your robot
// ============================================================
const int  BASE_SPEED     = 150;   // forward speed (0–255)
const int  TURN_SPEED     = 130;   // in-place turn speed
const int  WALL_DIST_CM   = 5;     // treat as "wall present" if closer
const int  FRONT_STOP_CM  = 4;     // stop before front wall
const long CELL_TRAVEL_MS = 500;   // ms to cross one ~6cm cell
const int  TURN_90_MS     = 400;   // ms for a 90° in-place turn
                                   // ↑ calibrate on flat surface first

// ============================================================
//  Pre-Planned Move Sequence  (traced from white path in image)
//  Adjust TURN_90_MS until turns are exactly 90° on your robot
// ============================================================
enum Move { FWD, TURN_L, TURN_R, DONE };

const Move PATH[] = {
  FWD,     // enter maze, go forward
  FWD,
  TURN_R,  // turn right at first junction
  FWD,
  FWD,
  FWD,
  TURN_L,  // turn left mid-section
  FWD,
  FWD,
  TURN_R,  // turn right
  FWD,
  FWD,
  TURN_L,  // turn left
  FWD,
  FWD,
  TURN_R,  // final right turn to exit
  FWD,
  FWD,
  DONE
};

const int PATH_LEN = sizeof(PATH) / sizeof(PATH[0]);

// ============================================================
//  Setup
// ============================================================
void setup() {
  pinMode(ENA, OUTPUT); pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(ENB, OUTPUT); pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);

  pinMode(trigFront, OUTPUT); pinMode(echoFront, INPUT);
  pinMode(trigLeft,  OUTPUT); pinMode(echoLeft,  INPUT);
  pinMode(trigRight, OUTPUT); pinMode(echoRight, INPUT);

  pinMode(irLeftPin,  INPUT);
  pinMode(irRightPin, INPUT);

  stopMotors();
  Serial.begin(9600);
  delay(2000);  // 2s pause — place robot, then it starts
  Serial.println("Mazebot starting...");
}

// ============================================================
//  Main Loop — execute pre-planned path step by step
// ============================================================
int step = 0;

void loop() {
  if (step >= PATH_LEN) {
    stopMotors();
    return;
  }

  Move m = PATH[step];

  switch (m) {

    case FWD:
      Serial.print("Step "); Serial.print(step); Serial.println(": FORWARD");
      driveCell();
      break;

    case TURN_L:
      Serial.print("Step "); Serial.print(step); Serial.println(": TURN LEFT");
      stopMotors();
      delay(100);
      turnLeft90();
      delay(100);
      break;

    case TURN_R:
      Serial.print("Step "); Serial.print(step); Serial.println(": TURN RIGHT");
      stopMotors();
      delay(100);
      turnRight90();
      delay(100);
      break;

    case DONE:
      Serial.println("MAZE COMPLETE!");
      stopMotors();
      // Victory blink — optional
      for (int i = 0; i < 5; i++) {
        moveForward(180); delay(100);
        stopMotors();     delay(100);
      }
      step++;  // prevent re-running
      return;
  }

  step++;
  delay(80);  // short pause between moves
}

// ============================================================
//  Drive One Cell (~6 cm)
//  Uses front ultrasonic to stop early if wall is too close,
//  and IR/side ultrasonics to keep the robot centered.
// ============================================================
void driveCell() {
  long start = millis();

  while (millis() - start < CELL_TRAVEL_MS) {
    int distF = getDistance(trigFront, echoFront);
    int distL = getDistance(trigLeft,  echoLeft);
    int distR = getDistance(trigRight, echoRight);
    bool wallL = (distL > 0 && distL < WALL_DIST_CM) || (digitalRead(irLeftPin)  == LOW);
    bool wallR = (distR > 0 && distR < WALL_DIST_CM) || (digitalRead(irRightPin) == LOW);

    // Emergency stop — wall right in front
    if (distF > 0 && distF < FRONT_STOP_CM) {
      Serial.println("  [!] Front wall — stopping early");
      break;
    }

    // Wall correction — steer away from close walls
    if (wallL && !wallR) {
      // Too close to left wall — nudge right
      setMotors(BASE_SPEED + 15, BASE_SPEED - 15);
    } else if (wallR && !wallL) {
      // Too close to right wall — nudge left
      setMotors(BASE_SPEED - 15, BASE_SPEED + 15);
    } else {
      // Centered — go straight
      moveForward(BASE_SPEED);
    }

    delay(10);
  }

  stopMotors();
}

// ============================================================
//  Ultrasonic Distance in cm (returns 0 on timeout)
// ============================================================
long getDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long dur = pulseIn(echoPin, HIGH, 20000); // 20ms timeout
  if (dur == 0) return 0;
  return dur * 0.034 / 2;
}

// ============================================================
//  Motor Primitives
// ============================================================
void setMotors(int leftSpd, int rightSpd) {
  // Left motor
  if (leftSpd >= 0) { digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW); }
  else              { digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH); leftSpd = -leftSpd; }
  analogWrite(ENA, constrain(leftSpd, 0, 255));

  // Right motor
  if (rightSpd >= 0) { digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW); }
  else               { digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH); rightSpd = -rightSpd; }
  analogWrite(ENB, constrain(rightSpd, 0, 255));
}

void moveForward(int spd)  { setMotors(spd, spd);   }
void moveBackward(int spd) { setMotors(-spd, -spd); }
void spinLeft(int spd)     { setMotors(-spd, spd);  }
void spinRight(int spd)    { setMotors(spd, -spd);  }

void stopMotors() {
  analogWrite(ENA, 0); analogWrite(ENB, 0);
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
}

void turnLeft90() {
  spinLeft(TURN_SPEED);
  delay(TURN_90_MS);
  stopMotors();
}

void turnRight90() {
  spinRight(TURN_SPEED);
  delay(TURN_90_MS);
  stopMotors();
}
