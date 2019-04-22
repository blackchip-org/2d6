#include <Entropy.h>

#define VERSION "1"

// Updated each loop with the current time
unsigned long now = millis();

// Mode sets the number of dice to roll
#define MODE_2D6 0
#define MODE_1D6 1
byte mode = MODE_2D6;

// Machine is in one of three states:
#define STATE_MODE 0 // Mode select
#define STATE_SHOW 1 // Show dice values
#define STATE_ROLL 2 // Show rolling animation
byte state = STATE_MODE;

// LED pin mapping
//
//     555
//    6   4
//    6   4
//     777
//    0   2
//    0   2
//     111  3
//
// The diagram above shows the seven segements (and dot) of the LED and
// its index in the following array. Use the array to find which
// Arduino pin to use. For example, the middle horizontal bar is at index 7,
// uses pin 9 on the Arduino which is connected to pin 10 on the led
// display.
//
byte ledPins[] = {
	// bottom pins, left to right
  2, // index 0, arduino 2, led 1
  3, // index 1, arduino 3, led 2
     //                     led 3 - ground
  4, // index 2, arduino 4, led 4
  5, // index 3, arduino 5, led 5

  // top pins, right to left
  6, // index 4, arduino 6, led 6
  7, // index 5, arduino 7, led 7
     //                     led 8 - ground
  8, // index 6, arduino 8, led 9
  9, // index 7, arduino 9, led 10
};

// Transistor pin mapping
byte transPins[] = {
  10,	// Left LED display
  11  // Right LED display
};

// Button pin mapping
byte rollButtonPin = 12;
byte modeButtonPin = 13;

// LED segment table for numbers. Row is the number to display.
// Column is the LED segment index.
byte numbers[][8] = {
  {1, 1, 1, 0, 1, 1, 1, 0}, // 0
  {0, 0, 1, 0, 1, 0, 0, 0}, // 1
  {1, 1, 0, 0, 1, 1, 0, 1}, // 2
  {0, 1, 1, 0, 1, 1, 0, 1}, // 3
  {0, 0, 1, 0, 1, 0, 1, 1}, // 4
  {0, 1, 1, 0, 0, 1, 1, 1}, // 5
  {1, 1, 1, 0, 0, 1, 1, 1}, // 6
  {0, 0, 1, 0, 1, 1, 0, 0}, // 7
  {1, 1, 1, 0, 1, 1, 1, 1}, // 8
  {0, 1, 1, 0, 1, 1, 1, 1}, // 9
};

// Other segment mappings
byte dash[] =    {0, 0, 0, 0, 0, 0, 0, 1};
byte blank[] =   {0, 0, 0, 0, 0, 0, 0, 0};
byte letterD[] = {1, 1, 1, 0, 1, 0, 0, 1};

// Display for each LED. First index is the left LED, second index
// is the right LED. Should contain a pointer to an array of
// eight segment values.
byte *leds[2] = {blank, blank};

// Only one LED is active at a time but swapped quickly enough
// that it looks like both LEDs are on. Index to the LED that
// is active
byte activeLed = 0;

// Last time the active LED was swapped
unsigned long ledLastSwap = now;

// Time in milliseconds to show each LED display before swapping
#define LED_SWAP_INTERVAL 5

void updateDisplay() {
	// Time to swap the displays?
  if (!(now - ledLastSwap > LED_SWAP_INTERVAL)) {
    return;
  }
  ledLastSwap = now;

  // Before switching the transistors, set all LED lines
  // to low. If not done, faint outlines of the "other"
  // LED display can been seen. Is there a better way?
  for (int i = 0; i < 8; i++) {
    digitalWrite(ledPins[i], 0);
  }

  // Disable the current transistor, and enable the other
  digitalWrite(transPins[activeLed], 0);
  activeLed = (activeLed + 1) % 2;
  digitalWrite(transPins[activeLed], 1);

  // Update all of the LED segments
  byte *segments = leds[activeLed];
  for (int i = 0; i < 8; i++) {
    digitalWrite(ledPins[i], segments[i]);
  }
}

// When the roll button is pressed, a "rolling" animation
// is shown for a random amount of time. These are the
// segments to display for each animation frame.
byte rollAnim[6][8] = {
  {1, 0, 0, 0, 0, 0, 1, 0},
  {0, 0, 0, 0, 1, 1, 0, 0},
  {0, 0, 1, 0, 1, 0, 0, 0},
  {0, 1, 1, 0, 0, 0, 0, 0},
  {1, 1, 0, 0, 0, 0, 0, 0},
};

// Update the animation after this time has elapsed, in milliseconds
#define ANIM_FRAME_RATE 50

// States shown during the roll, each die is independent.
#define DIE_ROLL  0 // Show the rolling animation
#define DIE_PAUSE 1 // Show a "dash" as a pause after the roll
#define DIE_STOP  2 // Show the number on the die after the pause
byte dieState[2] = {DIE_STOP, DIE_STOP};

// Incremented on each animation frame, reset to zero when starting animation
short animFrame = 0;

// For each die, the frame number where the animation should change to
// the paused state
short diePauseFrame[2] = {0, 0};

// For each die, the frame number where the animation should stop and show
// the die value.
short dieStopFrame[2] = {0, 0};

// Last time that the animation was updated
unsigned long animLastUpdate = now;

// The rolling animation time is randomly chosen between these two times
#define MIN_ROLL_FRAMES 1000 / ANIM_FRAME_RATE
#define MAX_ROLL_FRAMES 3000 / ANIM_FRAME_RATE

// Number of frames to between the rolling animation and the display
// of the die result
#define PAUSE_FRAMES 500 / ANIM_FRAME_RATE

void startRoll() {
  animFrame = 0;
  state = STATE_ROLL;
  for (byte i = 0; i < 2; i++) {
    // When only rolling one die, the left display should be blank
    if (mode == MODE_1D6 && i == 0) {
      dieState[i] = DIE_STOP;
      leds[i] = blank;
    } else {
      dieState[i] = DIE_ROLL;
      diePauseFrame[i] = Entropy.random(MIN_ROLL_FRAMES, MAX_ROLL_FRAMES);
      dieStopFrame[i] = diePauseFrame[i] + PAUSE_FRAMES;
    }
  }
}

void updateRoll() {
  // Time to update the animation?
  if (!(now - animLastUpdate > ANIM_FRAME_RATE)) {
    return;
  }

  animLastUpdate = now;
  animFrame++;
  short animPos = animFrame % 5;

  for (byte i = 0; i < 2; i++) {
    if (dieState[i] == DIE_ROLL && animFrame > diePauseFrame[i]) {
      dieState[i] = DIE_PAUSE;
      leds[i] = dash;
    } else if (dieState[i] == DIE_PAUSE && animFrame > dieStopFrame[i]) {
      dieState[i] = DIE_STOP;
      byte n = Entropy.random(5) + 1;
      leds[i] = numbers[n];
    } else if (dieState[i] == DIE_ROLL) {
      leds[i] = rollAnim[animPos];
    }
    if (dieState[0] == DIE_STOP && dieState[1] == DIE_STOP) {
        state = STATE_SHOW;
    }
  }
}

void nextMode() {
    mode = (mode + 1) % 2;
    updateMode();
}

void updateMode() {
    if (mode == MODE_1D6) {
        leds[0] = numbers[1];
        leds[1] = letterD;
    } else if (mode == MODE_2D6) {
        leds[0] = numbers[2];
        leds[1] = letterD;
    }
}

// Keep track of the last known state of the button to know
// when there is a change. 0 = closed, 1 = open
byte rollButton = 1;
byte modeButton = 1;

void checkButton() {
  byte btn = digitalRead(rollButtonPin);
  if (rollButton != btn) {
    rollButton = btn;
    // Don't allow another roll to start while a roll is active
    if (btn == 0 && state != STATE_ROLL) {
      startRoll();
    }
  }
  btn = digitalRead(modeButtonPin);
  if (modeButton != btn) {
    modeButton = btn;
    // Don't allow a mode change while a roll is active
    if (btn == 0 && state != STATE_ROLL) {
      nextMode();
    }
  }
}

void setup() {
    Serial.begin(9600);
		Serial.println("\n2d6 version " VERSION);

    for (int i = 0; i < 8; i++) {
        pinMode(ledPins[i], OUTPUT);
    }
    pinMode(transPins[0], OUTPUT);
    pinMode(transPins[0], OUTPUT);
    pinMode(rollButtonPin, INPUT_PULLUP);
    pinMode(modeButtonPin, INPUT_PULLUP);

    updateMode();
    Entropy.initialize();
}

void loop() {
    now = millis();
    checkButton();
    if (state == STATE_ROLL) {
        updateRoll();
    }
    updateDisplay();
}
