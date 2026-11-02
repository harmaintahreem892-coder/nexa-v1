/*
  NEXA v1 - Obstacle avoidance + TFT animated eyes + DFPlayer voice

  Pins:
    Ultrasonic: TRIG = D3, ECHO = D2
    L298N:      IN1 = D10, IN2 = D9, IN3 = D8, IN4 = D7, ENA = D11, ENB = D6
    Servo:      D5
    TFT (software SPI): CS=A0, DC=A1, MOSI=A2, SCLK=A3, RST=A4, VCC/BL=5V, GND=GND
    DFPlayer:   TX -> D4 (Arduino receives here)
                RX -> D12 (Arduino sends here, THROUGH a ~1k resistor)
                VCC -> 5V, GND -> GND, speaker -> SPK_1/SPK_2

  microSD /mp3 folder, files named exactly:
    0001.mp3  "hello everyone, i am nexa"
    0002.mp3  "welcome to the freshers party"
    0003.mp3  "alright lets bring this machine to life"
    0004.mp3  "oh, somethings in my way"
    0005.mp3  "that was fun i'll be back"

  Libraries needed: Servo (built in), SoftwareSerial (built in),
  DFRobotDFPlayerMini, Adafruit GFX Library, Adafruit ST7735 and ST7789 Library
*/

#include <Servo.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>

// -------- ULTRASONIC --------
#define trigPin 3
#define echoPin 2

// -------- MOTOR DRIVER --------
#define IN1 10
#define IN2 9
#define IN3 8
#define IN4 7
#define ENA 11
#define ENB 6

// -------- SERVO --------
#define servoPin 5
Servo myServo;

// -------- SPEED --------
int speedLeft = 140;
int speedRight = 140;

// -------- TFT (software SPI) --------
#define TFT_CS   A0
#define TFT_DC   A1
#define TFT_MOSI A2
#define TFT_SCLK A3
#define TFT_RST  A4
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

// -------- DFPlayer --------
SoftwareSerial dfSerial(4, 12); // RX(from DFPlayer TX)=D4, TX(to DFPlayer RX)=D12
DFRobotDFPlayerMini dfPlayer;
bool dfReady = false;

#define AUDIO_HELLO      1
#define AUDIO_WELCOME    2
#define AUDIO_START      3
#define AUDIO_OBSTACLE   4
#define AUDIO_FAREWELL   5

// -------- EYE SETTINGS --------
#define EYE_COLOR ST77XX_CYAN
#define BG_COLOR  ST77XX_BLACK

const int EYE_LEFT_X  = 50;
const int EYE_RIGHT_X = 110;
const int EYE_Y       = 64;
const int EYE_W       = 30;
const int EYE_H       = 38;
const int EYE_R       = 8;

int lastOx = 0, lastOy = 0, lastW = 0, lastH = 0;
unsigned long nextBlink = 0;
int obstacleCount = 0;

// -------- SETUP --------
void setup() {
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);

  myServo.attach(servoPin);
  myServo.write(90);

  stopCar();

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(BG_COLOR);
  eyesNormal();

  dfSerial.begin(9600);
  if (dfPlayer.begin(dfSerial)) {
    dfReady = true;
    dfPlayer.volume(30);
  }

  randomSeed(analogRead(A5));
  nextBlink = millis() + 3000;

  if (dfReady) {
    dfPlayer.play(AUDIO_HELLO);
    waitForTrack();
    dfPlayer.play(AUDIO_WELCOME);
    waitForTrack();
    dfPlayer.play(AUDIO_START);
    waitForTrack();
  } else {
    delay(2500);
  }
}

// -------- LOOP --------
void loop() {

  int frontDist = getDistance();

  if (frontDist > 25) {
    ensureNormalEyes();
    forward();
    handleBlink();
  }
  else {

    stopCar();
    eyesSurprised();
    if (dfReady) dfPlayer.playMp3Folder(AUDIO_OBSTACLE);
    delay(100);

    backward();
    delay(350);

    stopCar();
    delay(100);

    int rightDist = lookRight();
    int leftDist  = lookLeft();

    delay(100);

    if (rightDist > leftDist) {
      eyesLookRight();
      turnRight();
    } else {
      eyesLookLeft();
      turnLeft();
    }

    stopCar();
    delay(100);

    obstacleCount++;
    if (obstacleCount >= 3) {
      obstacleCount = 0;
      if (dfReady) dfPlayer.play(AUDIO_FAREWELL);
    }
  }
}

// -------- DFPLAYER HELPER --------
void waitForTrack() {
  delay(300);
  unsigned long start = millis();
  while (millis() - start < 8000) {
    if (dfPlayer.available()) {
      if (dfPlayer.readType() == DFPlayerPlayFinished) break;
    }
  }
}

// -------- EYES --------
void eraseEyes() {
  tft.fillRect(EYE_LEFT_X  + lastOx - lastW / 2, EYE_Y + lastOy - lastH / 2, lastW, lastH, BG_COLOR);
  tft.fillRect(EYE_RIGHT_X + lastOx - lastW / 2, EYE_Y + lastOy - lastH / 2, lastW, lastH, BG_COLOR);
}

void drawEyes(int ox, int oy, int w, int h) {
  if (lastW > 0) eraseEyes();
  int r = min(EYE_R, h / 2);
  tft.fillRoundRect(EYE_LEFT_X  + ox - w / 2, EYE_Y + oy - h / 2, w, h, r, EYE_COLOR);
  tft.fillRoundRect(EYE_RIGHT_X + ox - w / 2, EYE_Y + oy - h / 2, w, h, r, EYE_COLOR);
  lastOx = ox; lastOy = oy; lastW = w; lastH = h;
}

void eyesNormal()    { drawEyes(0, 0, EYE_W, EYE_H); }
void eyesLookLeft()  { drawEyes(-14, 0, EYE_W, EYE_H); }
void eyesLookRight() { drawEyes(14, 0, EYE_W, EYE_H); }
void eyesSurprised() { drawEyes(0, 0, EYE_W + 8, EYE_H + 12); }

void eyesBlink() {
  drawEyes(0, 0, EYE_W, 4);
  delay(100);
  drawEyes(0, 0, EYE_W, EYE_H);
}

void ensureNormalEyes() {
  if (lastOx != 0 || lastOy != 0 || lastW != EYE_W || lastH != EYE_H) {
    eyesNormal();
  }
}

void handleBlink() {
  if (millis() >= nextBlink) {
    eyesBlink();
    nextBlink = millis() + random(2500, 6000);
  }
}

// -------- ULTRASONIC --------
int getDistance() {
  int readings[3];
  for (int i = 0; i < 3; i++) {
    readings[i] = getRawDistance();
    delay(10);
  }
  // sort the 3 readings, smallest to largest
  for (int i = 0; i < 2; i++) {
    for (int j = i + 1; j < 3; j++) {
      if (readings[j] < readings[i]) {
        int temp = readings[i];
        readings[i] = readings[j];
        readings[j] = temp;
      }
    }
  }
  return readings[1]; // the middle value, ignoring the noisiest outlier
}

int getRawDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH, 25000);
  if (duration == 0) return 200;
  return duration * 0.034 / 2;
}

// -------- SCAN SERVO --------
int lookRight() {
  myServo.write(10);
  eyesLookRight();
  delay(300);
  int d = getDistance();
  myServo.write(90);
  delay(200);
  return d;
}

int lookLeft() {
  myServo.write(170);
  eyesLookLeft();
  delay(300);
  int d = getDistance();
  myServo.write(90);
  delay(200);
  return d;
}

// -------- MOVEMENTS --------
void forward() {
  analogWrite(ENA, speedRight);
  analogWrite(ENB, speedLeft);
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void backward() {
  analogWrite(ENA, speedRight);
  analogWrite(ENB, speedLeft);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void turnRight() {
  analogWrite(ENA, speedRight);
  analogWrite(ENB, speedLeft);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  delay(400);
}

void turnLeft() {
  analogWrite(ENA, speedRight);
  analogWrite(ENB, speedLeft);
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
  delay(400);
}

void stopCar() {
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}