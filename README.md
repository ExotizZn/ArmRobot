# ArmRobot - Le T.I.G.R.E
Ce projet consiste en la conception, la programmation et le contrôle d’un bras robotisé à l’aide d’une carte Arduino. Le bras peut être utilisé pour des tâches simples comme la rotation ou le positionnement d’objets légers.

![Animation de la télécommande](animations/remote_animation.gif)

## Matériel nécessaire

- 2 × Carte Arduino UNO R3
- 3 × Servomoteurs MG995
- 3 x Servomoteurs MG90S
- 1 × Alimentation externe (5V / 3A recommandée)
- 6 potentiomètres
- 1 HC06 & 1 HC05 ou 2 HC05
- Vis M3
- Fils de connexion
- Structure du bras

## Prérequis
- SDL2, SDL2_ttf, SDL2_gfx & zenity
```bash
sudo apt update
sudo apt install -y build-essential libsdl2-dev libsdl2-ttf-dev libsdl2-gfx-dev zenity
```

## Installation
### Compiler l'interface de contrôle
```bash
git clone https://github.com/ExotizZn/ArmRobot.git
cd ArmRobot
make
```
### Compiler et téléverser le firmware du robot sur l'arduino
1. Ouvrir le fichier "robotarm_firmware/robotarm_firmware.ino" dans Arduino IDE
2. Compiler et téléverser sur la carte Arduino.

### Compiler et téléverser le firmware de la télécommande sur l'arduino 
1. Ouvrir le fichier "remote_firmware/remote_firmware.ino" dans Arduino IDE
2. Compiler et téléverser sur la carte Arduino.


