#ifndef _MOTOR_H_
#define _MOTOR_H_

void motor_set_speed(int speed);
void motor_stop();
void motor_cw();
void motor_ccw();
void motor_setup();
void motor_brake();
void motor_home();
uint16_t motor_homing_loop();

#endif
