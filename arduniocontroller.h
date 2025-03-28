/*  arduinocontroller.h
 *  This library gives you a standard way to create Arduino code that talks
 *   to the UnoJoy firmware in order to make native USB game controllers.
 *  Functions:
 *   setupUnoJoy()
 *   getBlankDataForController()
 *   setControllerData(dataForController_t dataToSet)
 *
 *   === How to use this library ===
 *
 *  To use this library to make a controller, you'll need to do 3 things:
 *   Call setupUnoJoy(); in the setup() block
 *   Create and populate a dataForController_t type variable and fill it with your data
 *         The getBlankDataForController() function is good for that.
 *   Call setControllerData(yourData); where yourData is the variable from above,
 *         somewhere in your loop(), once you're ready to push your controller data to the system.
 *         If you forget to call sendControllerData in your loop, your controller won't ever do anything
 *
 *  You can then debug the controller with the included Processing sketch, UnoJoyProcessingVisualizer
 *
 *  To turn it into an actual USB video game controller, you'll reflash the
 *   Arduino's communication's chip using the instructions found in the 'Firmware' folder,
 *   then unplug and re-plug in the Arduino.
 *
 *  Details about the dataForController_t type are below, but in order to create and use it,
 *   you'll declare it like:
 *
 *      dataForController_t ControllerData;
 *
 *   and then control button presses and analog stick movement with statements like:
 *
 *      ControllerData.triangleOn = 1;   // Marks the triangle button as pressed
 *      ControllerData.squareOn = 0;     // Marks the square button as unpressed
 *      ControllerData.leftStickX = 90;  // Analog stick values can range from 0 - 255
 */

#ifndef UNOJOY_H
#define UNOJOY_H
    #include <stdint.h>
    #include <util/atomic.h>
    #include <Arduino.h>

	typedef struct dataForController_t
	{
		uint8_t triangleOn : 1;  // Each of these member variables
		uint8_t circleOn : 1;    //  control if a button is off or on
		uint8_t squareOn : 1;    // For the buttons,
		uint8_t crossOn : 1;     //  0 is off
		uint8_t l1On : 1;        //  1 is on
		uint8_t l2On : 1;
		uint8_t l3On : 1;        // The : 1 here just tells the compiler
		uint8_t r1On : 1;        //  to only have 1 bit for each variable.
                                 //  This saves a lot of space for our type!
		uint8_t r2On : 1;
		uint8_t r3On : 1;
		uint8_t selectOn : 1;
		uint8_t startOn : 1;
		uint8_t homeOn : 1;
		uint8_t dpadLeftOn : 1;
		uint8_t dpadUpOn : 1;
		uint8_t dpadRightOn : 1;

		uint8_t dpadDownOn : 1;
        uint8_t padding : 7;     // We end with 7 bytes of padding to make sure we get our data aligned in bytes

		uint8_t leftStickX : 8;  // Each of the analog stick values can range from 0 to 255
		uint8_t leftStickY : 8;  //  0 is fully left or up
		uint8_t rightStickX : 8; //  255 is fully right or down
		uint8_t rightStickY : 8; //  128 is centered.

	} dataForController_t;

    // Call setupUnoJoy in the setup block of your program.
    //  It sets up the hardware UnoJoy needs to work properly
    void setupUnoJoy(void);

    // You can also call the set
    void setupUnoJoy(int);

    // This sets the controller to reflect the button and
    // joystick positions you input (as a dataForController_t).
    // The controller will just send a zeroed (joysticks centered)
    // signal until you tell it otherwise with this function.
    void setControllerData(dataForController_t);

    // This function gives you a quick way to get a fresh
    //  dataForController_t with:
    //    No buttons pressed
    //    Joysticks centered
    // Very useful for starting each loop with a blank controller, for instance.
    // It returns a dataForController_t, so you want to call it like:
    //    myControllerData = getBlankDataForController();
    dataForController_t getBlankDataForController(void);


//----- End of the interface code you should be using -----//
//----- Below here is the actual implementation of

  // This dataForController_t is used to store
  //  the controller data that you want to send
  //  out to the controller.
  dataForController_t controllerDataBuffer;

  // This updates the data that the controller is sending out.
  //  The system actually works as following:
  //  The UnoJoy firmware on the ATmega8u2 regularly polls the
  //  Arduino chip for individual bytes of a dataForController_t.
  //
  void setControllerData(dataForController_t controllerData){
    // Probably unnecessary, but this guarantees that the data
    //  gets copied to our buffer all at once.
    ATOMIC_BLOCK(ATOMIC_FORCEON){
      controllerDataBuffer = controllerData;
    }
  }


  volatile int serialCheckInterval = 1;
  // This is an internal counter variable to count ms between
  //  serial check times
  int serialCheckCounter = 0;

  // This is the setup function - it sets up the serial communication
  //  and the timer interrupt for actually sending the data back and forth.
  void setupUnoJoy(void){
    // First, let's zero out our controller data buffer (center the sticks)
    controllerDataBuffer = getBlankDataForController();

    // Start the serial port at the specific, low-error rate UnoJoy uses.
    //  If you want to change the rate, you'll have to change it in the
    //  firmware for the ATmega8u2 as well.
    Serial.begin(38400);

    // Now set up the Timer 0 compare register A
    //  so that Timer0 (used for millis() and such)
    //  also fires an interrupt when it's equal to
    //  128, not just on overflow.
    // This will fire our timer interrupt almost
    //  every 1 ms (1024 us to be exact).
    OCR0A = 128;
    TIMSK0 |= (1 << OCIE0A);
  }

  // If you really need to change the serial polling
  //  interval, use this function to initialize UnoJoy.
  //  interval is the polling frequency, in ms.
  void setupUnoJoy(int interval){
    serialCheckInterval = interval;
    setupUnoJoy();
  }

  // This interrupt gets called approximately once per ms.
  //  It counts how many ms between serial port polls,
  //  and if it's been long enough, polls the serial
  //  port to see if the UnoJoy firmware requested data.
  //  If it did, it transmits the appropriate data back.
  ISR(TIMER0_COMPA_vect){
    serialCheckCounter++;
    if (serialCheckCounter >= serialCheckInterval){
      serialCheckCounter = 0;
      // If there is incoming data stored in the Arduino serial buffer
      while (Serial.available() > 0) {
        pinMode(13, OUTPUT);
        //digitalWrite(13, HIGH);
        // Get incoming byte from the ATmega8u2
        byte inByte = Serial.read();
        // That number tells us which byte of the dataForController_t struct
        //  to send out.
        Serial.write(((uint8_t*)&controllerDataBuffer)[inByte]);
        //digitalWrite(13, LOW);
      }
    }
  }

  // Returns a zeroed out (joysticks centered)
  //  dataForController_t variable
  dataForController_t getBlankDataForController(void){
    // Create a dataForController_t
    dataForController_t controllerData;
    // Make the buttons zero
    controllerData.triangleOn = 0;
    controllerData.circleOn = 0;
    controllerData.squareOn = 0;
    controllerData.crossOn = 0;
    controllerData.l1On = 0;
    controllerData.l2On = 0;
    controllerData.l3On = 0;
    controllerData.r1On = 0;
    controllerData.r2On = 0;
    controllerData.r3On = 0;
    controllerData.dpadLeftOn = 0;
    controllerData.dpadUpOn = 0;
    controllerData.dpadRightOn = 0;
    controllerData.dpadDownOn = 0;
    controllerData.selectOn = 0;
    controllerData.startOn = 0;
    controllerData.homeOn = 0;
    //Set the sticks to 128 - centered
    controllerData.leftStickX = 128;
    controllerData.leftStickY = 128;
    controllerData.rightStickX = 128;
    controllerData.rightStickY = 128;
    // And return the data!
    return controllerData;
  }

#endif
