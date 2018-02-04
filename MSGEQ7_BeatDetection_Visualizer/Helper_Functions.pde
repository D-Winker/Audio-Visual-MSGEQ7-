// :: General Functions ::

// Returns the current date and time as a string in the format
// month-day-year_hh-mm-ss
// It turns out colons are not safe for Windows file names!
String getTimeAndDate() {
  return (month() + "-" + day() + "-" + year() + "_" + hour() + "-" + minute() + "-" + second());
}

// Closes log files and exits the program
void quit() {
  closePort(); // Release the port before the program ends
}

// This is called when the program ends
// So, when someone clicks the X in the corner
public void stop() {
  quit();
  super.stop(); // This calls the default stop method
} 

// :: Setup Functions ::
// These functions get things ready to be used
// They likely only ever need to be called once, on startup

// Returns a String array of all available serial connections
String[] getSerialOptions() {
  return Serial.list();
}

// Establish a serial connection at the desired baud rate
void setUpSerial(String serialPort, int baudRate) {
  println("Setting up the serial connection"); // Debug
  closePort(); // If we're already connected to something, break the connection
  // Check that the port is still there, select it
  for (int portIter = 0; portIter < Serial.list().length; portIter++) {
    if (Serial.list()[portIter].equals(serialPort)) {
      mySerial = new Serial( this, Serial.list()[portIter], baudRate); // Open a serial port at the desired rate
      mySerial.bufferUntil(lf); // Fill buffer until there's a new line
      println("Making a port connection."); // Debug
    }
  }  
}

// Close the serial connection, if there is one
void closePort() { 
  try {
    mySerial.stop();
  } catch(Exception e) {
    println("There was no prior connection."); // Debug
  }
}