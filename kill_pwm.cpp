/*
 * GPIO PWM output example
 */

/////////////////////////
// INCLUDE STATEMENTS //
///////////////////////

// System calls
#include <unistd.h>
// Input/output streams and functions
#include <iostream>

// Interfaces with GPIO
#include "matrix_hal/gpio_control.h"
// Communicates with MATRIX device
#include "matrix_hal/matrixio_bus.h"

////////////////////////
// INITIAL VARIABLES //
//////////////////////

// GPIOOutputMode is 1
const uint16_t GPIOOutputMode = 1;
// GPIOInputMode is 0
const uint16_t GPIOInputMode = 0;
// PWMFunction is 1
const uint16_t PWMFunction = 1;

// Holds desired PWM frequency
float freqA;
float freqB;
// Holds desired PWM duty percentage
float percentA;
float percentB;

// Holds desired GPIO pin [0-15]
const uint16_t ENA = 0;
const uint16_t IN1 = 1;
const uint16_t IN2 = 2;
const uint16_t IN3 = 3;
const uint16_t IN4 = 4;
const uint16_t ENB = 5;

int main() {
  ////////////////////
  // INITIAL SETUP //
  //////////////////

  // Create MatrixIOBus object for hardware communication
  matrix_hal::MatrixIOBus bus;
  // Initialize bus and exit program if error occurs
  if (!bus.Init()) return false;

  /////////////////
  // MAIN SETUP //
  ///////////////

  // Create GPIOControl object
  matrix_hal::GPIOControl gpio;
  // Set gpio to use MatrixIOBus bus
  gpio.Setup(&bus);

  // Set pin mode to output
  gpio.SetMode(ENA, GPIOOutputMode);
  gpio.SetMode(IN1, GPIOOutputMode);
  gpio.SetMode(IN2, GPIOOutputMode);
  gpio.SetMode(IN3, GPIOOutputMode);
  gpio.SetMode(IN4, GPIOOutputMode);
  gpio.SetMode(ENB, GPIOOutputMode);
  
  // Set pin function to PWM
  gpio.SetFunction(ENA, PWMFunction);
  gpio.SetFunction(ENB, PWMFunction);
  
  // Set pin_out to output pin_out_state
  gpio.SetGPIOValue(IN1, 1);
  gpio.SetGPIOValue(IN2, 0);
  gpio.SetGPIOValue(IN3, 1);
  gpio.SetGPIOValue(IN4, 0);
  
  percentA = 0;
  percentB = 0;

  // If setting PWM returns an error, log it
  // SetPWM function carries out PWM logic and outputs PWM signal
  if (!gpio.SetPWM(freqA, percentA, ENA) ||
	  !gpio.SetPWM(freqB, percentB, ENB))
    // Output error to console
    std::cerr << "ERROR: invalid input" << std::endl;
  else
    // Else output GPIO PWM info to console
    std::cout << "Let them wheels turn" << std::endl;

  return 0;
}
