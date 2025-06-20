#include <SoftwareSerial.h>
#include <FastLED.h>

// ---------- CONFIGURATION LED ----------
#define LED_PIN     6
#define NUM_LEDS    15
#define BRIGHTNESS  100

CRGB leds[NUM_LEDS];

// ---------- CONFIGURATION BLUETOOTH HC-06 ----------
#define BT_RX 10 // Connecté au TX du HC-06
#define BT_TX 11 // Connecté au RX du HC-06
SoftwareSerial bluetooth(BT_RX, BT_TX);

// ---------- CONFIGURATION POTENTIOMÈTRES ----------
const int potPins[6] = {A0, A1, A2, A3, A4, A5};

// ---------- LISSAGE DES VALEURS ----------
const int numReadings = 10;
int readings[6][numReadings];
int readIndex[6] = {0};
int total[6] = {0};
int average[6] = {0};

int currentAngles[6] = {90, 90, 90, 90, 90, 90};
int lastSentAngles[6] = {90, 90, 90, 90, 90, 90};
unsigned long lastSendTime = 0;
const int sendInterval = 100;
const int threshold = 7;

void setup() {
  Serial.begin(9600);
  bluetooth.begin(38400);

  // Pins Bluetooth
  pinMode(BT_RX, INPUT);
  pinMode(BT_TX, OUTPUT);

  // Initialiser LEDs
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  fill_solid(leds, NUM_LEDS, CRGB::White);
  FastLED.show();

  // Initialisation potentiomètres
  for (int i = 0; i < 6; i++) {
    pinMode(potPins[i], INPUT);
    for (int j = 0; j < numReadings; j++) {
      readings[i][j] = 0;
    }
  }

  Serial.println("Contrôleur Bras Robot avec LEDs et Potentiomètres");
}

void loop() {
  for (int i = 0; i < 6; i++) {
    total[i] = total[i] - readings[i][readIndex[i]];
    readings[i][readIndex[i]] = analogRead(potPins[i]);
    total[i] = total[i] + readings[i][readIndex[i]];
    readIndex[i] = (readIndex[i] + 1) % numReadings;
    average[i] = total[i] / numReadings;
    int newAngle = map(average[i], 0, 1023, 180, 0);

    if (abs(newAngle - lastSentAngles[i]) >= threshold && (millis() - lastSendTime > sendInterval)) {
      currentAngles[i] = newAngle;
      sendCommand(i);
      lastSentAngles[i] = currentAngles[i];
      lastSendTime = millis();

      Serial.print("Commande envoyée: S");
      Serial.print(i + 1);
      Serial.println(currentAngles[i]);
    }
  }

  FastLED.show(); // Rafraîchir l’affichage (au cas où tu changes les couleurs plus tard)
  delay(1);
}

void sendCommand(int servoIndex) {
  String command = "S" + String(servoIndex + 1) + String(currentAngles[servoIndex]);
  bluetooth.println(command);
}
