/*
 Reads and strobes through the MSGEQ7's outputs (provides the signal amplitude at 7 different frequency bands)
 Uses approach 1 from 'Beat Detection Algorithms' (Frederic Patin)
 to detect beats. Tells the computer when a beat is detected, and on which frequency band.

 Low pass filters measurements from each of the 7 channels. Takes the average of these measurements for the last
 second and compares against the newest filtered measurement. If the new measurement is greater than the average*C
 (for some constant C), then the new measurement is considered to be a beat.
 If a beat is detected, the value of the beat is transmitted as the program's output. Otherwise, the output is the
 low-pass filtered (exponential moving average) measurements.

 Future changes: Sustained notes? Should a sustained note/beat keep up the brightness of that segment? (it kind of appears to do this already)

 V2 adds support for 2 channels, and the front LEDs
*/

#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif

#define LEFT_PIN 9
#define RIGHT_PIN 7
#define BACK_PIN 8
#define FRONT_PIN 6
// Parameter 1 = number of pixels in strip
// Parameter 2 = Arduino pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
Adafruit_NeoPixel leftStrip = Adafruit_NeoPixel(21, LEFT_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel rightStrip = Adafruit_NeoPixel(21, RIGHT_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel backStrip = Adafruit_NeoPixel(20, BACK_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel frontStrip = Adafruit_NeoPixel(2, FRONT_PIN, NEO_GRB + NEO_KHZ800);
// With the neopixel library, each LED takes 3 bytes of RAM.
// The neopixel library lets us do parallel LED strips (whereas FastLED doesn't on the pro mini)
// IMPORTANT: To reduce NeoPixel burnout risk, add 1000 uF capacitor across
// pixel power leads, add 300 - 500 Ohm resistor on first pixel's data input
// and minimize distance between Arduino and first pixel.  Avoid connecting
// on a live circuit...if you must, connect GND first.

// These constants won't change.  They're used to give names
// to the pins used:
const int swPin = A3;  // Analog input pin that the 4-position switch is attached to
const int outPinR = A0;  // Analog input pin that the MSGEQ7 output is attached to
int strobePinR  = 12;  // Strobe(4) Pin on the MSGEQ7
int resetPinR   = 11;  // Reset (7) Pin on the MSGEQ7
const int outPinL = A1;  // Analog input pin that the MSGEQ7 output is attached to
int strobePinL  = 13;  // Strobe(4) Pin on the MSGEQ7
int resetPinL   = 10;  // Reset (7) Pin on the MSGEQ7

// Helps us pick the max beat in a time period to use for the front LED
unsigned int frontBeatPower[3];  // Holds low pass filtered brightness values for R, G, and B

// For low pass filtering (boxcar filter)
unsigned int levelL[7][8];      // An array to hold the values from the 7 frequency bands
unsigned int totalLevelL[7];      // An array to hold the summed values from the 7 frequency bands
unsigned int levelR[7][8];      // An array to hold the values from the 7 frequency bands
unsigned int totalLevelR[7];      // An array to hold the summed values from the 7 frequency bands
unsigned char oldIndex, newIndex; // What's the oldest measurement in the array? Where does the new one go?

// To store the average 'energy' over the last ~1 second
unsigned int samplesL[7][64]; // Holds the low-pass filtered samples
unsigned int totalSamplesL[7]; // Holds the sums of 64 samples
unsigned int avgSampleL[7]; // Holds the averages of the last 64 samples
unsigned char endIndexL, startIndexL; // What's the oldest sample in the array? Where does the new one go?
unsigned int samplesR[7][64]; // Holds the low-pass filtered samples
unsigned int totalSamplesR[7]; // Holds the sums of 64 samples
unsigned int avgSampleR[7]; // Holds the averages of the last 64 samples
unsigned char endIndexR, startIndexR; // What's the oldest sample in the array? Where does the new one go?

const char numBeats = 7;  // How many channels/frequency ranges are we detecting beats on?
unsigned int outputL[numBeats]; // Value the program will output for a channel
unsigned int outputR[numBeats]; // Value the program will output for a channel
boolean beatL[7]; // Track whether or not there was a beat
boolean beatR[7]; // Track whether or not there was a beat
unsigned int thresholdL[7]; // For separating out noise
unsigned int thresholdR[7]; // For separating out noise
float CL[7]; // Multiplier for the 'beat threshold'
float CR[7]; // Multiplier for the 'beat threshold'
float varL[7]; // Variance of the measurements
float varR[7]; // Variance of the measurements
signed int temp; // Used in calculating variance
float faderL[7]; // Fade out if there hasn't been a beat in a while
float faderR[7]; // Fade out if there hasn't been a beat in a while
float faderAdjL[7]; // The rate at which the fader approaches zero
float faderAdjR[7]; // The rate at which the fader approaches zero
int beatTimeL[7]; // A count of the time between beats. Used to determine faderAdj
int beatTimeR[7]; // A count of the time between beats. Used to determine faderAdj

void setup() {
  leftStrip.begin();
  rightStrip.begin();
  backStrip.begin();
  frontStrip.begin();
  leftStrip.show(); // Initialize all pixels to 'off'
  rightStrip.show(); // Initialize all pixels to 'off'
  backStrip.show(); // Initialize all pixels to 'off'
  frontStrip.show(); // Initialize all pixels to 'off'
  
  Serial.begin(115200);
  pinMode      (strobePinL, OUTPUT);
  pinMode      (resetPinL,  OUTPUT);
  pinMode      (strobePinR, OUTPUT);
  pinMode      (resetPinR,  OUTPUT);
    // Create an initial state for our pins
  digitalWrite (resetPinL,  LOW);
  digitalWrite (strobePinL, LOW);
  digitalWrite (resetPinR,  LOW);
  digitalWrite (strobePinR, LOW);
  delay        (1);
 
  // Reset the MSGEQ7 as per the datasheet timing diagram
  digitalWrite (resetPinL,  HIGH);
  digitalWrite (resetPinR,  HIGH);
  delay        (1);
  digitalWrite (resetPinL,  LOW);
  digitalWrite (resetPinR,  LOW);
  digitalWrite (strobePinL, HIGH);
  digitalWrite (strobePinR, HIGH); 
  delay        (1);

  // Set up the arrays/circular buffers
  newIndex = 0;
  oldIndex = 1;
  for (int i = 0; i < 7; i++) {
    totalLevelL[i] = 0;
    totalSamplesL[i] = 0;
    avgSampleL[i] = 0;
    totalLevelR[i] = 0;
    totalSamplesR[i] = 0;
    avgSampleR[i] = 0;
    for(int j = 0; j < 64; j++) {
      samplesL[i][j] = 0;
      samplesR[i][j] = 0;
    }
    for (int j = 0; j < 8; j++) {
      levelL[i][j] = 0;
      levelR[i][j] = 0;
    }
    CL[i] = 1.3;
    thresholdL[i] = 0;
    faderL[i] = 1;
    faderAdjL[i] = 0.1;
    beatTimeL[i] = 0;
    CR[i] = 1.3;
    thresholdR[i] = 0;
    faderR[i] = 1;
    faderAdjR[i] = 0.1;
    beatTimeR[i] = 0;
  }
}

void loop() { // Should take about 2.5 ms, 400 Hz
  // Cycle through each frequency band by pulsing the strobe. Should take ~200us per band (1.4 ms total)
  for (int i = 0; i < 7; i++) {
    digitalWrite(strobePinL, LOW); // Switch to the next band (MSGEQ7)
    digitalWrite(strobePinR, LOW); // Switch to the next band (MSGEQ7)
    delayMicroseconds(100); // Delay necessary due to timing diagram
    levelL[i][newIndex] = analogRead(outPinL);
    totalLeveLl[i] -= level[i][oldIndex]; // Clear out the old value
    totalLevelL[i] += level[i][newIndex]; // Add in the new value
    levelR[i][newIndex] = analogRead(outPinR);
    totalLevelR[i] -= level[i][oldIndex]; // Clear out the old value
    totalLevelR[i] += level[i][newIndex]; // Add in the new value
    digitalWrite(strobePinL, HIGH);
    digitalWrite(strobePinR, HIGH);
    delayMicroseconds(1); // Delay necessary due to timing diagram  
  }
  // Adjust the indices of the new and old measurements
  // Update the array(s) of previous samples
  if (++newIndex > 7) {
    !! ~ Need to update things below this point to account for the left and right channels ~ !!
    // Update the samples and average of samples from the last second
    for(int i = 0; i < 7; i++) {
      samples[i][startIndex] = totalLevel[i] >> 3; // Divide by 8
      totalSamples[i] -= samples[i][endIndex]; // Clear out the old value
      totalSamples[i] += samples[i][startIndex]; // Add in the new value      
      avgSample[i] = totalSamples[i] >> 6; // Divide by 64
    }
    newIndex = 0; // reset the index of the new value

    // Check for beats
    for (int i = 0; i < 7; i++) {
      beat[i] = false;
      if (samples[i][startIndex] > avgSample[i] * C[i]) {
        beat[i] = true;
        //Serial.print("Beat: ");
        //Serial.print(samples[i][startIndex]);
        //Serial.print(" vs ");
//        Serial.println(avgSample[i]);
      } else {
//        Serial.println("No beat.");
      }
    }

    // Expecting this (serial writing) to take about 1 ms
    for (int i = 0; i < 7; i++) {
      if (beat[i] && output[i] < samples[i][startIndex]) {
        if (beatTime[i] < 5) {
          output[i] = 0.5*output[i] + 0.5 * samples[i][startIndex];
        } else {
          output[i] = samples[i][startIndex];
        }
        threshold[i] = avgSample[i];
        fader[i] = 1;
        if (beatTime[i] != 0) {
          faderAdj[i] = faderAdj[i] * 0.75 + (1 - 0.75) * (1 / beatTime[i]);
          if (beatTime[i] < 10) {
            faderAdj[i] = 0.25;
            if (beatTime[i] < 5) {
              faderAdj[i] = 0;
            }
          }
         if (faderAdj[i] < 0.01) {
          faderAdj[i] = 0.01;
         }
        }
        beatTime[i] = 0;
//      } else if(samples[i][startIndex] < (threshold[i])) {
        //output[i] = 0;
      } else {
        output[i] = fader[i] * (0.8 * output[i] + (1 - 0.8) * samples[i][startIndex]);
        fader[i] -= faderAdj[i];
        beatTime[i]++;
        if (fader[i] < 0) {
          fader[i] = 0;
        }
      }
      
//      Serial.print("Beat time: "); Serial.print(beatTime[i]); Serial.print(" "); Serial.print("Fader ["); Serial.print(i); Serial.print("]: "); Serial.print(faderAdj[i]); Serial.print(",  ");
      Serial.print(output[i]);
      if (i < 6) {
        Serial.print(", ");
      }
    }
    Serial.println();

    // Update the LEDs based on the current beat values. (6/29/2018 - might need to adjust to linearize brightness)
    // also need to blend the LEDs better, especially on the sides
    // 1. Need to correct for the actual heights of the side LEDs
    // 2. Back LEDs should hit harder on red for the bass
    // If a beat hits, and a strip/row of LEDs hits, then there probably shouldn't be any LEDs that are totally off
    // Midhigh seems to be a pretty active frequency band
    // Maybe make the high notes white? (affect overall brightness?)
    // It looks like red is easily overpowered by the other colors
    // For some songs it looks like the middle LEDs never turn off. Might need a more aggressive beat-fade (maybe more adaptive for everything?)
    // Should the frequency range be dynamically stretched to fill the LEDs?
    // Could be cool for the front ring LED to be a delayed version of the power icon LED. Or, maybe just pulse a rotating color cycle on the front?
    setLEDs(output, numBeats);
  
    // Calculate a new value for C using the variance of the measurements
    for (int i = 0; i < 7; i++) {
     var[i] = 0;
     for (int j = 0; j < 64; j++) { 
       temp = (samples[i][j] - avgSample[i]);
       var[i] += (temp * temp) / 64;
//       Serial.print(temp); Serial.print(" = "); 
          //Serial.print(samples[i][j]); Serial.print(";  ");
          //Serial.print(" - "); Serial.print(avgSample[i]); Serial.print(", "); Serial.print(var[i]); Serial.println(";  "); 
     }
//     Serial.println(" Next Channel");
     C[i] = (-0.00025714 * var[i]) + 1.5142857; // Modified from the paper
//     Serial.println(C[i]);
    } 
//    Serial.println("");
    // Update the starting and ending indices for the samples from the last second
    startIndex = (startIndex + 1) & 63;
    endIndex = (startIndex + 1) & 63;
  }
  oldIndex = (newIndex + 1) & 7;
  
  // wait x milliseconds before the next loop
  //delay(1);  
}

// Use the output from the beat finder to set the color/brightness of the LEDs
    // Back LED locations
    // low     0, 7, 8,  15, 16
    // midlow  1, 6, 9,  14, 17
    // midhigh 2, 5, 10, 13, 18
    // high    3, 4, 11, 12, 19
void setLEDs(unsigned int beatVals[], char arraySize) {
  // Smush the measured values into fewer buckets for the LEDs
  int low = (beatVals[0] + beatVals[1]) >> 2; // Average the 2 lowest values
  int high = (beatVals[5] + beatVals[6]) >> 2; // Average the 2 highest values
  // Create upper and lower middle levels for the back LEDs
  int midLow = (beatVals[2] + beatVals[3]) >> 2;
  int midHigh = (beatVals[3] + beatVals[4]) >> 2;
  // Create a middle value for the side LEDs
  int mid = (midHigh + midLow) >> 2;
  // Set up the colors for the LEDs
  uint32_t lowColor = leftStrip.Color(low, 0, 0);  // Create a color object. R, G, B.
  uint32_t midColor = leftStrip.Color(low >> 2, mid, mid >> 2);  // Create a color object. R, G, B.
  uint32_t midHighColor = leftStrip.Color(low >> 2, midHigh, midHigh >> 2);  // Create a color object. R, G, B.
  uint32_t midLowColor = leftStrip.Color(low, midLow, midLow >> 2);  // Create a color object. R, G, B.
  uint32_t highColor = leftStrip.Color(low >> 4, mid >> 2, high);  // Create a color object. R, G, B.
  // Set the colors in the LEDs
  // Set the front LEDs
  frontBeatPower[0] = 0.9 * frontBeatPower[0] + (1 - 0.9) * low;  // Exponential low pass filter on the power in each band
  frontBeatPower[1] = 0.9 * frontBeatPower[1] + (1 - 0.9) * mid; 
  frontBeatPower[2] = 0.9 * frontBeatPower[2] + (1 - 0.9) * high;
  char largest = 0;
  if (frontBeatPower[largest] < frontBeatPower[1]) {  // Select the largest band
    largest = 1;
  }
  if (frontBeatPower[largest] < frontBeatPower[2]) {
    largest = 2;
  }
  uint32_t ringColor;
  if (largest == 0) {  // Set the appropriate color for the power icon LED, set the ring LED to something different, and make it bright
    frontStrip.setPixelColor(0, lowColor);
    if (low + mid < 256) {
      ringColor = leftStrip.Color(low, 0, low + mid);  // Create a color object. R, G, B.
    } else {
      ringColor = leftStrip.Color(low, 0, 255);  // Create a color object. R, G, B.      
    }
  } else if (largest == 1) {
    frontStrip.setPixelColor(0, midColor);
    if (low + mid + high < 256) {
      ringColor = leftStrip.Color(0, low + mid + high, 0);  // Create a color object. R, G, B.
    } else {
      ringColor = leftStrip.Color(0, 255, 0);  // Create a color object. R, G, B.      
    }
  } else if (largest == 2) {
    frontStrip.setPixelColor(0, highColor);
    if (low + high < 256) {
      ringColor = leftStrip.Color(low + high, 0, mid >> 2);  // Create a color object. R, G, B.
    } else {
      ringColor = leftStrip.Color(255, 0, mid >> 2);  // Create a color object. R, G, B.      
    }
  }
  frontStrip.setPixelColor(1, ringColor);
  for (int i = 0; i < 3; i++) {  // Set the side LEDs
    // Set the side bottom LEDs
    leftStrip.setPixelColor(i, lowColor);
    rightStrip.setPixelColor(i, lowColor);
    leftStrip.setPixelColor(i + 3, lowColor);
    rightStrip.setPixelColor(i + 3, lowColor);
    // Set the side top LEDs
    leftStrip.setPixelColor(i + 9, highColor);
    rightStrip.setPixelColor(i + 9, highColor);
    leftStrip.setPixelColor(i + 15, highColor);
    rightStrip.setPixelColor(i + 15, highColor);
  }
  // Set the side lower middle LEDs
  rightStrip.setPixelColor(6, midLowColor);
  rightStrip.setPixelColor(14, lowColor);  // Nominally, this would be a mid-low LED, but because of the way the strips were constructed, it's in line with the low LEDs
  rightStrip.setPixelColor(20, midLowColor);
  leftStrip.setPixelColor(6, midLowColor);
  leftStrip.setPixelColor(12, midLowColor);
  leftStrip.setPixelColor(20, midLowColor);
  // Set the side middle LEDs
  rightStrip.setPixelColor(7, midColor);
  rightStrip.setPixelColor(13, midColor);
  rightStrip.setPixelColor(19, midColor);
  leftStrip.setPixelColor(7, midColor);
  leftStrip.setPixelColor(13, midColor);
  leftStrip.setPixelColor(19, midColor);
  // Set the side upper middle LEDs
  rightStrip.setPixelColor(8, highColor);  // Nominally, this would be a mid-high LED, but because of the way the strips were constructed, it's in line with the high LEDs
  rightStrip.setPixelColor(12, midHighColor);
  rightStrip.setPixelColor(18, midHighColor);
  leftStrip.setPixelColor(8, highColor);  // Nominally, this would be a mid-high LED, but because of the way the strips were constructed, it's in line with the high LEDs
  leftStrip.setPixelColor(14, midHighColor);
  leftStrip.setPixelColor(18, highColor);  // Nominally, this would be a mid-high LED, but because of the way the strips were constructed, it's in line with the high LEDs
  // Set the colors of the back LEDs
  for (int i = 0; i < 17; i = i + 8) {
    backStrip.setPixelColor(i, lowColor);
    backStrip.setPixelColor(i + 1, midLowColor);
    backStrip.setPixelColor(i + 2, midHighColor);
    backStrip.setPixelColor(i + 3, highColor);
    if (i > 7) {
      backStrip.setPixelColor(i - 1, lowColor);
      backStrip.setPixelColor(i - 2, midLowColor);
      backStrip.setPixelColor(i - 3, midHighColor);
      backStrip.setPixelColor(i - 4, highColor);
    }
  }
  // Show the new LED colors
  leftStrip.show();
  rightStrip.show();
  backStrip.show();
  frontStrip.show();
}

