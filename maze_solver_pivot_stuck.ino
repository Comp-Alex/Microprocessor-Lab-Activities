// ==========================================================
// MAZE SOLVER ROBOT - LEFT PREFERRED (Professor Optimized)
// STRAIGHT FORWARD until WALL → LEFT priority turns
// Active corridor centering + stop-before-turn
// Motor Driver: L298N
//
// GEOMETRY:
//   Left/Right sensors at ~30° outward from front corners.
//   Front sensor straight ahead at center.
//   Caster wheel at REAR → pivot near wheel axle (center).
//   Bot: 25cm long × 12.5cm wide. Maze cell: 40cm × 40cm.
//
//   cos(30°) = 0.866 → angled reading × 0.866 = true lateral gap
//   Both sensors share the same angle → raw L-R error for centering
//   is valid as-is (cos factor cancels). Lateral clearance decisions
//   use the trig-corrected value.
//
// NEW in this version:
//   PIVOT TURNS  — instead of a dead stop + spin, the bot does a
//                  gentle arc (one wheel slower, other faster) for
//                  90° turns when space allows. Feels smoother and
//                  stays better centered in the cell.
//   STUCK ESCAPE — detects being stuck (no meaningful forward
//                  progress while trying to move) and executes a
//                  multi-step escape: reverse → wiggle → re-scan.
// ==========================================================

// ── Motor Driver Pins (L298N) ──────────────────────────────
const int ENA = 5;
const int IN1 = 4;
const int IN2 = 3;
const int ENB = 6;
const int IN3 = 7;
const int IN4 = 8;

// ── Ultrasonic Sensor Pins ─────────────────────────────────
const int LEFT_TRIG  = A0;
const int LEFT_ECHO  = A1;
const int FRONT_TRIG = A2;
const int FRONT_ECHO = A3;
const int RIGHT_TRIG = A4;
const int RIGHT_ECHO = A5;

// ── Geometry ───────────────────────────────────────────────
constexpr float COS30 = 0.866f;

// ── TUNED PARAMETERS ───────────────────────────────────────
const int baseSpeedLeft   = 100;
const int baseSpeedRight  = 100;
const int turnSpeedLeft   = 120;
const int turnSpeedRight  = 120;

const int wallThreshold      = 15;   // cm front wall detect
const int closeThreshold     = 10;   // cm raw angled: side wall close
const int veryCloseThreshold = 7;    // cm raw angled: too close → adjust
const int openTurnThreshold  = 18;   // cm raw angled: full cell open that side

const int turnDuration90   = 300;   // ms full pivot 90°
const int turnDuration180  = 600;   // ms full pivot 180°
const int shortStopDuration    = 150;
const int shortReverseDuration = 250;
const int smallAdjustDuration  = 100;
const int preTurnStopDuration  = 100;

// ── Pivot-arc parameters ───────────────────────────────────
constexpr float innerWheelRatio   = 0.30f;  // 0.0–0.5 (tune on floor)
const int   pivotArcDuration  = 375;   // ms for 90° arc (tune: turnDuration90 × 1.25)
const int   pivotArc180Duration = 750; // ms for 180° arc

// Whether to use arc turns or full pivot spins.
// Set to false if arcs cause the bot to clip walls in tight corridors.
const bool usePivotArc = true;

// ── Centering ──────────────────────────────────────────────
const float centerGain    = 1.8;
const int   maxCorrection = 12;
const int   minSpeed      = 60;

// ── Stuck detection ───────────────────────────────────────
// The bot is "stuck" if it has been commanding FORWARD for
// stuckTimeLimit ms but the front sensor reading has not
// decreased (it hasn't moved closer to anything — motor stall,
// wheel slip, wedged against wall, etc.).
//
// We track the front distance at the moment forward starts.
// If distFront doesn't change by more than stuckMovementThreshold
// after stuckTimeLimit ms → trigger escape sequence.
//
// Escape sequence:
//   Step 1: reverse stuckReverseDuration ms (pull away)
//   Step 2: wiggle — short left arc then right arc (unstick wheels)
//   Step 3: return to scan (re-read sensors, pick new direction)
const unsigned long stuckTimeLimit        = 800;   // ms with no progress → stuck
const float         stuckMovementThreshold = 2.0;  // cm change needed to prove moving
const int           stuckReverseDuration  = 400;   // ms reverse during escape
const int           stuckWiggleDuration   = 180;   // ms each wiggle direction

// Stuck tracking variables
unsigned long forwardStartTime  = 0;
float         forwardStartFront = 999.0;
bool          trackingForward   = false;

// ── State Machine ──────────────────────────────────────────
enum RobotState {
  SCAN,
  FORWARD,
  LEFT90,
  UTURN,
  RIGHT90,
  STOP,
  REVERSE,
  ADJUST_LEFT,
  ADJUST_RIGHT,
  PRE_TURN_STOP,
  STUCK_REVERSE,
  STUCK_WIGGLE_L,
  STUCK_WIGGLE_R
};

RobotState robotState           = SCAN;
unsigned long actionStartTime  = 0;
int  currentActionDuration     = 0;
bool wallDetected  = false;
bool justReversed  = false;
RobotState intendedTurn  = UTURN;

// ==========================================================
void setup() {
  Serial.begin(9600);
  Serial.println("=== MAZE BOT - PIVOT ARC + STUCK ESCAPE ===");
  Serial.println("Angled sensors (30deg) + rear caster geometry.");
  Serial.println("usePivotArc=" + String(usePivotArc)
               + " innerWheelRatio=" + String(innerWheelRatio));
  Serial.println("Logic: FORWARD → STOP → REVERSE → PRE-STOP → ARC/SPIN TURN");

  pinMode(ENA,OUTPUT); pinMode(IN1,OUTPUT); pinMode(IN2,OUTPUT);
  pinMode(ENB,OUTPUT); pinMode(IN3,OUTPUT); pinMode(IN4,OUTPUT);
  pinMode(LEFT_TRIG,OUTPUT);  pinMode(LEFT_ECHO,INPUT);
  pinMode(FRONT_TRIG,OUTPUT); pinMode(FRONT_ECHO,INPUT);
  pinMode(RIGHT_TRIG,OUTPUT); pinMode(RIGHT_ECHO,INPUT);

  stopMotors();
  delay(1000);
  Serial.println(">>> READY! Place in maze...");
  Serial.println("Tune: turnDuration90=" + String(turnDuration90)
               + " pivotArcDuration=" + String(pivotArcDuration));
}

// ==========================================================
void loop() {

  // ── 1. STATE MACHINE ──────────────────────────────────────
  if (robotState != SCAN) {
    if (millis() - actionStartTime >= (unsigned long)currentActionDuration) {
      stopMotors();

      if (robotState == STOP && wallDetected) {
        // Wall stop done → reverse
        startAction(REVERSE, shortReverseDuration);
        executeShortReverse();
        wallDetected = false;
        justReversed = true;

      } else if (robotState == PRE_TURN_STOP) {
        // Pre-turn settle done → execute arc or spin
        int dur = (intendedTurn == UTURN)
                  ? (usePivotArc ? pivotArc180Duration : turnDuration180)
                  : (usePivotArc ? pivotArcDuration    : turnDuration90);
        startAction(intendedTurn, dur);
        if      (intendedTurn == LEFT90) executeLeftTurn();
        else if (intendedTurn == RIGHT90) executeRightTurn();
        else                             executeRightTurn();  // u-turn spins right

      } else if (robotState == STUCK_REVERSE) {
        // Stuck reverse done → wiggle left
        startAction(STUCK_WIGGLE_L, stuckWiggleDuration);
        executeWiggleLeft();

      } else if (robotState == STUCK_WIGGLE_L) {
        // Wiggle left done → wiggle right
        startAction(STUCK_WIGGLE_R, stuckWiggleDuration);
        executeWiggleRight();

      } else if (robotState == STUCK_WIGGLE_R) {
        // Wiggle right done → back to scan
        robotState = SCAN;
        trackingForward = false;
        Serial.println("--- Stuck escape complete, re-scanning ---");

      } else {
        robotState = SCAN;
        trackingForward = false;
        Serial.println("--- Action done, scanning ---");
      }
    }
    return;
  }

  // ── 2. READ SENSORS ───────────────────────────────────────
  float distLeft  = getDistance(LEFT_TRIG,  LEFT_ECHO);
  float distFront = getDistance(FRONT_TRIG, FRONT_ECHO);
  float distRight = getDistance(RIGHT_TRIG, RIGHT_ECHO);

  float latLeft  = distLeft  * COS30;
  float latRight = distRight * COS30;

  // ── 3. DEBUG ──────────────────────────────────────────────
  Serial.print("L:"); Serial.print(distLeft,0);
  Serial.print(" F:"); Serial.print(distFront,0);
  Serial.print(" R:"); Serial.print(distRight,0);
  Serial.print(" | Ll:"); Serial.print(latLeft,1);
  Serial.print(" Rl:"); Serial.print(latRight,1);

  // ── 4. AFTER REVERSE: decide turn direction ───────────────
  if (justReversed) {
    justReversed = false;
    trackingForward = false;
    Serial.print(" | AFTER REVERSE:");
    if      (distLeft  > openTurnThreshold) { intendedTurn = LEFT90; Serial.println(" → LEFT");   }
    else if (distRight > openTurnThreshold) { intendedTurn = RIGHT90; Serial.println(" → RIGHT");  }
    else                                    { intendedTurn = UTURN; Serial.println(" → U-TURN"); }
    startAction(PRE_TURN_STOP, preTurnStopDuration);
    stopMotors();
    return;
  }

  // ── 5. PROACTIVE LEFT TURN (angled sensor advantage) ──────
  // Sensors see junctions before the bot reaches them.
  if (distFront > wallThreshold + 6       &&
      distLeft  > openTurnThreshold * 1.4 &&
      distRight < 40) {
    Serial.println(" | PROACTIVE LEFT → PRE-STOP");
    trackingForward = false;
    intendedTurn = LEFT90;
    startAction(PRE_TURN_STOP, preTurnStopDuration);
    stopMotors();
    return;
  }

  // ── 6. MAIN FORWARD LOGIC ─────────────────────────────────
  if (distFront > wallThreshold) {
    bool leftDanger = latLeft < veryCloseThreshold * COS30;
    bool rightDanger = latRight < veryCloseThreshold * COS30;
    bool leftClose = distLeft < closeThreshold;
    bool rightClose = distRight < closeThreshold;

    // ── Reverse decision making (only for dangerous side proximity)
    if (shouldReverse(distFront, distLeft, distRight, latLeft, latRight)) {
      Serial.println(" | REVERSE DECISION → BACK OFF");
      trackingForward = false;
      startAction(REVERSE, shortReverseDuration);
      executeShortReverse();
      return;
    }

    // ── Left/right proximity adjustments act independently
    if (leftDanger) {
      Serial.println(" | LEFT TOO CLOSE → ADJUST RIGHT");
      trackingForward = false;
      startAction(ADJUST_RIGHT, smallAdjustDuration);
      executeAdjustRight();
      return;
    }
    if (rightDanger) {
      Serial.println(" | RIGHT TOO CLOSE → ADJUST LEFT");
      trackingForward = false;
      startAction(ADJUST_LEFT, smallAdjustDuration);
      executeAdjustLeft();
      return;
    }

    // ── Stuck detection ───────────────────────────────────
    if (!trackingForward) {
      // Start tracking this forward run
      forwardStartTime  = millis();
      forwardStartFront = distFront;
      trackingForward   = true;
    } else {
      unsigned long elapsed = millis() - forwardStartTime;
      if (elapsed >= stuckTimeLimit) {
        // Check if front distance changed enough to prove movement
        float distChange = forwardStartFront - distFront; // positive = moved closer
        if (distChange < stuckMovementThreshold) {
          // No meaningful progress → STUCK
          Serial.println(" | *** STUCK DETECTED → ESCAPE ***");
          trackingForward = false;
          startAction(STUCK_REVERSE, stuckReverseDuration);
          executeStuckReverse();
          return;
        } else {
          // We are moving — reset window
          forwardStartTime  = millis();
          forwardStartFront = distFront;
        }
      }
    }

    int speedL = baseSpeedLeft;
    int speedR = baseSpeedRight;

    // ── Corridor centering ──
    if (distLeft < 40 && distRight < 40) {
      float error    = distLeft - distRight;
      int correction = (int)(error * centerGain);
      correction     = constrain(correction, -maxCorrection, maxCorrection);
      speedL += correction;
      speedR -= correction;
      Serial.print(" | CENTER err="); Serial.print(error,1);
      Serial.print(" corr="); Serial.print(correction);
    } else {
      // Single-wall hug
      if (distLeft  < closeThreshold) { speedL -= 10; speedR += 10; Serial.print(" | HUG-L"); }
      if (distRight < closeThreshold) { speedL += 10; speedR -= 10; Serial.print(" | HUG-R"); }
      // Emergency scrape guard
      if (latLeft  < 5.0) { speedL -= 15; speedR += 15; Serial.print(" | SCRAPE-L!"); }
      if (latRight < 5.0) { speedL += 15; speedR -= 15; Serial.print(" | SCRAPE-R!"); }
    }

    speedL = constrain(speedL, minSpeed, 255);
    speedR = constrain(speedR, minSpeed, 255);

    Serial.println(" → FORWARD");
    executeForward(speedL, speedR);

  } else {
    // Front wall
    Serial.println(" | WALL! → STOP");
    trackingForward = false;
    wallDetected = true;
    startAction(STOP, shortStopDuration);
    stopMotors();
  }

  delay(30);
}

// ==========================================================
// MOTOR FUNCTIONS
// ==========================================================

void startAction(RobotState stateCode, int duration) {
  robotState            = stateCode;
  currentActionDuration = duration;
  actionStartTime       = millis();
}

float getDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);  delayMicroseconds(5);
  digitalWrite(trigPin, HIGH); delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH, 20000);
  if (duration < 100 || duration > 20000) return 999.0;
  float distance = duration * 0.034 / 2;
  return (distance > 50) ? 999.0 : distance;
}

void executeForward(int speedL, int speedR) {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW); analogWrite(ENA, speedL);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW); analogWrite(ENB, speedR);
}

// ── LEFT TURN: pivot arc or full spin based on usePivotArc ─
// Arc:  outer wheel (right) at full turnSpeed,
//       inner wheel (left)  at turnSpeed × innerWheelRatio.
//       Bot curves left — smooth, stays centered in cell.
// Spin: left reverse, right forward — original tight pivot.
void executeLeftTurn() {
  if (usePivotArc) {
    int outerSpeed = turnSpeedRight;
    int innerSpeed = (int)(turnSpeedLeft * innerWheelRatio);
    // Inner (left) crawls forward, outer (right) drives forward faster
    // → bot arcs left
    digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW); analogWrite(ENA, innerSpeed);
    digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW); analogWrite(ENB, outerSpeed);
  } else {
    // Original full spin
    digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH); analogWrite(ENA, turnSpeedLeft);
    digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);  analogWrite(ENB, turnSpeedRight);
  }
}

// ── RIGHT TURN ─────────────────────────────────────────────
// Arc:  outer wheel (left) at full turnSpeed,
//       inner wheel (right) at turnSpeed × innerWheelRatio.
// Spin: left forward, right reverse.
void executeRightTurn() {
  if (usePivotArc) {
    int outerSpeed = turnSpeedLeft;
    int innerSpeed = (int)(turnSpeedRight * innerWheelRatio);
    digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW); analogWrite(ENA, outerSpeed);
    digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW); analogWrite(ENB, innerSpeed);
  } else {
    // Original full spin
    digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);  analogWrite(ENA, turnSpeedLeft);
    digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH); analogWrite(ENB, turnSpeedRight);
  }
}

void executeAdjustLeft() {
  digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH); analogWrite(ENA, turnSpeedLeft  / 2);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);  analogWrite(ENB, turnSpeedRight / 2);
}

void executeAdjustRight() {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);  analogWrite(ENA, turnSpeedLeft  / 2);
  digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH); analogWrite(ENB, turnSpeedRight / 2);
}

void executeShortReverse() {
  // Normal post-wall reverse: slow and controlled
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH); analogWrite(ENA, baseSpeedLeft);
  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH); analogWrite(ENB, baseSpeedRight);
}

// ── Stuck escape: harder reverse to break free ─────────────
// Slightly faster than normal reverse to overcome any jam.
void executeStuckReverse() {
  int escapeSpeed = constrain(baseSpeedLeft + 30, 0, 255);
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH); analogWrite(ENA, escapeSpeed);
  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH); analogWrite(ENB, escapeSpeed);
}

// ── Wiggle left/right — shakes wheels free after being stuck ─
// These are short arcs, not full turns.
void executeWiggleLeft() {
  digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH); analogWrite(ENA, turnSpeedLeft);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);  analogWrite(ENB, turnSpeedRight);
}

void executeWiggleRight() {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);  analogWrite(ENA, turnSpeedLeft);
  digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH); analogWrite(ENB, turnSpeedRight);
}

void stopMotors() {
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);
}

bool shouldReverse(float distFront, float distLeft, float distRight, float latLeft, float latRight) {
  // Decide whether to back off instead of continuing forward.
  // Reverse only when both sides are dangerously close at the same time.
  if (distFront > wallThreshold) {
    bool leftDanger  = latLeft  < veryCloseThreshold * COS30;
    bool rightDanger = latRight < veryCloseThreshold * COS30;
    return leftDanger && rightDanger;
  }
  return false;
}

// ==========================================================
// TUNING GUIDE
// ==========================================================
// PIVOT ARC TUNING
//   usePivotArc = true  → smooth arc turns (default, recommended)
//   usePivotArc = false → tight spin-in-place (narrow corridors)
//
//   innerWheelRatio (0.0–0.5):
//     0.0 = spin in place (same as usePivotArc=false)
//     0.3 = gentle arc (good starting point)
//     0.5 = wide sweeping arc (needs room)
//   Start at 0.3. If the bot clips the outer wall during a turn,
//   reduce to 0.2. If turns are too wide, reduce to 0.15.
//
//   pivotArcDuration:
//     Start at turnDuration90 × 1.25 (default: 375ms).
//     Place bot at cell center, run a left arc. It should face
//     the next corridor exactly. Adjust ±25ms.
//
// STUCK DETECTION TUNING
//   stuckTimeLimit (default 800ms):
//     How long to wait before declaring stuck. Increase on slow
//     floors (carpet). Decrease on fast floors (tiles).
//   stuckMovementThreshold (default 2.0cm):
//     Minimum front distance change to prove the bot is moving.
//     Increase if false stuck triggers occur on straight runs.
//     Decrease if the bot doesn't detect real stalls fast enough.
//   stuckReverseDuration (default 400ms):
//     Longer than shortReverseDuration to really pull free.
//   stuckWiggleDuration (default 180ms):
//     Each wiggle direction. Increase if wheels are still jammed
//     after the first escape attempt.
//
// GENERAL
//   wallThreshold (15cm): increase if nose clips wall before stop.
//   openTurnThreshold (18cm at 30°→15.6cm lateral): true full cell open.
//   centerGain (2.5): increase if drifts, decrease if snakes.
// ==========================================================
