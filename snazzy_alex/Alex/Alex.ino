  
#include <serialize.h>
#include <stdarg.h>
#include "packet.h"
#include "constants.h"
#include "math.h"

typedef enum
{
    STOP = 0,
    FORWARD = 1,
    BACKWARD = 2,
    LEFT = 3,
    RIGHT = 4
} TDirection;

volatile TDirection dir = STOP;
/*
 * Alex's configuration constants
 */

// Number of ticks per revolution from the 
// wheel encoder.

#define COUNTS_PER_REV      48
#define PI 3.141592654
// Wheel circumference in cm.
// We will use this to calculate forward/backward distance traveled 
// by taking revs * WHEEL_CIRC

#define WHEEL_CIRC         20.4

// Motor control pins. You need to adjust these till
// Alex moves in the correct direction
#define LF                  6   // Left forward pin
#define LR                  5   // Left reverse pin
#define RF                  11  // Right forward pin
#define RR                  10  // Right reverse pin


#define ALEX_LENGTH      17
#define ALEX_BREADTH     12.5

float alexDiagonal = 0.0;
float alexCirc = 0.0;
/*
 *    Alex's State Variables
 */

// Store the ticks from Alex's left and
// right encoders.
volatile unsigned long leftForwardTicks; 
volatile unsigned long rightForwardTicks;
volatile unsigned long leftReverseTicks; 
volatile unsigned long rightReverseTicks;

volatile unsigned long leftForwardTicksTurns; 
volatile unsigned long rightForwardTicksTurns;
volatile unsigned long leftReverseTicksTurns; 
volatile unsigned long rightReverseTicksTurns;

volatile unsigned long ir_reading1;
volatile unsigned long ir_reading2;
volatile unsigned long ultra_reading;
volatile unsigned long red_reading;
volatile unsigned long green_reading;

// Store the revolutions on Alex's left
// and right wheels
volatile unsigned long leftRevs;
volatile unsigned long rightRevs;

// Forward and backward distance traveled
volatile unsigned long forwardDist;
volatile unsigned long reverseDist;

unsigned long deltaDist;
unsigned long newDist;
unsigned long deltaTicks;
unsigned long targetTicks;


//IR Sensor
int IRSensor = A1; //Analog pin A1 for ___ IR Sensor
int IRSensor2 = A2; // Analog pin A2 for ___ IR Sensor


//Ultrasonic Sensor
#define echoPin 9 // attach pin 8 Arduino to pin Echo of HC-SR04
#define trigPin 8 //attach pin 9 Arduino to pin Trig of HC-SR04
long duration; // variable for the duration of sound wave travel
int distance; // variable for the distance measurement

//Color Test
#define S0 1 
#define S1 4
#define S2 7
#define S3 12
#define sensorOut 13
int frequency = 0;

/*
 * 
 * Alex Communication Routines.
 * 
 */
 
TResult readPacket(TPacket *packet)
{
    // Reads in data from the serial port and
    // deserializes it.Returns deserialized
    // data in "packet".
    
    char buffer[PACKET_SIZE];
    int len;

    len = readSerial(buffer);

    if(len == 0)
      return PACKET_INCOMPLETE;
    else
      return deserialize(buffer, len, packet);
    
}

void sendStatus()
{
  // Implement code to send back a packet containing key
  // information like leftTicks, rightTicks, leftRevs, rightRevs
  // forwardDist and reverseDist
  // Use the params array to store this information, and set the
  // packetType and command files accordingly, then use sendResponse
  // to send out the packet. See sendMessage on how to use sendResponse.
  //

  TPacket statusPacket ;

  statusPacket.packetType = PACKET_TYPE_RESPONSE;
  statusPacket.command = RESP_STATUS;
  statusPacket.params[0] = leftForwardTicks;
  statusPacket.params[1] = rightForwardTicks;
  statusPacket.params[2] = leftReverseTicks;
  statusPacket.params[3] = rightReverseTicks;
  statusPacket.params[4] = leftForwardTicksTurns;
  statusPacket.params[5] = rightForwardTicksTurns;
  statusPacket.params[6] = leftReverseTicksTurns;
  statusPacket.params[7] = rightReverseTicksTurns;
  statusPacket.params[8] = forwardDist;
  statusPacket.params[9] = reverseDist;
  statusPacket.params[10] = ir_reading1;
  statusPacket.params[11] = ir_reading2;
  statusPacket.params[12] = ultra_reading;
  statusPacket.params[13] = red_reading;
  statusPacket.params[14] = green_reading;

  sendResponse(&statusPacket);
}

void sendMessage(const char *message)
{
  // Sends text messages back to the Pi. Useful
  // for debugging.
  
  TPacket messagePacket;
  messagePacket.packetType=PACKET_TYPE_MESSAGE;
  strncpy(messagePacket.data, message, MAX_STR_LEN);
  sendResponse(&messagePacket);
}

void dbprint(char *format, ...){
  va_list args;
  char buffer[128];

  va_start(args,format);
  vsprintf(buffer,format,args);
  sendMessage(buffer);
}

void sendBadPacket()
{
  // Tell the Pi that it sent us a packet with a bad
  // magic number.
  
  TPacket badPacket;
  badPacket.packetType = PACKET_TYPE_ERROR;
  badPacket.command = RESP_BAD_PACKET;
  sendResponse(&badPacket);
  
}

void sendBadChecksum()
{
  // Tell the Pi that it sent us a packet with a bad
  // checksum.
  
  TPacket badChecksum;
  badChecksum.packetType = PACKET_TYPE_ERROR;
  badChecksum.command = RESP_BAD_CHECKSUM;
  sendResponse(&badChecksum);  
}

void sendBadCommand()
{
  // Tell the Pi that we don't understand its
  // command sent to us.
  
  TPacket badCommand;
  badCommand.packetType=PACKET_TYPE_ERROR;
  badCommand.command=RESP_BAD_COMMAND;
  sendResponse(&badCommand);
  
}

void sendBadResponse()
{
  TPacket badResponse;
  badResponse.packetType = PACKET_TYPE_ERROR;
  badResponse.command = RESP_BAD_RESPONSE;
  sendResponse(&badResponse);
}

void sendOK()
{
  TPacket okPacket;
  okPacket.packetType = PACKET_TYPE_RESPONSE;
  okPacket.command = RESP_OK;
  sendResponse(&okPacket);  
}

void sendResponse(TPacket *packet)
{
  // Takes a packet, serializes it then sends it out
  // over the serial port.
  char buffer[PACKET_SIZE];
  int len;
  
  len = serialize(buffer, packet, sizeof(TPacket));
  writeSerial(buffer, len);
}


/*
 * Setup and start codes for external interrupts and 
 * pullup resistors.
 * 
 */
// Enable pull up resistors on pins 2 and 3
void enablePullups()
{
  DDRD &= 0b11110011;
  PORTD |= 0b00001100;
  // Use bare-metal to enable the pull-up resistors on pins
  // 2 and 3. These are pins PD2 and PD3 respectively.
  // We set bits 2 and 3 in DDRD to 0 to make them inputs. 
}

// Functions to be called by INT0 and INT1 ISRs.
void leftISR()
{

  if(dir == FORWARD){
     leftForwardTicks++;
     
  }
  if(dir == BACKWARD){
    leftReverseTicks++;
    
  }
  if(dir == LEFT){
    leftReverseTicksTurns++;
  }
  if(dir == RIGHT){
    leftForwardTicksTurns++;
  }
  
}

void rightISR()
{
  if(dir == FORWARD){
     rightForwardTicks++;
     forwardDist = (unsigned long) ((float) rightForwardTicks /  COUNTS_PER_REV * WHEEL_CIRC);
  }
  if(dir == BACKWARD){
    rightReverseTicks++;
    reverseDist = (unsigned long) ((float) rightReverseTicks /  COUNTS_PER_REV * WHEEL_CIRC);
  }
  if(dir == LEFT){
    rightForwardTicksTurns++;
  }
  if(dir == RIGHT){
    rightReverseTicksTurns++;
  }
  
}

// Set up the external interrupt pins INT0 and INT1
// for falling edge triggered. Use bare-metal.
void setupEINT()
{
  cli();
  
  DDRD &= ~0b00001100; 
  EICRA |= 0b00001010;
  EIMSK |= 0b00000011;
  sei();
  // Use bare-metal to configure pins 2 and 3 to be
  // falling edge triggered. Remember to enable
  // the INT0 and INT1 interrupts.
}

// Implement the external interrupt ISRs below.
// INT0 ISR should call leftISR while INT1 ISR
// should call rightISR.

ISR(INT0_vect){
  rightISR();
  //SensorTest();
}

ISR(INT1_vect){
  leftISR();
  //SensorTest();
}

// Implement INT0 and INT1 ISRs above.

/*
 * Setup and start codes for serial communications
 * 
 */
// Set up the serial connection. For now we are using 
// Arduino Wiring, you will replace this later
// with bare-metal code.
void setupSerial()
{
  // To replace later with bare-metal.
  Serial.begin(9600);
}

// Start the serial connection. For now we are using
// Arduino wiring and this function is empty. We will
// replace this later with bare-metal code.

void startSerial()
{
  // Empty for now. To be replaced with bare-metal code
  // later on.
  
}

// Read the serial port. Returns the read character in
// ch if available. Also returns TRUE if ch is valid. 
// This will be replaced later with bare-metal code.

int readSerial(char *buffer)
{

  int count=0;

  while(Serial.available())
    buffer[count++] = Serial.read();

  return count;
}

// Write to the serial port. Replaced later with
// bare-metal code

void writeSerial(const char *buffer, int len)
{
  Serial.write(buffer, len);
}

/*
 * Alex's motor drivers.
 * 
 */

// Set up Alex's motors. Right now this is empty, but
// later you will replace it with code to set up the PWMs
// to drive the motors.
void setupMotors()
{
  /* Our motor set up is:  
   *    A1IN - Pin 5, PD5, OC0B
   *    A2IN - Pin 6, PD6, OC0A
   *    B1IN - Pin 10, PB2, OC1B
   *    B2In - pIN 11, PB3, OC2A
   */
   
}

// Start the PWM for Alex's motors.
// We will implement this later. For now it is
// blank.
void startMotors()
{
  
}

// Convert percentages to PWM values
int pwmVal(float speed)
{
  if(speed < 0.0)
    speed = 0;

  if(speed > 100.0)
    speed = 100.0;

  return (int) ((speed / 100.0) * 255.0);
}

// Move Alex forward "dist" cm at speed "speed".
// "speed" is expressed as a percentage. E.g. 50 is
// move forward at half speed.
// Specifying a distance of 0 means Alex will
// continue moving forward indefinitely.
void forward(float dist, float speed)
{
  int val = pwmVal(speed);

  dir = FORWARD;

  if(dist == 0)
    deltaDist = 999999;
  else
    deltaDist = dist;
    
  if(dist > 0)
    deltaDist = dist;
  else
    deltaDist = 9999999;

   newDist = forwardDist + deltaDist;
  
  // For now we will ignore dist and move
  // forward indefinitely. We will fix this
  // in Week 9.

  // LF = Left forward pin, LR = Left reverse pin
  // RF = Right forward pin, RR = Right reverse pin
  // This will be replaced later with bare-metal code.
  
  analogWrite(LF, val );
  analogWrite(RF, val);
  analogWrite(LR,0);
  analogWrite(RR, 0);
}

// Reverse Alex "dist" cm at speed "speed".
// "speed" is expressed as a percentage. E.g. 50 is
// reverse at half speed.
// Specifying a distance of 0 means Alex will
// continue reversing indefinitely.
void reverse(float dist, float speed)
{

  int val = pwmVal(speed);
  dir = BACKWARD;

  if(dist == 0)
    deltaDist = 999999;
  else
    deltaDist = dist;
    
  if(dist > 0)
    deltaDist = dist;
  else
    deltaDist = 9999999;

   newDist = reverseDist + deltaDist;
  // For now we will ignore dist and 
  // reverse indefinitely. We will fix this
  // in Week 9.

  // LF = Left forward pin, LR = Left reverse pin
  // RF = Right forward pin, RR = Right reverse pin
  // This will be replaced later with bare-metal code.
  analogWrite(LR, val);
  analogWrite(RR, val);
  analogWrite(LF, 0);
  analogWrite(RF, 0);
}

unsigned long computeDeltaTicks(float ang){

  unsigned long tick = (unsigned long)((ang * alexCirc * COUNTS_PER_REV) / (360.0 * WHEEL_CIRC));
  return tick;
}

// Turn Alex left "ang" degrees at speed "speed".
// "speed" is expressed as a percentage. E.g. 50 is
// turn left at half speed.
// Specifying an angle of 0 degrees will cause Alex to
// turn left indefinitely.
void left(float ang, float speed)
{  
  int val = pwmVal(speed);
  dir = LEFT;
  
  if(ang == 0)
    deltaTicks = 99999999;
  else
    deltaTicks = computeDeltaTicks(ang);

   targetTicks = leftReverseTicksTurns + deltaTicks;
   
  // For now we will ignore ang. We will fix this in Week 9.
  // We will also replace this code with bare-metal later.
  // To turn left we reverse the left wheel and move
  // the right wheel forward.
  analogWrite(LR, val*0.9);
  analogWrite(RF, val);
  analogWrite(LF, 0);
  analogWrite(RR, 0);
}

// Turn Alex right "ang" degrees at speed "speed".
// "speed" is expressed as a percentage. E.g. 50 is
// turn left at half speed.
// Specifying an angle of 0 degrees will cause Alex to
// turn right indefinitely.
void right(float ang, float speed)
{
  int val = pwmVal(speed);
  dir = RIGHT;

  if(ang == 0)
    deltaTicks = 99999999;
  else
    deltaTicks = computeDeltaTicks(ang);

   targetTicks = rightReverseTicksTurns + deltaTicks;
   
  // For now we will ignore ang. We will fix this in Week 9.
  // We will also replace this code with bare-metal later.
  // To turn right we reverse the right wheel and move
  // the left wheel forward.
  analogWrite(RR, val*0.95);
  analogWrite(LF, val);
  analogWrite(LR, 0);
  analogWrite(RF, 0);
}

// Stop Alex. To replace with bare-metal code later.
void stop()
{

  dir = STOP;
  
  analogWrite(LF, 0);
  analogWrite(LR, 0);
  analogWrite(RF, 0);
  analogWrite(RR, 0);
}

/*
 * Alex's setup and run codes
 * 
 */

// Clears all our counters
void clearCounters()
{
  leftForwardTicks = 0; 
 rightForwardTicks = 0;
 leftReverseTicks = 0; 
 rightReverseTicks = 0;

 leftForwardTicksTurns = 0; 
 rightForwardTicksTurns = 0;
 leftReverseTicksTurns =0; 
 rightReverseTicksTurns =0;

 leftRevs = 0;
 rightRevs = 0;

 forwardDist = 0;
 reverseDist = 0; 
}

// Clears one particular counter
void clearOneCounter(int which)
{
  clearCounters();
  
}
// Intialize Vincet's internal states

void initializeState()
{
  clearCounters();
}

void handleCommand(TPacket *command)
{
  switch(command->command)
  {
    // For movement commands, param[0] = distance, param[1] = speed.
    case COMMAND_FORWARD:
        sendOK();
        forward((float)command->params[0], (float)command->params[1]);
      break;
    case COMMAND_REVERSE:
        sendOK();
        reverse((float) command->params[0], (float) command->params[1]);
        break;
    case COMMAND_TURN_LEFT:
        sendOK();
        left((float) command->params[0], (float) command->params[1]);
        break;
    case COMMAND_TURN_RIGHT:
        sendOK();
        right((float) command->params[0], (float) command->params[1]);
        break;
    case COMMAND_STOP:
        sendOK();
        stop();
        break;
    case COMMAND_GET_STATS:
        sendOK();
        sendStatus();
        break;
    case COMMAND_CLEAR_STATS:
        sendOK();
        clearOneCounter(command->params[0]);
        clearOneCounter(command->params[1]);
        clearOneCounter(command->params[2]);
        clearOneCounter(command->params[3]);
        clearOneCounter(command->params[4]);
        clearOneCounter(command->params[5]);
        clearOneCounter(command->params[6]);
        clearOneCounter(command->params[7]);
        clearOneCounter(command->params[8]);
        clearOneCounter(command->params[9]);
        clearOneCounter(command->params[10]);
        clearOneCounter(command->params[11]);
        clearOneCounter(command->params[12]);
        clearOneCounter(command->params[13]);
        clearOneCounter(command->params[14]);
        break;
        /*
     * Implement code for other commands here.
     * 
     */
        
    default:
      sendBadCommand();
  }
}

void waitForHello()
{
  int exit=0;

  while(!exit)
  {
    TPacket hello;
    TResult result;
    
    do
    {
      result = readPacket(&hello);
    } while (result == PACKET_INCOMPLETE);

    if(result == PACKET_OK)
    {
      if(hello.packetType == PACKET_TYPE_HELLO)
      {
     

        sendOK();
        exit=1;
      }
      else
        sendBadResponse();
    }
    else
      if(result == PACKET_BAD)
      {
        sendBadPacket();
      }
      else
        if(result == PACKET_CHECKSUM_BAD)
          sendBadChecksum();
  } // !exit
}

void setup() {
  // put your setup code here, to run once:

  alexDiagonal = sqrt((ALEX_LENGTH  * ALEX_LENGTH) + (ALEX_BREADTH * ALEX_BREADTH ));

  alexCirc = PI * alexDiagonal;
  
  //Serial.begin(9600);
  
  cli();
  setupEINT();
  setupSerial();
  startSerial();
  setupMotors();
  startMotors();
  enablePullups();
  initializeState();
  sei();
  pinMode(IRSensor, INPUT);
  pinMode(IRSensor2, INPUT);
  
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  pinMode(S0, OUTPUT);
  pinMode(S1, OUTPUT);
  pinMode(S2, OUTPUT);
  pinMode(S3, OUTPUT);
  pinMode(sensorOut, INPUT);
  digitalWrite(S0, HIGH);
  digitalWrite(S1, LOW);

}

void handlePacket(TPacket *packet)
{
  switch(packet->packetType)
  {
    case PACKET_TYPE_COMMAND:
      handleCommand(packet);
      break;

    case PACKET_TYPE_RESPONSE:
      break;

    case PACKET_TYPE_ERROR:
      break;

    case PACKET_TYPE_MESSAGE:
      break;

    case PACKET_TYPE_HELLO:
      break;
  }
}


void SensorTest(){
  ir_reading1 = digitalRead(IRSensor);
  ir_reading2 = digitalRead(IRSensor2);
    
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  // Sets the trigPin HIGH (ACTIVE) for 10 microseconds
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  // Reads the echoPin, returns the sound wave travel time in microseconds
  duration = pulseIn(echoPin, HIGH);
  // Calculating the distance
  ultra_reading = duration * 0.034 / 2; // Speed of sound wave divided by 2 (go and back)
  // Displays the distance on the Serial Monitor
/*
  Serial.print(ir_reading1);
  Serial.print(" ");
  Serial.println(ir_reading2);
  Serial.print ("Distance: ");
  Serial.print(ultra_reading);
  Serial.println(" cm");
  */
  digitalWrite(S2,LOW);
  digitalWrite(S3,LOW);
  // Reading the output frequency
  red_reading = pulseIn(sensorOut, LOW);
  //Remaping the value of the frequency to the RGB Model of 0 to 255
  red_reading = map(red_reading, 1800,2350,255,0);
  // Printing the value on the serial monitor
  /*Serial.print("R= ");
  Serial.print(red_reading);
  Serial.print(" ");
  delay(100);*/
  // Setting Green filtered photodiodes to be read
  digitalWrite(S2,HIGH);
  digitalWrite(S3,HIGH);
  // Reading the output frequency
  green_reading = pulseIn(sensorOut, LOW);
  //Remaping the value of the frequency to the RGB Model of 0 to 255
  green_reading = map(green_reading, 1850,2600,255,0);
  // Printing the value on the serial monitor
  /*Serial.print("G= ");
  Serial.print(green_reading);
  Serial.print(" ");
  delay(100);
  */
}



void loop() {

// Uncomment the code below for Step 2 of Activity 3 in Week 8 Studio 2

  //forward(0, 100);
// Uncomment the code below for Week 9 Studio 2

  
 // put your main code here, to run repeatedly:
  TPacket recvPacket; // This holds commands from the Pi

  TResult result = readPacket(&recvPacket);

 // SensorTest();
  
  
  if(result == PACKET_OK)
    handlePacket(&recvPacket);
  else
    if(result == PACKET_BAD)
    {
      sendBadPacket();
    }
    else
      if(result == PACKET_CHECKSUM_BAD)
      {
        sendBadChecksum();
      } 
  if(deltaDist > 0){
      if(dir == FORWARD){
        if(forwardDist > newDist){
          deltaDist = 0;
          newDist = 0;
          stop();
        }
      }
      else
        if(dir == BACKWARD){
          if(reverseDist > newDist){
            deltaDist = 0;
            newDist = 0;
            stop();
          }
        }
      else if(dir == STOP){
        deltaDist = 0;
        newDist = 0;
        stop();
      }
       
      }
    if(deltaTicks > 0){
      if(dir == LEFT){
        if(leftReverseTicksTurns >= targetTicks){
          deltaTicks = 0;
          targetTicks = 0;
          stop();
        }
      }

      else if(dir == RIGHT){
        if(rightReverseTicksTurns >= targetTicks){
          deltaTicks = 0;
          targetTicks = 0;
          stop();
          }
        }
      else if(dir == STOP){  
        deltaTicks = 0;
          targetTicks = 0;
          stop();
        }
      }
      SensorTest();
    }

  
