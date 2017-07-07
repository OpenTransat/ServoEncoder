# ServoEncoder
Controls a motor by evaluating position feedback from a magnetic encoder

## Features

* Based on Atmega328P @ 2 MHz

* Absolute magnetic rotary encoder EMS22A is mounted on the PCB

* Used with JGY-2838 or a similar motor controlled by a two-wire interface

* The two-wire output can be used as serial RX/TX for debugging

* One can use a stepper motor by connecting the two-wire output to a stepper motor driver

* PWM input like a typical servo

* It provides a feedback to the main controller to determine if the motor is overloaded or stuck

![ServoEncoder PCB](https://github.com/OpenTransat/ServoEncoder/blob/master/images/servoencoder.jpg "ServoEncoder PCB")

It has been designed as a robust solution for steering the rudder on autonomous boat. The life span has been tested with a worm geared motor JGY-2838.

Ongoing test: 3 million cycles

## Video: Testing worm drive life span

[![Testing worm drive life span](http://img.youtube.com/vi/ynh-ik3KGtM/0.jpg)](https://www.youtube.com/watch?v=ynh-ik3KGtM)
