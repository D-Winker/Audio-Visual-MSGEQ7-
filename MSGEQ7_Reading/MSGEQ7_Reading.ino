/*
 Reads and strobes through the MSGEQ7's outputs
 Prints the readings over serial
*/

// These constants won't change.  They're used to give names
// to the pins used:
const int outPin = A0;  // Analog input pin that the potentiometer is attached to
const int analogOutPin = 9; // Analog output pin that the LED is attached to
int strobePin  = 7;    // Strobe Pin on the MSGEQ7
int resetPin   = 8;    // Reset Pin on the MSGEQ7

int sensorValue = 0;        // value read from the pot
int outputValue = 0;        // value output to the PWM (analog out)

int level[7];          // An array to hold the values from the 7 frequency bands

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
}

void loop() {
  // Cycle through each frequency band by pulsing the strobe.
  for (int i = 0; i < 7; i++) {
    digitalWrite       (strobePin, LOW);
    delayMicroseconds  (100);                  // Delay necessary due to timing diagram
    level[i] = 0.25*analogRead (outPin) + (1-0.25)*level[i];
    digitalWrite       (strobePin, HIGH);
    delayMicroseconds  (1);                    // Delay necessary due to timing diagram  
  }
  for (int i = 0; i < 7; i++) {
    Serial.print       (level[i]);
    if (i < 6) {
      Serial.print       (", ");
    }
  }
  Serial.println ();

  // wait x milliseconds before the next loop
  delay(5);
}
