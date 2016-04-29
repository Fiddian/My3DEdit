/******************

v1.1 19.04.2016
trigger drop value lowered to goes to black quicker 
tried brightening cylons
brethe lower value raised from 2 to 10
max twinkle brightness reduced from 3/4 to 1/2

Version 1.0 (release) 18.04.2016.
Drops breathe for Cylons in baseanimation
Reduces min brightness will turn down to. Default value 64 18.04.2016

******************/
 
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_CAP1188.h>
#include <FastLED.h>

#define COLOR_ORDER RGB
#define LED_TYPE APA102
#define CAP1188_SENSITIVITY 0x1F

Adafruit_CAP1188 cap = Adafruit_CAP1188(); // Use I2C for the CAP1188 cap sensor, no reset pin

const int NUM_LEDS = 404;           // total number of LEDs in string
const int SECTION_LENGTH = NUM_LEDS / 4; // leds are divided into four sections
const int FRAMES_PER_SECOND = 100;  // 100 default
const int MAX_WHITE = 765;          // brightest in extended white system (3 * 255)
const int MIN_WHITE = 10;            // dimmest non-zero in extended white system (3 * 255) DEFAULT WAS '2' in v1.0 (10 looks good)

const int MIN_BRIGHTNESS_SETTING = 64; // used to set minimum value brigtness can be turned down to using pot
const int MAX_BRIGHTNESS_SETTING = 255;
int maxBrightness = MAX_BRIGHTNESS_SETTING; // variable for overall brightness
int potentiometerValue = 1024;      // value to read from potentiometer to control brightness


CRGB leds[NUM_LEDS];

// mode

enum {
  BASE_MODE,
  TRIGGERING_MODE,
  CYLONS_MODE
};

int mode = CYLONS_MODE;

// breathe

const float BREATHE_PERIOD_SECONDS = 20.0; //############# 50.0 quite nice or whatever value
const int BREATHE_PERIOD_FRAMES = BREATHE_PERIOD_SECONDS * FRAMES_PER_SECOND;
const int BREATHE_MAX_WHITE = MAX_WHITE / 3;
const int BREATHE_MIN_WHITE = MIN_WHITE;

int breathePhaseFrame = 0;
int breatheWhite = 0;


// twinkles

struct Twinkle {
  int waitFrames; // wait this number of frames before showing
  int position;   // index into LEDs
  int whiteness;  // 0..765
};

const int NUM_TWINKLES = 6;
const int TWINKLE_MAX_WHITE = (MAX_WHITE) / 2; //default v1.0 TWINKLE_MAX_WHITE = (MAX_WHITE * 3) / 4;
const int TWINKLE_WHITE_DROP = 1;
const int TWINKLE_MAX_WAIT_FRAMES = 5 * FRAMES_PER_SECOND;

Twinkle twinkles[NUM_TWINKLES];


// triggering

enum {
  DROP_TRIGGER_MODE,
  RISE_TRIGGER_MODE,
  PRE_BREATHE_TRIGGER_MODE,
  BREATHE_TRIGGER_MODE
};

const int TRIGGER_DROP_WHITE_DELTA = 20; // whiteness drop per frame
const int TRIGGER_PRE_BREATHE_DROP_WHITE_DELTA = 4; // drop to breathe max per frame
const int TRIGGER_DROP_FRAMES = FRAMES_PER_SECOND/6; //speed of trigger drop v1.0 standard was; const int TRIGGER_DROP_FRAMES = FRAMES_PER_SECOND / 2;
const int TRIGGER_RISE_FRAMES = FRAMES_PER_SECOND * 2;
const int TRIGGER_BREATHE_FRAMES = FRAMES_PER_SECOND * 10; //use to extend blocking trigger

int triggerMode = DROP_TRIGGER_MODE;
int triggerPhaseFrame = 0;


// cylons
struct Cylon {
	int position;         // pixel position of front of cylon within section
	int phase;            // counts sub-pixel positions
	int whitenessDelta;   // amount to add to background image
};

const int NUM_CYLONS = 8;         // and each one appears in four places on LED strip
const int NUM_STANDARD_CYLONS = 3; // must be less than NUM_CYLONS
const int CYLON_STEP_FRAMES = 24;  // number of sub-pixels per step
const int CYLON_LENGTH = 30;       // in pixels, length includes one end pixel
const int CYLON_STANDARD_FOLLOW_DISTANCE = CYLON_LENGTH + 50;
const int CYLON_GAP_INCREASE = 20; // each cylon after initial standard set is this amount further away
const int CYLON_STANDARD_WHITENESS_DELTA = MAX_WHITE; // brightest cylon increases image by this amount
const int CYLON_MIN_WHITENESS_DELTA = 8; // dimmest cylon effect

Cylon cylons[NUM_CYLONS];
CRGB cylonPixelDeltaRGB;


// main

void setup() {
  delay(1000);
  //Serial.begin(115200);
  //Serial.println("\nversion 1.0");
  LEDS.addLeds<LED_TYPE, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(maxBrightness);
 
  if (!cap.begin()) {                 // Initialize the sensor
    //Serial.println("CAP1188 not found");
    while (1);
  }
  //Serial.println("CAP1188 found!");
  
  //CAP sensitivity settings. Decrease sensitivity a little - default is 0x2F (32x) per datasheet
  cap.writeRegister(CAP1188_SENSITIVITY, 0x0F);  // 128x sensitivity (works for proxinity)
  //cap.writeRegister(CAP1188_SENSITIVITY, 0x1F);  // 64x sensitivity
  //cap.writeRegister(CAP1188_SENSITIVITY, 0x2F);  // 32x sensitivity
  //cap.writeRegister(CAP1188_SENSITIVITY, 0x3F);  // 16x sensitivity
  //cap.writeRegister(CAP1188_SENSITIVITY, 0x4F);  // 8x  sensitivity
  //cap.writeRegister(CAP1188_SENSITIVITY, 0x5F);  // 4x  sensitivity
  //cap.writeRegister(CAP1188_SENSITIVITY, 0x6F);  // 2x  sensitivity THIS SEEMS TO WORK THE BEST FOR 3.5" plate sensors
  //cap.writeRegister(CAP1188_SENSITIVITY, 0x7F);  // 1x  sensitivity
  ////Serial.print("Sensitivity: 0x");
  ////Serial.println(cap.readRegister(CAP1188_SENSITIVITY), HEX);

  FastLED.setDither(0);
  
  FastLED.clear();
  fill_solid(leds, NUM_LEDS, CRGB::White);
  
  initTwinkles();
  startCylons();
}


void loop() {
  int potentiometerValue = analogRead(A0);       // read the input on analog pin
  maxBrightness = map(potentiometerValue, 0, 1023, MIN_BRIGHTNESS_SETTING, MAX_BRIGHTNESS_SETTING);
  FastLED.setBrightness(maxBrightness); // adjust master brightness control

  uint8_t touched = cap.touched(); //test if CAP1188 is touched, check even if not using in order to keep buffer empty
  if (touched && mode != TRIGGERING_MODE) {
    startTriggering();
  }

  update();
  draw();

  FastLED.show();
  delay(1000 / FRAMES_PER_SECOND);
}


void update() {
  if (mode == TRIGGERING_MODE) {
    updateTriggering();
  }
  else {
    updateBreathe();
    updateTwinkles();
    if (mode == CYLONS_MODE) {
      updateCylons();
    }
  }
}


void draw() {
  drawBreathe();
  if (mode != TRIGGERING_MODE) {
    drawTwinkles();
    if (mode == CYLONS_MODE) {
      drawCylons();
    }
  }
}


// utility


void setMode(int m) {
  mode = m;
  //Serial.print("Mode ");
  //Serial.println(mode);
}


// create LED value from 0 to 765 range (0 to 3*255)
CRGB extendedWhite(int upTo765) {
  int high = 255, medium = 255, low = 255; // these instead of r g b
  
  if (upTo765 < 255 * 3) {
    if (upTo765  > 255 * 2) {
      high = upTo765 - 255 * 2;
    }
    else if (upTo765 > 255) {
      high = 0;
      medium = upTo765 - 255;
    }
    else {
      high = medium = 0;
      low = upTo765;
    }
  }
  // else is full on already
  
  return CRGB(high, medium, low);
}


// breathe


void updateBreathe() {
  // phase as 0 to 2π
  float phase = (breathePhaseFrame * 2.0 * PI) / (float) BREATHE_PERIOD_FRAMES;

  // brightness as 0 to 1.0
  float brightness = (cos(phase) + 1.0) / 2.0;
  
  // as BREATHE_MIN_WHITE to BREATHE_MAX_WHITE
  breatheWhite = BREATHE_MIN_WHITE + brightness * (BREATHE_MAX_WHITE - BREATHE_MIN_WHITE);

  // update phase
  if (breathePhaseFrame < BREATHE_PERIOD_FRAMES - 1) {
    breathePhaseFrame++;
  }
  else {
    breathePhaseFrame = 0;
  }
}


void drawBreathe() {
  fill_solid(leds, NUM_LEDS, extendedWhite(breatheWhite));
}


// twinkles


void initTwinkles() {
  for (int i = 0; i < NUM_TWINKLES; i++) {
    freshTwinkle(&twinkles[i]);
  }
}


void updateTwinkles() {
  for (int i = 0; i < NUM_TWINKLES; i++) {
    updateTwinkle(&twinkles[i]);
  }
}


void drawTwinkles() {
  for (int i = 0; i < NUM_TWINKLES; i++) {
    drawTwinkle(&twinkles[i]);
  }
}


void freshTwinkle(Twinkle *t) {
  t->waitFrames = random16(TWINKLE_MAX_WAIT_FRAMES);
  t->position = random16(NUM_LEDS - 1);
  t->whiteness = TWINKLE_MAX_WHITE;
}


void updateTwinkle(Twinkle *t) {
  if (t->waitFrames > 0) {
  	t->waitFrames--;
  } else {
    if (t->whiteness > breatheWhite) {
      // reduce whiteness
      t->whiteness -= TWINKLE_WHITE_DROP;
    }
    else {
      // pick somewhere else to twinkle
      freshTwinkle(t);
    }
  }
}


void drawTwinkle(Twinkle *t) {
  if (t->waitFrames == 0) {
    leds[t->position] = extendedWhite(t->whiteness);
  }
}


// cylons


void startCylons() {
  setMode(CYLONS_MODE);

	int at = 0;
	int whiteness = CYLON_STANDARD_WHITENESS_DELTA;
	
	int i = 0;
	// standard ones
	for ( ; i < NUM_STANDARD_CYLONS; i++) {
		freshCylon(&cylons[i], at, whiteness);
		at -= CYLON_STANDARD_FOLLOW_DISTANCE;
	}
	
	// trailing ones: increasingly greater separation and dimmer
	int extraGap = CYLON_GAP_INCREASE;
	at -= extraGap;
	for( ; i < NUM_CYLONS; i++) {
		whiteness = map(i, NUM_STANDARD_CYLONS - 1, NUM_CYLONS - 1, CYLON_STANDARD_WHITENESS_DELTA, CYLON_MIN_WHITENESS_DELTA);
		freshCylon(&cylons[i], at, whiteness);
		extraGap += CYLON_GAP_INCREASE;
		at -= CYLON_STANDARD_FOLLOW_DISTANCE + extraGap;
	} 
}


void freshCylon(Cylon *c, int position, int whitenessDelta) {
	c->position = position;
	c->whitenessDelta = whitenessDelta;
	c->phase = 0;
  //Serial.print("Cy white ");
  //Serial.println(whitenessDelta);
}


void updateCylons() {
	bool hasEnded = false;
	
  for (int i = 0; i < NUM_CYLONS; i++) {
  	hasEnded = updateCylon(&cylons[i]);
  }
  
  if (hasEnded) { // last cylon has ended
  	//setMode(BASE_MODE); // end cylon mode
   startCylons(); // restart
  }
}
  

void drawCylons() {
  for (int i = 0; i < NUM_CYLONS; i++) {
    drawCylon(&cylons[i]);
  }
}


bool updateCylon(Cylon *c) {
  if (c->phase < CYLON_STEP_FRAMES - 1) {
    c->phase++;
  }
  else {
    c->phase = 0;
    c->position++;
    if (c->position > SECTION_LENGTH + CYLON_LENGTH) { // we're off the end of the section
      return true; // signal cylon has ended
    }
  } 
  return false; // signal cylon is in progress
}
  

void drawCylon(Cylon *c) {
	if (c->position < 0) {
		return; // hasn't yet entered the display section
	}
	
	int firstPixelWhiteness = map(c->phase, 0, CYLON_STEP_FRAMES - 1, 0, c->whitenessDelta);
	int lastPixelWhiteness = c->whitenessDelta - firstPixelWhiteness;
	
	cylonPixelDeltaRGB = extendedWhite(firstPixelWhiteness); // global var to save multiply-passing as param
	drawCylonPixelSet(c->position);
	
	cylonPixelDeltaRGB = extendedWhite(c->whitenessDelta);
	for (int i = 1; i < CYLON_LENGTH; i++) {
		drawCylonPixelSet(c->position - i);
	}
	
	cylonPixelDeltaRGB = extendedWhite(lastPixelWhiteness);
	drawCylonPixelSet(c->position - CYLON_LENGTH);
}


void drawCylonPixelSet(int position) {
	if (position >= 0 && position < SECTION_LENGTH) { // if in the section
		drawCylonPixel(position);                      // top1, forward
		drawCylonPixel(SECTION_LENGTH * 2 - 1 - position); // bottom1, backward
		drawCylonPixel(SECTION_LENGTH * 2 + position); // top2, forward
		drawCylonPixel(SECTION_LENGTH * 4 - 1 - position); // bottom2, backward
	}
}


void drawCylonPixel(int position) {
	if (position >= 0 && position < NUM_LEDS) { // if in the display
		leds[position] += cylonPixelDeltaRGB; // add in whiteness to current image
	}
}


// triggering


void startTriggering() {
  setMode(TRIGGERING_MODE);
  triggerMode = DROP_TRIGGER_MODE;
  triggerPhaseFrame = 0;
}


void updateTriggering() {
  switch (triggerMode) {
    case DROP_TRIGGER_MODE:
      breatheWhite -= TRIGGER_DROP_WHITE_DELTA;
      if (breatheWhite < MIN_WHITE) {
        breatheWhite = MIN_WHITE;
      }
      if (triggerPhaseFrame < TRIGGER_DROP_FRAMES) {
        triggerPhaseFrame++;
      }
      else {
        triggerMode = RISE_TRIGGER_MODE;
        triggerPhaseFrame = 0;
      }
      break;
      
    case RISE_TRIGGER_MODE:
      breatheWhite = map(triggerPhaseFrame, 0, TRIGGER_RISE_FRAMES, MIN_WHITE, MAX_WHITE);
      if (triggerPhaseFrame < TRIGGER_RISE_FRAMES) {
        triggerPhaseFrame++;
      }
      else {
        triggerMode = PRE_BREATHE_TRIGGER_MODE;
      }
      break;

    case PRE_BREATHE_TRIGGER_MODE:
      if (breatheWhite > BREATHE_MAX_WHITE) {
        breatheWhite -= TRIGGER_PRE_BREATHE_DROP_WHITE_DELTA;
      }
      else {
        triggerMode = BREATHE_TRIGGER_MODE;
        triggerPhaseFrame = 0;
        breathePhaseFrame = 0;
      }
      break;
      
    case BREATHE_TRIGGER_MODE:
      updateBreathe();
      if (triggerPhaseFrame < TRIGGER_BREATHE_FRAMES) {
        triggerPhaseFrame++;
      }
      else {
        startCylons();
      }
      break;
  }
}
