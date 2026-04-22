# GSM Home Controller (AVR) - Work in Progress

## Project Overview
This is an embedded C project developed for the **ATmega16** microcontroller. The primary objective is to create a versatile controller capable of managing electrical outputs based on various inputs, including **SMS commands (GSM module)**, **time-based scheduling (RTC)**, and **sound triggers (Clap Switch)**.

The project demonstrates a hands-on approach to low-level programming and hardware-software interaction in an embedded environment.

## Key Features & Tech Stack
* **State Machine Architecture:** Implemented a robust state machine (`enum Tryb`) to manage multiple operation modes (GSM, Bluetooth, Potentiometer control, Time Scheduling, and Sound Control).
* **Communication Protocols:**
    * **UART:** Integration with a GSM module using AT commands for remote control and message parsing.
    * **I2C (TWI):** Interfacing with a Real-Time Clock (DS1307/similar) for precise time-based output triggering.
* **Peripheral Management:**
    * **ADC:** Used for sound detection (Clap Switch logic) and reading analog values from potentiometers or sensors.
    * **PWM (Fast PWM Mode):** Power output control with adjustable duty cycle.
    * **Interrupts:** Efficient handling of serial data reception (USART_RX) and timer-based tasks (display refreshing).
* **Hardware:** AVR ATmega16, HD44780 LCD, GSM/BT modules, Triac/Relay outputs.
* **Environment:** Developed using **Eclipse IDE with AVR plugin**.

## Current Status
* [x] Hardware abstraction layer for GPIO, PWM, and ADC.
* [x] I2C Communication with RTC and time-matching logic.
* [x] Menu system for manual mode switching via hardware buttons.
* [x] Real-time status reporting on HD44780 LCD.
* [ ] **In Progress:** Refining the circular buffer logic for GSM message parsing and improving ASPICE-aligned documentation for the project.

## How to build
The project is configured for a CPU frequency of **8MHz** (defined in the IDE/Makefile). It requires the `avr-gcc` compiler and `avr-libc`.
