/**
 * Includes all servo functions
 *
 * @author    Florian Staeblein
 * @date      2024/04/12
 * @copyright © 2024 Florian Staeblein
 */
 
//===============================================================
// Includes
//===============================================================
#include "Servo.h"

//===============================================================
// Servo constructor
//===============================================================
Servo::Servo(uint8_t pin)
{
  _pin = pin;

  // PWM frequency 50Hz
  // 12 bits resolution
  ledcAttach(_pin, _frequency, _resolution);
}

//===============================================================
// Sets the angle of the servo
//===============================================================
void Servo::SetAngle(int16_t value)
{
  value = min(value, (int16_t)180);
  value = max(value, (int16_t)0);

  int16_t pwmValueUs = map(value, 0, 180, _servoMinUs, _servoMaxUs);
  int16_t duty = pwmValueUs * 4096 / 20000;
  
  // Write value
  ledcWrite(_pin, duty);
}
