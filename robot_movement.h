// System calls
#include <unistd.h>
// Interfaces with GPIO
#include "driver/gpio_control.h"
// Interfaces with IMU sensor
#include "driver/imu_sensor.h"
// Holds data from IMU sensor
#include "driver/imu_data.h"
// Communicates with MATRIX device
#include "driver/matrixio_bus.h"

void gpioInit(matrix_hal::GPIOControl *gpio);
bool movementStraight(matrix_hal::GPIOControl *gpio, 
					  matrix_hal::IMUData *imu_data,
					  matrix_hal::IMUSensor *imu_sensor, 
					  char direction, float distance);
bool movementTurn (matrix_hal::GPIOControl *gpio, 
				   matrix_hal::IMUData *imu_data,
				   matrix_hal::IMUSensor *imu_sensor,
				   char direction, char turnType, int setAngle);
