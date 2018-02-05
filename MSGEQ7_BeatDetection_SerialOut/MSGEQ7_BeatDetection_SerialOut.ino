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
*/

// These constants won't change.  They're used to give names
// to the pins used:
const int outPin = A0;  // Analog input pin that the potentiometer is attached to
const int analogOutPin = 9; // Analog output pin that the LED is attached to
int strobePin  = 7;    // Strobe Pin on the MSGEQ7
int resetPin   = 8;    // Reset Pin on the MSGEQ7

int sensorValue = 0;        // value read from the pot
int outputValue = 0;        // value output to the PWM (analog out)

// For low pass filtering (boxcar filter)
unsigned int level[7][8];      // An array to hold the values from the 7 frequency bands
unsigned int totalLevel[7];      // An array to hold the summed values from the 7 frequency bands
unsigned char oldIndex, newIndex; // What's the oldest measurement in the array? Where does the new one go?

// To store the average 'energy' over the last ~1 second
unsigned int samples[7][64]; // Holds the low-pass filtered samples
unsigned int totalSamples[7]; // Holds the sums of 64 samples
unsigned int avgSample[7]; // Holds the averages of the last 64 samples
unsigned char endIndex, startIndex; // What's the oldest sample in the array? Where does the new one go?

unsigned int output[7]; // Value the program will output for a channel
boolean beat[7]; // Track whether or not there was a beat
unsigned int threshold[7]; // For separating out noise
float C[7]; // Multiplier for the 'beat threshold'
float var[7]; // Variance of the measurements
signed int temp; // Used in calculating variance
float fader[7]; // Fade out if there hasn't been a beat in a while
float faderAdj[7]; // The rate at which the fader approaches zero
int beatTime[7]; // A count of the time between beats. Used to determine faderAdj

void setup() {
  // initialize serial communications at 9600 bps:
  Serial.begin(115200);
  pinMode      (strobePin, OUTPUT);
  pinMode      (resetPin,  OUTPUT);
    // Create an initial state for our pins
  digitalWrite (resetPin,  LOW);
  digitalWrite (strobePin, LOW);
  delay        (1);
 
  // Reset the MSGEQ7 as per the datasheet timing diagram
  digitalWrite (resetPin,  HIGH);
  delay        (1);
  digitalWrite (resetPin,  LOW);
  digitalWrite (strobePin, HIGH); 
  delay        (1);

  // Set up the arrays/circular buffers
  newIndex = 0;
  oldIndex = 1;
  for (int i = 0; i < 7; i++) {
    totalLevel[i] = 0;
    totalSamples[i] = 0;
    avgSample[i] = 0;
    for(int j = 0; j < 64; j++) {
      samples[i][j] = 0;
    }
    for (int j = 0; j < 8; j++) {
      level[i][j] = 0;
    }
    C[i] = 1.3;
    threshold[i] = 0;
    fader[i] = 1;
    faderAdj[i] = 0.1;
    beatTime[i] = 0;
  }
}

void loop() { // Should take about 2.5 ms, 400 Hz
  // Cycle through each frequency band by pulsing the strobe. Should take ~200us per band (1.4 ms total)
  for (int i = 0; i < 7; i++) {
    digitalWrite(strobePin, LOW); // Switch to the next band (MSGEQ7)
    delayMicroseconds(100); // Delay necessary due to timing diagram
    level[i][newIndex] = analogRead(outPin);
    totalLevel[i] -= level[i][oldIndex]; // Clear out the old value
    totalLevel[i] += level[i][newIndex]; // Add in the new value
    digitalWrite(strobePin, HIGH);
    delayMicroseconds(1); // Delay necessary due to timing diagram  
  }
  // Adjust the indices of the new and old measurements
  // Update the array(s) of previous samples
  if (++newIndex > 7) {
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
