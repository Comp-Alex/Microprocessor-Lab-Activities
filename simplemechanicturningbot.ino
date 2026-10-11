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
    int baseSpeedRight  = 120;
    int turnSpeedLeft   = 150;
    int turnSpeedRight  = 140;
    int wallThreshold   = 18;   // Distance to detect wall (cm)
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
    // Move forward; if any sensor detects a wall, stop and wait until ALL sensors are clear.
    float distLeft  = getDistance(LEFT_TRIG, LEFT_ECHO);
    float distFront = getDistance(FRONT_TRIG, FRONT_ECHO);
    float distRight = getDistance(RIGHT_TRIG, RIGHT_ECHO);

    Serial.print("L:"); Serial.print(distLeft, 0);
    Serial.print(" F:"); Serial.print(distFront, 0);
    Serial.print(" R:"); Serial.print(distRight, 0);

    if (distLeft > wallThreshold && distFront > wallThreshold && distRight > wallThreshold) {
        Serial.println(" → FORWARD");
        executeForward();
    } else {
        Serial.print(" → OBSTACLE:");
        if (distLeft <= wallThreshold) Serial.print(" LEFT");
        if (distFront <= wallThreshold) Serial.print(" FRONT");
        if (distRight <= wallThreshold) Serial.print(" RIGHT");
        Serial.println(" STOPPING");
        stopMotors();

        // Blocking wait until all sensors report clear
        while (true) {
        float dl = getDistance(LEFT_TRIG, LEFT_ECHO);
        float df = getDistance(FRONT_TRIG, FRONT_ECHO);
        float dr = getDistance(RIGHT_TRIG, RIGHT_ECHO);
        if (dl > wallThreshold && df > wallThreshold && dr > wallThreshold) break;
        delay(100);
        }

        Serial.println(" → CLEARED: TURNING 90°");
        // Execute 90 degree turn (left preferred)
        executeLeftTurn();
        delay(turnDuration90);
        stopMotors();
        delay(200);
        Serial.println(" → TURN COMPLETE: RESUMING FORWARD");
        executeForward();
    }

    delay(50);
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

    void executeForward() {
    // Straight with motor compensation
    digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW); analogWrite(ENA, baseSpeedLeft);
    digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW); analogWrite(ENB, baseSpeedRight);
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