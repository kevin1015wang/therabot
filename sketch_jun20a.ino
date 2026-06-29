#include <ESP32Servo.h> // Include the ESP32-specific servo library

// Create a servo object
Servo myContinuousServo; 
Servo myContinuousServo2; 

// Define the GPIO pin connected to the servo signal wire
const int servoPin = 18; 
const int servoPin2 = 12; 

bool runMotor = false;

int universalDelay = 200;

int incomingByte = 0; // for incoming serial data

void setup() {
  // Allow allocation of all hardware timers
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  
  // Set frequency to standard 50Hz for hobby servos
  myContinuousServo.setPeriodHertz(50); 
  myContinuousServo2.setPeriodHertz(50); 
  
  // Attach the servo object to the pin
  myContinuousServo.attach(servoPin); 
  myContinuousServo2.attach(servoPin2); 

  Serial.begin(9600);
}

void loop() {

  if (Serial.available() > 0) {
    // read the incoming byte:
    incomingByte = Serial.read();

    // say what you got:
    Serial.print("I received: ");
    Serial.println(incomingByte, DEC);

    // we will need to read the incomingByte and turn servo motor on/off
    if (incomingByte == 49 || incomingByte == '1') {
      // this is a '1'
      // turn on
      runMotor = true;

    } else if (incomingByte == 48 || incomingByte == '0') {
      // turn off
      runMotor = false;
    }
  }

  if (runMotor) {
    // 1. Rotate full speed Clockwise
    myContinuousServo.write(0); 
    delay(universalDelay); // Spin for 3 seconds
    myContinuousServo2.write(0); 
    delay(universalDelay); // Spin for 3 seconds

    // 2. Stop the motor
    myContinuousServo.write(90); 
    delay(universalDelay); // Remain stopped for 2 seconds
    myContinuousServo2.write(90); 
    delay(universalDelay); // Remain stopped for 2 seconds

    // 3. Rotate full speed Counter-Clockwise
    myContinuousServo.write(180); 
    delay(universalDelay); // Spin for 3 seconds  
    myContinuousServo2.write(180); 
    delay(universalDelay); // Spin for 3 seconds

    // 4. Stop the motor again
    myContinuousServo.write(90); 
    delay(universalDelay); // Remain stopped for 2 seconds
    myContinuousServo2.write(90); 
    delay(universalDelay); // Remain stopped for 2 seconds
  } else {
    // stop moving
    myContinuousServo.write(90); 
    delay(universalDelay); // Remain stopped for 2 seconds
    myContinuousServo2.write(90); 
    delay(universalDelay); // Remain stopped for 2 seconds
  }


}