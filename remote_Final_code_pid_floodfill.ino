// Ensure Arduino core symbols and types are available
#include <Arduino.h>
#include <math.h>

// ---------------------------
// Pin definitions 
// ---------------------------
#ifndef ENA
#define ENA 5
#endif
#ifndef IN1
#define IN1 4
#endif
#ifndef IN2
#define IN2 3
#endif
#ifndef ENB
#define ENB 6
#endif
#ifndef IN3
#define IN3 7
#endif
#ifndef IN4
#define IN4 8
#endif

#ifndef trigF
#define trigF A2
#endif
#ifndef echoF
#define echoF A3
#endif
#ifndef trigL
#define trigL A0
#endif
#ifndef echoL
#define echoL A1
#endif
#ifndef trigR
#define trigR A4
#endif
#ifndef echoR
#define echoR A5
#endif

// ---------------------------
// PID + Flood-Fill configuration
// ---------------------------
const int PIDFF_forwardSpeed = 120;  // base forward PWM
const int PIDFF_turnSpeed = 160;     // turn-in-place PWM
const int PIDFF_thresholdFront = 12; // cm to consider obstacle
const int PIDFF_thresholdSide = 18;  // cm side wall threshold

// PID gains (tune on the robot)
float PIDFF_kp = 2.0;
float PIDFF_ki = 0.01;
float PIDFF_kd = 0.35;
float PIDFF_integral = 0.0;
float PIDFF_lastError = 0.0;
unsigned long PIDFF_lastTime = 0;

// Flood-fill map size
#define PIDFF_MAP_N 7
// 0 = unknown, 1 = free, 2 = wall
uint8_t PIDFF_map[PIDFF_MAP_N][PIDFF_MAP_N];
int PIDFF_dist[PIDFF_MAP_N][PIDFF_MAP_N];

// Robot state in cell coordinates (center cell = (3,3) initial)
int PIDFF_x = PIDFF_MAP_N / 2;
int PIDFF_y = PIDFF_MAP_N / 2;
// Heading: 0=N, 1=E, 2=S, 3=W
int PIDFF_heading = 0;

// Desired distance from left wall when side-following
const float PIDFF_desiredLeft = 15.0;

// Timing for moving one cell (approximate, adapt for your robot)
const unsigned long PIDFF_cellMoveTime = 500; // ms moving forward per cell

// ---------------------------
// Helper: readDistance (same algorithm as original sketches)
// ---------------------------
long PIDFF_readDistanceCM(int trigPin, int echoPin, unsigned long timeout = 30000UL)
{
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);
    unsigned long duration = pulseIn(echoPin, HIGH, timeout);
    if (duration == 0)
        return -1;
    return duration / 58L;
}

// ---------------------------
// Motor helpers (unique names to avoid collisions)
// These accept signed speeds for individual motors
// ---------------------------
void PIDFF_setLeftMotor(int leftSpeed)
{
    if (leftSpeed >= 0)
    {
        digitalWrite(IN1, HIGH);
        digitalWrite(IN2, LOW);
        analogWrite(ENA, constrain(leftSpeed, 0, 255));
    }
    else
    {
        digitalWrite(IN1, LOW);
        digitalWrite(IN2, HIGH);
        analogWrite(ENA, constrain(-leftSpeed, 0, 255));
    }
}

void PIDFF_setRightMotor(int rightSpeed)
{
    if (rightSpeed >= 0)
    {
        digitalWrite(IN3, HIGH);
        digitalWrite(IN4, LOW);
        analogWrite(ENB, constrain(rightSpeed, 0, 255));
    }
    else
    {
        digitalWrite(IN3, LOW);
        digitalWrite(IN4, HIGH);
        analogWrite(ENB, constrain(-rightSpeed, 0, 255));
    }
}

void PIDFF_driveMotors(int leftSpeed, int rightSpeed)
{
    PIDFF_setLeftMotor(leftSpeed);
    PIDFF_setRightMotor(rightSpeed);
}

void PIDFF_stopMotors()
{
    analogWrite(ENA, 0);
    analogWrite(ENB, 0);
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, LOW);
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, LOW);
}

void PIDFF_turnLeftInPlace(int speed)
{
    // left wheel reverse, right wheel forward
    PIDFF_setLeftMotor(-speed);
    PIDFF_setRightMotor(speed);
}

void PIDFF_turnRightInPlace(int speed)
{
    // left wheel forward, right wheel reverse
    PIDFF_setLeftMotor(speed);
    PIDFF_setRightMotor(-speed);
}

// Simple forward using signed motor control for compatibility
void PIDFF_forwardMotors(int speed)
{
    PIDFF_driveMotors(speed, speed);
}

void PIDFF_reverseMotors(int speed)
{
    PIDFF_driveMotors(-speed, -speed);
}

// ---------------------------
// PID compute: returns steering offset in PWM units
// Positive -> increase right motor relative to left
// ---------------------------
float PIDFF_computePID(float error)
{
    unsigned long now = millis();
    float dt = 0.02; // default small dt
    if (PIDFF_lastTime != 0)
    {
        dt = (now - PIDFF_lastTime) / 1000.0;
        if (dt <= 0)
            dt = 0.0001;
    }
    PIDFF_integral += error * dt;
    float derivative = (error - PIDFF_lastError) / dt;
    float output = PIDFF_kp * error + PIDFF_ki * PIDFF_integral + PIDFF_kd * derivative;
    PIDFF_lastError = error;
    PIDFF_lastTime = now;
    return output;
}

// ---------------------------
// Flood-fill helpers
// ---------------------------
// Reset map: unknown = 0
void PIDFF_mapReset()
{
    for (int i = 0; i < PIDFF_MAP_N; i++)
        for (int j = 0; j < PIDFF_MAP_N; j++)
        {
            PIDFF_map[i][j] = 0;
            PIDFF_dist[i][j] = 255;
        }
}

// Mark a cell as free or wall safely
void PIDFF_setCellFree(int x, int y)
{
    if (x >= 0 && x < PIDFF_MAP_N && y >= 0 && y < PIDFF_MAP_N)
        PIDFF_map[y][x] = 1;
}
void PIDFF_setCellWall(int x, int y)
{
    if (x >= 0 && x < PIDFF_MAP_N && y >= 0 && y < PIDFF_MAP_N)
        PIDFF_map[y][x] = 2;
}

// Given current cell and heading, mark walls based on sensors
void PIDFF_updateWallsFromSensors(long distF, long distL, long distR)
{
    // Directions: 0=N -> (0,-1), 1=E -> (1,0), 2=S -> (0,1), 3=W -> (-1,0)
    int dx[4] = {0, 1, 0, -1};
    int dy[4] = {-1, 0, 1, 0};

    // front
    int fx = PIDFF_x + dx[PIDFF_heading];
    int fy = PIDFF_y + dy[PIDFF_heading];
    if (distF == -1 || distF > PIDFF_thresholdFront)
        PIDFF_setCellFree(fx, fy);
    else
        PIDFF_setCellWall(fx, fy);

    // left sensor: heading-1
    int leftDir = (PIDFF_heading + 3) & 3;
    int lx = PIDFF_x + dx[leftDir];
    int ly = PIDFF_y + dy[leftDir];
    if (distL == -1 || distL > PIDFF_thresholdSide)
        PIDFF_setCellFree(lx, ly);
    else
        PIDFF_setCellWall(lx, ly);

    // right sensor: heading+1
    int rightDir = (PIDFF_heading + 1) & 3;
    int rx = PIDFF_x + dx[rightDir];
    int ry = PIDFF_y + dy[rightDir];
    if (distR == -1 || distR > PIDFF_thresholdSide)
        PIDFF_setCellFree(rx, ry);
    else
        PIDFF_setCellWall(rx, ry);

    // mark current cell free
    PIDFF_setCellFree(PIDFF_x, PIDFF_y);
}

// Flood-fill / BFS from a target cell to compute distance map
void PIDFF_computeDistancesTo(int tx, int ty)
{
    for (int y = 0; y < PIDFF_MAP_N; y++)
        for (int x = 0; x < PIDFF_MAP_N; x++)
            PIDFF_dist[y][x] = 255;
    // queue simple circular array
    const int MAXQ = PIDFF_MAP_N * PIDFF_MAP_N;
    int qx[MAXQ], qy[MAXQ];
    int qs = 0, qe = 0;
    // start from target
    PIDFF_dist[ty][tx] = 0;
    qx[qe] = tx;
    qy[qe] = ty;
    qe++;

    while (qs < qe)
    {
        int cx = qx[qs];
        int cy = qy[qs];
        qs++;
        int cd = PIDFF_dist[cy][cx];
        // neighbors
        const int dx[4] = {0, 1, 0, -1};
        const int dy[4] = {-1, 0, 1, 0};
        for (int i = 0; i < 4; i++)
        {
            int nx = cx + dx[i];
            int ny = cy + dy[i];
            if (nx < 0 || nx >= PIDFF_MAP_N || ny < 0 || ny >= PIDFF_MAP_N)
                continue;
            // if cell is wall, skip
            if (PIDFF_map[ny][nx] == 2)
                continue;
            if (PIDFF_dist[ny][nx] > cd + 1)
            {
                PIDFF_dist[ny][nx] = cd + 1;
                qx[qe] = nx;
                qy[qe] = ny;
                qe++;
            }
        }
    }
}

// Choose next neighbor cell (x,y) to move to (lowest distance)
bool PIDFF_chooseNextCell(int *nx_out, int *ny_out)
{
    int bestx = -1, besty = -1;
    int bestd = 10000;
    const int dx[4] = {0, 1, 0, -1};
    const int dy[4] = {-1, 0, 1, 0};
    for (int i = 0; i < 4; i++)
    {
        int nx = PIDFF_x + dx[i];
        int ny = PIDFF_y + dy[i];
        if (nx < 0 || nx >= PIDFF_MAP_N || ny < 0 || ny >= PIDFF_MAP_N)
            continue;
        if (PIDFF_map[ny][nx] == 2)
            continue; // wall
        int d = PIDFF_dist[ny][nx];
        if (d < bestd)
        {
            bestd = d;
            bestx = nx;
            besty = ny;
        }
    }
    if (bestx == -1)
        return false;
    *nx_out = bestx;
    *ny_out = besty;
    return true;
}

// Rotate the robot to face a target neighbor cell and move one cell forward
void PIDFF_moveToCell(int tx, int ty)
{
    // compute desired heading
    int dx = tx - PIDFF_x;
    int dy = ty - PIDFF_y;
    int desired = PIDFF_heading;
    if (dx == 1)
        desired = 1;
    else if (dx == -1)
        desired = 3;
    else if (dy == 1)
        desired = 2;
    else if (dy == -1)
        desired = 0;

    int diff = (desired - PIDFF_heading + 4) % 4;
    if (diff == 1)
    {
        // turn right 90
        PIDFF_turnRightInPlace(PIDFF_turnSpeed);
        delay(260);
        PIDFF_stopMotors();
        delay(50);
    }
    else if (diff == 2)
    {
        // turn 180
        PIDFF_turnRightInPlace(PIDFF_turnSpeed);
        delay(520);
        PIDFF_stopMotors();
        delay(50);
    }
    else if (diff == 3)
    {
        // turn left 90
        PIDFF_turnLeftInPlace(PIDFF_turnSpeed);
        delay(260);
        PIDFF_stopMotors();
        delay(50);
    }

    // Move forward approximately one cell while running PID side-following
    unsigned long start = millis();
    while (millis() - start < PIDFF_cellMoveTime)
    {
        long dL = PIDFF_readDistanceCM(trigL, echoL);
        long dR = PIDFF_readDistanceCM(trigR, echoR);
        // prefer left sensor for side-following; if unavailable, mirror using right
        float currentSide = NAN;
        int useLeft = 0;
        if (dL != -1)
        {
            currentSide = dL;
            useLeft = 1;
        }
        else if (dR != -1)
        {
            currentSide = dR;
            useLeft = 0;
        }

        if (!isnan(currentSide))
        {
            float err = PIDFF_desiredLeft - currentSide;
            float steer = PIDFF_computePID(err);
            if (!useLeft)
                steer = -steer; // mirror when using right
            int leftPWM = (int)constrain(PIDFF_forwardSpeed - steer, -255, 255);
            int rightPWM = (int)constrain(PIDFF_forwardSpeed + steer, -255, 255);
            PIDFF_driveMotors(leftPWM, rightPWM);
        }
        else
        {
            PIDFF_forwardMotors(PIDFF_forwardSpeed);
        }
        delay(20);
    }
    PIDFF_stopMotors();
    delay(30);

    // update logical position
    PIDFF_heading = desired;
    PIDFF_x = tx;
    PIDFF_y = ty;
}

// ---------------------------
// Setup & Loop
// ---------------------------
void setup()
{
    // pin modes (safe to call even if other sketch also sets them)
    pinMode(ENA, OUTPUT);
    pinMode(ENB, OUTPUT);
    pinMode(IN1, OUTPUT);
    pinMode(IN2, OUTPUT);
    pinMode(IN3, OUTPUT);
    pinMode(IN4, OUTPUT);
    pinMode(trigF, OUTPUT);
    pinMode(echoF, INPUT);
    pinMode(trigL, OUTPUT);
    pinMode(echoL, INPUT);
    pinMode(trigR, OUTPUT);
    pinMode(echoR, INPUT);

    Serial.begin(115200);
    PIDFF_stopMotors();
    PIDFF_mapReset();
    // mark starting cell free
    PIDFF_setCellFree(PIDFF_x, PIDFF_y);
}

void loop()
{
    // read sensors
    long dF = PIDFF_readDistanceCM(trigF, echoF);
    long dL = PIDFF_readDistanceCM(trigL, echoL);
    long dR = PIDFF_readDistanceCM(trigR, echoR);

    // update map knowledge
    PIDFF_updateWallsFromSensors(dF, dL, dR);

    // set a goal: center of map for now
    int goalX = PIDFF_MAP_N / 2;
    int goalY = PIDFF_MAP_N / 2;

    // compute distances from goal
    PIDFF_computeDistancesTo(goalX, goalY);

    // choose next cell to move to
    int nx, ny;
    if (PIDFF_chooseNextCell(&nx, &ny))
    {
        Serial.print("Moving to: ");
        Serial.print(nx);
        Serial.print(",");
        Serial.println(ny);
        PIDFF_moveToCell(nx, ny);
    }
    else
    {
        // no reachable neighbor (surrounded by walls or unknown) -> rotate to scan
        Serial.println("No neighbor chosen; rotating to scan");
        PIDFF_turnRightInPlace(PIDFF_turnSpeed);
        delay(300);
        PIDFF_stopMotors();
        delay(200);
    }

    // short delay between planning cycles
    delay(100);
}
