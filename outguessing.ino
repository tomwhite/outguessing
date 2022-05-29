// Includes for 7-segment display
#include <Wire.h>
#include "Adafruit_LEDBackpack.h"
#include "Adafruit_GFX.h"

class DebouncedButton {
  long debounceDelay;
  int pin;
  int buttonState;
  int lastButtonState;
  long lastDebounceTime;
  
  public:
  DebouncedButton(int buttonPin) {
    debounceDelay = 20;
    pin = buttonPin;
    buttonState = LOW;
    lastButtonState = LOW;
    lastDebounceTime = 0;
  }
  
  boolean buttonPressed() {
    boolean pressed = false;
    int reading = digitalRead(pin);
    if (reading != lastButtonState) {
      // reset the debouncing timer
      lastDebounceTime = millis();
    }
    if ((millis() - lastDebounceTime) > debounceDelay) {
      if (reading != buttonState) {
        buttonState = reading;
        if (buttonState == HIGH) {
          pressed = true;
        }
      }
    }
    lastButtonState = reading;
    return pressed;
  }
};

class RandomStrategy {
  public:
  int makeMove() {
    return random(2);
  }
  void opponentsMove(int move) {
    // ignore
  }
};

class ShannonStrategy {
  int ownMoveHistory[100];
  int oppMoveHistory[100];
  int numMoves;
  
  byte played;
  byte prev;
  byte repeat;
  
  public:
  int makeMove() {
    if (numMoves < 3) {
      Serial.println("Random for first 3 moves");
      return random(2);
    }
    int ownM1 = ownMoveHistory[numMoves - 3];
    int oppM1 = oppMoveHistory[numMoves - 3];
    int ownM2 = ownMoveHistory[numMoves - 2];
    int oppM2 = oppMoveHistory[numMoves - 2];
    int ownM3 = ownMoveHistory[numMoves - 1];
    int oppM3 = oppMoveHistory[numMoves - 1];
    
    int prevKey = stateKey(ownM1, oppM1, ownM2, oppM2);
    
    // Now determine how to play based on last two moves
    int key = stateKey(ownM2, oppM2, ownM3, oppM3);

    if ((played & key) == 0) {
      Serial.print("Random as "); Serial.print(key); Serial.println(" not played yet");
      return random(2);
    }

    if ((repeat & key) == 0) {
      Serial.print("Random as "); Serial.print(key); Serial.println(" not repeated");
      return random(2);
    }

    // If we get here we know the sequence has been played before, and that it has been repeated.
    // Use prev to find the move.
    int oppPrev = oppM3;
    Serial.println("Using previous");
    return (prev & key) == 0 ? different(oppPrev) : oppPrev;
  }
  void opponentsMove(int m) {
    // update history
    oppMoveHistory[numMoves] = m;
    numMoves++;
    
    if (numMoves < 3) {
      return;
    }
    
    // update state based on opp's last move
    
    int ownM1 = ownMoveHistory[numMoves - 3];
    int oppM1 = oppMoveHistory[numMoves - 3];
    int ownM2 = ownMoveHistory[numMoves - 2];
    int oppM2 = oppMoveHistory[numMoves - 2];
    int ownM3 = ownMoveHistory[numMoves - 1];
    int oppM3 = oppMoveHistory[numMoves - 1];
    
    int prevKey = stateKey(ownM1, oppM1, ownM2, oppM2);
    int value = oppM2 == oppM3 ? 1 : 0; // opp then played same move
    
    if ((played & prevKey) == 0) { // not seen this before
      played |= prevKey; // set to 1
      if (value == 0) {
        prev &= ~prevKey; // set to 0
      } else {
        prev |= prevKey; // set to 1
      }
      // repeat stays as 0
    } else { // seen before
      int prevValue = (prev & prevKey) == 0 ? 0 : 1;
      if (prevValue == value) {
        // prev stays unchanged
        repeat |= prevKey; // set to 1
      } else {
        if (value == 0) {
            prev &= ~prevKey; // set to 0
        } else {
            prev |= prevKey; // set to 1
        }
        repeat &= ~prevKey; // set to 0
      }
    }
  }
  
  static int stateKey(int ownM1, int oppM1, int ownM2, int oppM2) {
    int key = 0;
    if (oppM1 != ownM1) { // opp won penultimate
      key |= 1 << 2;
    }
    if (oppM1 == oppM2) { // opp then played same move
      key |= 1 << 1;
    }
    if (oppM2 != ownM2) { // opp then won
      key |= 1;
    }
    return 1 << key;
  }
  
  static int different(int m) {
    return 1 - m;
  }
};

const boolean DEBUG = false;
const int NUM_MOVES = 30;
const boolean SHOW_MACHINE_CHOICE_EARLY = false; // true = original rules as described by Shannon 

const int BUTTON_PIN = 8;
const int LEFT_BUTTON_PIN = 9;
const int RIGHT_BUTTON_PIN = 10;
const int LEFT_LED_PIN = 12;
const int RIGHT_LED_PIN = 11;

// state machine
const int START_STATE = 0;
const int WAIT_FOR_MACHINE_MOVE_STATE = 1;
const int WAIT_FOR_HUMAN_MOVE_STATE = 2;
const int END_STATE = 3;

const int LEFT = 0;
const int RIGHT = 1;

DebouncedButton button(BUTTON_PIN);
DebouncedButton leftButton(LEFT_BUTTON_PIN);
DebouncedButton rightButton(RIGHT_BUTTON_PIN);

Adafruit_7segment matrix = Adafruit_7segment();

ShannonStrategy machine;

int state = START_STATE;

int machineMove;
int humanMove;
int machineScore = 0;
int humanScore = 0;

void setup() {
  pinMode(BUTTON_PIN, INPUT);
  pinMode(LEFT_BUTTON_PIN, INPUT);
  pinMode(RIGHT_BUTTON_PIN, INPUT);
  pinMode(LEFT_LED_PIN, OUTPUT);
  pinMode(RIGHT_LED_PIN, OUTPUT);
  Serial.begin(9600);
  Serial.println("Starting...");
  
  randomSeed(analogRead(0));
  
  matrix.begin(0x70);
  matrix.setBrightness(2);
  // write dashes to the display
  matrix.print(10000, DEC);
  matrix.writeDisplay();
  delay(500);
}

void loop() {
  switch (state) {
    case START_STATE:
      machineScore = 0;
      humanScore = 0;
      printScore();
      state = WAIT_FOR_MACHINE_MOVE_STATE;
      break;
    case WAIT_FOR_MACHINE_MOVE_STATE:
      if (button.buttonPressed()) {
        debug("Button pressed");
        if (!SHOW_MACHINE_CHOICE_EARLY) {
          hideMachineMove();
        }
        playMachineMove();
        state = WAIT_FOR_HUMAN_MOVE_STATE;
      }        
      break;
    case WAIT_FOR_HUMAN_MOVE_STATE:
      if (leftButton.buttonPressed()) {
        debug("Left button pressed");
        if (!SHOW_MACHINE_CHOICE_EARLY) {
          showMachineMove();
        }
        Serial.println("L");
        humanMove = LEFT;
        evaluateMove();
        if ((machineScore + humanScore) < NUM_MOVES) {
          state = WAIT_FOR_MACHINE_MOVE_STATE;
        } else {
          finishGame();
          state = END_STATE;
        }
      } else if (rightButton.buttonPressed()) {
        debug("Right button pressed");
        if (!SHOW_MACHINE_CHOICE_EARLY) {
          showMachineMove();
        }
        Serial.println("R");
        humanMove = RIGHT;
        evaluateMove();
        if ((machineScore + humanScore) < NUM_MOVES) {
          state = WAIT_FOR_MACHINE_MOVE_STATE;
        } else {
          finishGame();
          state = END_STATE;
        }
      } 
      break;
    case END_STATE:
      if (leftButton.buttonPressed()) {
        state = START_STATE;
      }
      break;
    default:
      break;
  }
}

void playMachineMove() {
  machineMove = machine.makeMove();
  if (SHOW_MACHINE_CHOICE_EARLY) {
    showMachineMove();
  }
}

void showMachineMove() {
  if (machineMove == LEFT) {
      digitalWrite(LEFT_LED_PIN, HIGH);
      Serial.print("L ");
  } else {
      digitalWrite(RIGHT_LED_PIN, HIGH);
      Serial.print("R ");
  }
}

void hideMachineMove() {
  digitalWrite(LEFT_LED_PIN, LOW);
  digitalWrite(RIGHT_LED_PIN, LOW);
}

void evaluateMove() {
  machine.opponentsMove(humanMove);
  if (machineMove == humanMove) {
    machineScore++;
  } else {
    humanScore++;
  }
  printScore();
  if (SHOW_MACHINE_CHOICE_EARLY) {
    hideMachineMove();
  }
}

void printScore() {
  Serial.print(machineScore);
  Serial.print(":");
  Serial.println(humanScore);
  matrix.clear();
  matrix.blinkRate(0);
  if (machineScore >= 10) {
    matrix.writeDigitNum(0, (machineScore / 10) % 10, false);
  }
  matrix.writeDigitNum(1, machineScore % 10, false);
  if (humanScore >= 10) {
    matrix.writeDigitNum(3, (humanScore / 10) % 10, false);
  }
  matrix.writeDigitNum(4, humanScore % 10, false);
  matrix.drawColon(true);
  matrix.writeDisplay();
}

void finishGame() {
  debug("Game over");
//  Serial.print("Non random wins: ");
//  Serial.println(nonRandomWins);
//  Serial.print("Non random losses: ");
//  Serial.println(nonRandomLosses);
  matrix.blinkRate(1);
}

void debug(String msg) {
  if (DEBUG) Serial.println(msg);
}
