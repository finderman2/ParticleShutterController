// This #include statement was automatically added by the Particle IDE.
#include <Adafruit_LSM303_U.h>

// This #include statement was automatically added by the Particle IDE.
#include <Adafruit_Sensor.h>

// This #include statement was automatically added by the Particle IDE.
#include <SparkIntervalTimer.h>
#include <math.h>
#include <application.h>


/*
Dixon Observatory Shutter Control
Written by Liam O'Brien
*/

void blink () {
    GPIOA->ODR ^= (1<<13); //toggle the led on pin 7
}

// reverses a string 'str' of length 'len'
void reverse(char *str, int len) {
    int i=0, j=len-1, temp;
    while (i<j)
    {
        temp = str[i];
        str[i] = str[j];
        str[j] = temp;
        i++; j--;
    }
}
 
 // Converts a given integer x to string str[].  d is the number
 // of digits required in output. If d is more than the number
 // of digits in x, then 0s are added at the beginning.
int intToStr(int x, char str[], int d) {
    int i = 0;
    while (x)
    {
        str[i++] = (x%10) + '0';
        x = x/10;
    }
 
    // If number of digits required is more, then
    // add 0s at the beginning
    while (i < d)
        str[i++] = '0';
 
    reverse(str, i);
    str[i] = '\0';
    return i;
}
void ftoa(float n, char *res, int afterpoint) {
    // Extract integer part
    int ipart = (int)n;
 
    // Extract floating part
    float fpart = n - (float)ipart;
 
    // convert integer part to string
    int i = intToStr(ipart, res, 0);
 
    // check for display option after point
    if (afterpoint != 0)
    {
        res[i] = '.';  // add dot
 
        // Get the value of fraction part upto given no.
        // of points after dot. The third parameter is needed
        // to handle cases like 233.007
        fpart = fpart * pow(10, afterpoint);
 
        intToStr((int)fpart, res + i + 1, afterpoint);
    }
}

//timers
IntervalTimer switchCheck;
void checkSwitches(void);

/* Assign a unique ID to this sensor at the same time */
Adafruit_LSM303_Mag_Unified mag = Adafruit_LSM303_Mag_Unified(12345);
Adafruit_LSM303_Accel_Unified accel(30301);

float Pi = 3.14159;
double heading;
String azimuth;
int northOffset = 0;

int magRaw[3];
int accRaw[3];
float accXnorm,accYnorm,pitch,roll,magXcomp,magYcomp;

char res[20];

// Offsets applied to raw x/y/z values
float mag_offsets[3] = { 3.88F, -1.30F, -23.28F };

//Pin used for status LED
const int LED = 7;
//Control lines for the motors
const int sA = A0;
const int sB = A1;
const int sPWM = A4;
const int wA = A2;
const int wB = A3;
const int wPWM = A5;
//Pin numbers of limit switches
const int shutter_limit_open = D5;
const int shutter_limit_closed = D6;
const int windscreen_limit_open = 3;
const int windscreen_limit_closed = 4;
//const int current_sense_A = 5;
//const int current_sense_B = 6;
//Values in amps
double current_val_A;
double current_val_B;
//state of the shutter and screen, 1 for open
int sl_open = 0;
int sl_close = 0;

unsigned long lastAzUpdateTime = 0;
int compassError = 0;

String shutterCmd = "Startup"; //what is the last state we set the shutter to
String screenCmd = ""; // ditto above

Timer heartBeat(2000, blink);

void setup() {
  //Serial.begin(9600);
  //Cloud Setup
  Particle.function("shutter", shutterControl);
  //Particle.variable("current1", current_val_A);
  //Particle.variable("current2", current_val_B);
  Particle.variable("shutterStat", shutterCmd);
  Particle.variable("screenStat", screenCmd);
  Particle.variable("compassErr", compassError);
  
  pinMode(LED, OUTPUT);
  //Set limit switches as inputs
  pinMode(windscreen_limit_open, INPUT);
  pinMode(windscreen_limit_closed, INPUT);
  pinMode(shutter_limit_open, INPUT);
  pinMode(shutter_limit_closed, INPUT);
  //Set current sense lines as inputs
  //pinMode(current_sense_A, INPUT);
  //pinMode(current_sense_B, INPUT);
  
  //Set motors as outputs
  pinMode(sA, OUTPUT);
  pinMode(sB, OUTPUT);
  pinMode(wA, OUTPUT);
  pinMode(wB, OUTPUT);
  pinMode(sPWM, OUTPUT);
  pinMode(wPWM, OUTPUT);
  
  heartBeat.start();
  switchCheck.begin(checkSwitches, 125, hmSec, TIMER4); // tick every 62.5ms using hmSec 120*.5
  /* Initialise the sensor */
  if(!mag.begin()) {
    compassError = 1;
  }
}

void checkSwitches(void) {
	sl_open = digitalRead(shutter_limit_open);
	sl_close = digitalRead(shutter_limit_closed);
}

void loop() {
  //shutter_status = readLimitSwitch(shutter_limit_open, shutter_limit_closed);
  //current_val_A = (analogRead(current_sense_A)/1240.9) / 0.13;
  //current_val_B = (analogRead(current_sense_B)/1240.9) / 0.13;
  
  if (shutterCmd == "Startup") {
      // do nothing
  } 
  else if (shutterCmd == "Open") {
      
  }
  else if (shutterCmd == "Closed") {
      
  } else {
    if (shutterCmd == "Opening") {
        if (sl_open == 0 ) {
            //Opening
         }
        else if (sl_open == 1) {
            stopShutter();
            shutterCmd = "Open";
        }
  }
  
  if (shutterCmd == "Closing") {
    if (sl_close == 0 ) {
        //Opening
    }
    else if (sl_close == 1) {
        stopShutter();
        shutterCmd = "Closed";
    }
  }
} //end loop
   /* Get a new sensor event */ 
  sensors_event_t magEvent; 
  
  mag.getEvent(&magEvent);
  
  magRaw[0] = magEvent.magnetic.x - mag_offsets[0];
  magRaw[1] = magEvent.magnetic.y - mag_offsets[1];
  magRaw[2] = magEvent.magnetic.z - mag_offsets[2];
  
  //flip the y axis readings since it's pointing down 
  magRaw[2] = -magRaw[2];
  
  //Calculate heading
  //heading = 180*atan2(magXcomp,magZcomp)/Pi;
  heading = (180*atan2(magRaw[0], magRaw[2])/Pi) - northOffset;

  //Convert heading to 0 - 360
  if(heading < 0)
    heading += 360;
  
  ftoa(heading, res, 4);
  
  if(millis() - lastAzUpdateTime >= 5000){
    Particle.publish("domeAzimuth", res, 60, PRIVATE);
    lastAzUpdateTime = millis(); //update old_time to current millis()
  }
}

int openShutter() {
      //Send motor ON command
      digitalWrite(sPWM, HIGH);
      digitalWrite(sA, HIGH);
      digitalWrite(sB, LOW);
      shutterCmd = "Opening";
      return 1;
}

int closeShutter() {
    // if (wl_stat == 0) { //interlock to ensure windscreen is already closed
        digitalWrite(sPWM, HIGH);
        digitalWrite(sA, LOW);
        digitalWrite(sB, HIGH);
        shutterCmd = "Closing";
        return 2;
    //}
}

// int openScreen() {
//       if (sl_stat == 1) { //interlock to ensure shutter is already open
//           //Send motor ON command
//           digitalWrite(wPWM, HIGH);
//           digitalWrite(wA, HIGH);
//           digitalWrite(wB, LOW);
//           screenCmd = "Open";
//           wl_stat = 1;
//           return 3;
//       }
//       else {
//           openShutter();
//       }
// }

// int closeScreen() {
//       if (sl_stat == 1) {
//           digitalWrite(wPWM, HIGH);
//           digitalWrite(wA, LOW);
//           digitalWrite(wB, HIGH);
//           screenCmd = "Closed";
//           wl_stat = 0;
//           return 4;
//       }
// }


void stopShutter() {
    //Motor OFF Command
    digitalWrite(sPWM, LOW);
    digitalWrite(sA, LOW);
    digitalWrite(sB, LOW);
}

void stopScreen() {
    //Motor OFF Command
    digitalWrite(wPWM, LOW);
    digitalWrite(wA, LOW);
    digitalWrite(wB, LOW);
}

int shutterControl(String command) {
  if (command != '\0') {
    if (command == "openshutter") {
      openShutter();
      return 1;
    }
    else if (command == "closeshutter") {
      closeShutter();
      return 2;
    }
    // else if (command == "openscreen") {
    //   openScreen();
    //   return 3;
    // }
    // else if (command == "closescreen") {
    //   closeScreen();
    //   return 4;
    // }
  }
  return 5;
}