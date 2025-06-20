#include <VarSpeedServo.h>
#include <SoftwareSerial.h>

// Configuration Bluetooth HC-06
#define BT_RX 2  // Connecté au TX du HC-06
#define BT_TX 3  // Connecté au RX du HC-06
SoftwareSerial bluetooth(BT_RX, BT_TX);

// Configuration des servos
VarSpeedServo servos[6];
const int servoPins[6] = {4, 5, 6, 9, 10, 11};  // Broches pour Servos 1-6
int currentPositions[6] = {90, 90, 90, 90, 90, 90};
const uint8_t SERVO_SPEED = 50;  // Vitesse des servos (0-255, 50 = modéré)

// Buffers pour la réception
char btBuffer[16];    // Buffer pour Bluetooth
char comBuffer[16];   // Buffer pour port COM
int btIndex = 0;      // Index pour buffer Bluetooth
int comIndex = 0;     // Index pour buffer port COM
const int COMMAND_TIMEOUT = 50;  // Timeout pour compatibilité

void setup() {
  // Initialisation servos
  for (int i = 0; i < 6; i++) {
    servos[i].attach(servoPins[i]);
    servos[i].write(currentPositions[i], SERVO_SPEED, true);  // Mouvement initial à vitesse modérée
  }
  
  // Communication
  Serial.begin(115200);
  bluetooth.begin(38400);  // HC-06 configuré à 115200 bauds
  
  Serial.println("Bras Robot - Prêt à recevoir des commandes via Bluetooth ou port COM");
}

void loop() {
  // Réception Bluetooth
  while (bluetooth.available()) {
    char c = bluetooth.read();
    
    if (c == '\n') {
      btBuffer[btIndex] = '\0';  // Terminer la chaîne
      processCommand(btBuffer, "Bluetooth");
      btIndex = 0;  // Réinitialiser l'index
    } else if (btIndex < sizeof(btBuffer) - 1) {
      btBuffer[btIndex++] = c;
    }
  }

  // Réception port COM
  while (Serial.available()) {
    char c = Serial.read();
    
    if (c == '\n') {
      comBuffer[comIndex] = '\0';  // Terminer la chaîne
      processCommand(comBuffer, "COM");
      comIndex = 0;  // Réinitialiser l'index
    } else if (comIndex < sizeof(comBuffer) - 1) {
      comBuffer[comIndex++] = c;
    }
  }
}

void processCommand(const char *cmd, const char *source) {
  // Ignorer les commandes vides
  if (cmd[0] == '\0') return;
  
  // Afficher la commande reçue avec la source
  Serial.print("Commande reçue via ");
  Serial.print(source);
  Serial.print(": ");
  Serial.println(cmd);

  // Format attendu: "S<num><angle>" (ex: "S1120")
  if (cmd[0] == 'S' && strlen(cmd) >= 3) {
    int servoNum = cmd[1] - '1';  // Convertir '1'-'6' en index 0-5
    int angle = atoi(&cmd[2]);    // Convertir la partie angle
    
    // Validation
    if (servoNum >= 0 && servoNum < 6 && angle >= 0 && angle <= 180) {
      servos[servoNum].write(angle, SERVO_SPEED, true);  // Mouvement à vitesse modérée
      currentPositions[servoNum] = angle;
      
      // Feedback
      Serial.print("Servo ");
      Serial.print(servoNum + 1);
      Serial.print(" → ");
      Serial.print(angle);
      Serial.print("° (via ");
      Serial.print(source);
      Serial.println(")");
      
      // Envoyer le feedback via Bluetooth si la commande vient du port COM
      if (strcmp(source, "COM") == 0) {
        bluetooth.print("Servo ");
        bluetooth.print(servoNum + 1);
        bluetooth.print(" → ");
        bluetooth.print(angle);
        bluetooth.println("°");
      }
    } else {
      Serial.print("Commande invalide: ");
      Serial.println(cmd);
    }
  } else {
    Serial.print("Format incorrect: ");
    Serial.println(cmd);
  }
}