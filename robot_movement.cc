#include "assistant/robot_movement.h"
#include <iostream>

// GPIOOutputMode is 1
const uint16_t GPIOOutputMode = 1;
// GPIOInputMode is 0
const uint16_t GPIOInputMode = 0;
// PWMFunction is 1
const uint16_t PWMFunction = 1;

// Holds desired GPIO pin [0-15]
const uint16_t ENA = 0;
const uint16_t IN1 = 1;
const uint16_t IN2 = 2;
const uint16_t IN3 = 3;
const uint16_t IN4 = 4;
const uint16_t ENB = 5;

void gpioInit(matrix_hal::GPIOControl *gpio) {
	// Set pin mode to output
	gpio->SetMode(ENA, GPIOOutputMode);
	gpio->SetMode(IN1, GPIOOutputMode);
	gpio->SetMode(IN2, GPIOOutputMode);
	gpio->SetMode(IN3, GPIOOutputMode);
	gpio->SetMode(IN4, GPIOOutputMode);
	gpio->SetMode(ENB, GPIOOutputMode);
  
	// Set pin function to PWM
	gpio->SetFunction(ENA, PWMFunction);
	gpio->SetFunction(ENB, PWMFunction);
}

bool movementStraight(matrix_hal::GPIOControl *gpio, 
					  matrix_hal::IMUData *imu_data,
					  matrix_hal::IMUSensor *imu_sensor, 
					  char direction, float distance) {
	// Set pin_out to output pin_out_state
	if(direction == 'f') {
		gpio->SetGPIOValue(IN1, 1);
		gpio->SetGPIOValue(IN2, 0);
		gpio->SetGPIOValue(IN3, 1);
		gpio->SetGPIOValue(IN4, 0);
	} else if (direction == 'b') {
		gpio->SetGPIOValue(IN1, 0);
		gpio->SetGPIOValue(IN2, 1);
		gpio->SetGPIOValue(IN3, 0);
		gpio->SetGPIOValue(IN4, 1);
	}

	// Velocity ~ 1.25 m/s
	// Distance to travel => Time to travel
	float duration = distance/1.25;

	// Holds desired PWM frequency
	float freqA = 50;
	float freqB = 50;
	// Holds desired PWM duty percentage
	float percentA = 30;
	float percentB = 30;
	gpio->SetPWM(freqA, percentA, ENA);
	gpio->SetPWM(freqB, percentB, ENB);
	
	// read IMU and get current yaw
	imu_sensor->Read(imu_data);
	float orgYaw = imu_data->yaw;

	// each loop lasts approx. 50ms => # of loops = (1/50ms)*duration [s]
	if (direction == 'f') {
		for (int i = 0; i < duration*20; i++) {
			imu_sensor->Read(imu_data);
			float newYaw = imu_data->yaw;
			if(newYaw == orgYaw) { // no correction needed
				percentA = 30;
				percentB = 30;
				gpio->SetPWM(freqA, percentA, ENA);
				gpio->SetPWM(freqB, percentB, ENB);
			} else if (newYaw < orgYaw) { // correction to the right needed
				percentA = 40;
				percentB = 25;
				gpio->SetPWM(freqA, percentA, ENA);
				gpio->SetPWM(freqB, percentB, ENB);
			} else if (newYaw > orgYaw) { // correction to the left needed
				percentA = 25;
				percentB = 40;
				gpio->SetPWM(freqA, percentA, ENA);
				gpio->SetPWM(freqB, percentB, ENB);
			}
			usleep(50000);
		}
	} else if (direction == 'b') {
		for (int i = 0; i < duration*20; i++) {
			imu_sensor->Read(imu_data);
			float newYaw = imu_data->yaw;
			if(newYaw == orgYaw) { // no correction needed
				percentA = 30;
				percentB = 30;
				gpio->SetPWM(freqA, percentA, ENA);
				gpio->SetPWM(freqB, percentB, ENB);
			} else if (newYaw > orgYaw) { // correction to the left needed
				percentA = 32;
				percentB = 28;
				gpio->SetPWM(freqA, percentA, ENA);
				gpio->SetPWM(freqB, percentB, ENB);
			} else if (newYaw < orgYaw) { // correction to the right needed
				percentA = 28;
				percentB = 32;
				gpio->SetPWM(freqA, percentA, ENA);
				gpio->SetPWM(freqB, percentB, ENB);
			}
			usleep(50000);
		}
	}
	
	percentA = 0;
	percentB = 0;
	gpio->SetPWM(freqA, percentA, ENA);
	gpio->SetPWM(freqB, percentB, ENB);
	
	return true;
}

bool movementTurn (matrix_hal::GPIOControl *gpio, 
				   matrix_hal::IMUData *imu_data,
				   matrix_hal::IMUSensor *imu_sensor,
				   char direction, char turnType, int setAngle) {
	// Holds desired PWM frequency
	float freqA = 50;
	float freqB = 50;
	// Holds desired PWM duty percentage
	float percentA = 30;
	float percentB = 30;
	
	if (direction == 'r') {
		// Set pin_out to output pin_out_state
		if (turnType == 'p') {
			gpio->SetGPIOValue(IN1, 0);
			gpio->SetGPIOValue(IN2, 1);
			gpio->SetGPIOValue(IN3, 1);
			gpio->SetGPIOValue(IN4, 0);
		} else if (turnType == 's') {
			gpio->SetGPIOValue(IN1, 0);
			gpio->SetGPIOValue(IN2, 0);
			gpio->SetGPIOValue(IN3, 1);
			gpio->SetGPIOValue(IN4, 0);
		}
		
		gpio->SetPWM(freqA, percentA, ENA);
		gpio->SetPWM(freqB, percentB, ENB);

		float angle = 0;

		// Endless loop
		while (angle > -1*setAngle) {
		  // Overwrites imu_data with new data from IMU sensor
		  imu_sensor->Read(imu_data);
		  
		  // Read Gyroscope Z axis & compute angle of rotation (yaw)
		  float gyro_Z = imu_data->gyro_z;
		  angle += gyro_Z*0.02*8/5;
		  // Sleep for 20000 microseconds
		  usleep(20000);
		}
		std::cout << "Angle of rotation = " << angle << std::endl;
	} else if (direction == 'l') {
		if (turnType == 'p') {
			// Set pin_out to output pin_out_state
			gpio->SetGPIOValue(IN1, 1);
			gpio->SetGPIOValue(IN2, 0);
			gpio->SetGPIOValue(IN3, 0);
			gpio->SetGPIOValue(IN4, 1);
		} else if (turnType == 's') {
			gpio->SetGPIOValue(IN1, 1);
			gpio->SetGPIOValue(IN2, 0);
			gpio->SetGPIOValue(IN3, 0);
			gpio->SetGPIOValue(IN4, 0);
			
		}
		
		gpio->SetPWM(freqA, percentA, ENA);
		gpio->SetPWM(freqB, percentB, ENB);

		float angle = 0;

		// Endless loop
		while (angle < setAngle) {
		  // Overwrites imu_data with new data from IMU sensor
		  imu_sensor->Read(imu_data);
		  
		  // Read Gyroscope Z axis & compute angle of rotation (yaw)
		  float gyro_Z = imu_data->gyro_z;
		  angle += gyro_Z*0.02*8/5;
		  std::cout << "Angle of rotation = " << angle << std::endl;
		  // Sleep for 20000 microseconds
		  usleep(20000);
		}
		std::cout << "Angle of rotation = " << angle << std::endl;
	}
	
	// turn off motors
	percentA = 0;
	percentB = 0;
	
	gpio->SetPWM(freqA, percentA, ENA);
	gpio->SetPWM(freqB, percentB, ENB);
	
	return true;
}
