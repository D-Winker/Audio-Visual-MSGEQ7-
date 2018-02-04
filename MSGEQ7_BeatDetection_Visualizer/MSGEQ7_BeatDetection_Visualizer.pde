/* MSGEQ7 Beat Detection Visualizer (Modified code from Trace)
 * Expects a serial message of 7 integers separated by commas, with a newline at the end.
 * The numbers are separated, and the values are used as colors for 7 rectangles.
 * The first number corresponds to the leftmost rectangle, the final number to the rightmost rectangle.
 *
 * By Daniel Winker
 */ 

import processing.serial.*; // For reading serial data
import java.util.*;

PFont defaultFont; // Declare PFont variable
Serial mySerial; // Serial object for reading from the port
PrintWriter output; // For writing to a text file
String serialMessage = "Hi!"; // Text displayed on the GUI
String debugText = "Hi.";
int lf = 10; // ASCII linefeed (newline)
boolean messageFlag = false; // Is there a new serial message?
int numMeasurements; // The number of measurements in the most recent bunch of serial data
boolean measurementFlag; // Indicate whether or not there are new measurements
float[] measurements; // The newest measurements from serial

int bckgndColor = 175; // Background color

void setup() {
  size(1400, 800, P3D); // Set up GUI, width and height, create a 3D space
  perspective(PI/3.0, width/height, ((height/2.0) / tan(PI*60.0/360.0))/10.0, ((height/2.0) / tan(PI*60.0/360.0)) * 100.0); // Extend draw distance
  surface.setResizable(true);
  stroke(150, 150, 50); // Color for drawing lines
  defaultFont = createFont("Arial",20,true); // Arial, 18 point, anti-aliasing on
  setUpSerial("COM3", 115200);
  println("Finished setup.");
}

void draw() {  
  guiManagement();
  parseSerial();
} // End draw() ----------------------------------------------------------------------------------------------------------------------------------------


// Called when bufferUntil reaches the 'until' (a newline, in the case of this code)
void serialEvent(Serial p) {
  serialMessage = p.readString(); // Store whatever message was sent
  messageFlag = true; // Indicate that we have a new message
}


// Take care of various GUI tasks (setup, colors, etc.) that have to be done for each frame
void guiManagement() {
  textFont(defaultFont,36); // Font variable, font size override
  fill(0); // Specify color
  textAlign(LEFT); // Text alignment (L, R, C)
  if (measurementFlag) {
    measurementFlag = false; // Note that the measurement is being used
    if (measurements.length == 7) {
      //background(bckgndColor); // Create background and set background color
      // use the measurements
      fill(measurements[0],0,0);
      rect(0,0,width/7, height);
      print(measurements[0] + ", ");
      fill(measurements[1],0.5*measurements[1],0);
      rect(width/7,0,width/7, height);
      print(measurements[1] + ", ");
      fill(0.5*measurements[2],measurements[2],0);
      rect(2*width/7,0,width/7, height);
      print(measurements[2] + ", ");
      fill(0,measurements[3],0);
      rect(3*width/7,0,width/7, height);
      print(measurements[3] + ", ");
      fill(0,measurements[4],0.5*measurements[4]);
      rect(4*width/7,0,width/7, height);
      print(measurements[4] + ", ");
      fill(0,0.5*measurements[5],measurements[5]);
      rect(5*width/7,0,width/7, height);
      print(measurements[5] + ", ");
      fill(0,0,measurements[6]);
      rect(6*width/7,0,width/7, height);
      println(measurements[6]);
      //println(frameRate);
    }
  }
}

// Extract the measurements from the message sent
// Remove commas and whitespace, convert to integers
void parseSerial() {
 if (serialMessage != null && messageFlag) { // Did we really get something?
   messageFlag = false; // Note that we're using the message
   measurementFlag = true; // Indicate that there are new measurements
   String[] receivedSerial = split(serialMessage, ','); // Split the message on commas, store the substrings
   numMeasurements = receivedSerial.length;
   measurements = new float[numMeasurements]; // Well, we never actually declared this. Better do that before using it. Should probably replace this with either local vars only, or an arraylist
   // Loop through the data received, convert each number to an int, store in the measurements array
   for (int i = 0; i < numMeasurements; i++) {
     try {
         //measurements[i] = i;
         //Integer.parseInt(receivedSerial[i].trim());
         measurements[i] = int(receivedSerial[i].trim()); // Remove whitespace, convert the string value to an int, and scale
         if (measurements[i] > 255) {
           measurements[i] = 255;
         }
       } catch (Exception e) {
         measurementFlag = false; // Things didn't work out; don't use this message.
         println("Error converting string data to integer. Measurement " + i + " [" + receivedSerial[i].trim() + "]");
       }
   }
 }
}