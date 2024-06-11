/**
 * Includes all servo functions
 *
 * @author    Florian Staeblein
 * @date      2024/04/12
 * @copyright © 2024 Florian Staeblein
 */
 
#ifndef SERVO_H
#define SERVO_H

//===============================================================
// Inlcudes
//===============================================================
#include <Arduino.h>
#include "Driver/ledc.h"


// Values for SG90 servos; adjust if needed
#define MIN_MICROS      800
#define MAX_MICROS      2450

//===============================================================
// Servo class
//===============================================================
class Servo
{
  public:
    // Constructor
    Servo(uint8_t pin);

    // Sets the angle of the servo
    void SetAngle(int16_t value);

  private:
    uint8_t _pin;
    uint32_t _frequency = 50;    // PWM frequency (Hz)
    uint8_t _resolution = 12;    // Bits, determines the number of steps in PWM period
    uint16_t _servoMinUs = 544;  // Minimum servo pulse in μs (544)
    uint16_t _servoMaxUs = 2400; // Maximum servo pulse in μs (2400)
};

#endif
