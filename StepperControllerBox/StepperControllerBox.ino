/*
 Name:		StepperControllerBox.ino
 Created:	6/25/2019 10:01:15 PM
 Author:	Mr. H
*/
#include <LiquidCrystal_I2C.h>
#include "VirtualTimer.h"
#include "fastio.h"

#define STEP 5
#define DIR 4
#define ENABLE 9

#define MS3 6
#define MS2 7
#define MS1 8

#define VOLUME_RESISTOR A0

#define DEFAULT_STEP_PER_MM 88.5732f

#define DEFAULT_PULSE_CYCLE 200
#define MAX_SPEED 50.0f //mm/s


//#define INVERT_STEPPER_DIRECTION

//Set up volume resistor
long inputValue = 0;
long newInputValue = 0;
float Speed;

uint32_t PulseCycle;
int32_t Direction;

bool isStopped = false;

unsigned long timer1;

void setup() {
	//Serial.begin(9600);
	InitIO();
	InitVirtualTimer();
	InitData();
	timer1 = millis();
}

void loop() {
	UpdateData();
}

void InitIO()
{
	pinMode(VOLUME_RESISTOR,INPUT);
	pinMode(DIR, OUTPUT);
	pinMode(ENABLE, OUTPUT);
	pinMode(STEP, OUTPUT);

	pinMode(MS1, OUTPUT);
	pinMode(MS2, OUTPUT);
	pinMode(MS3, OUTPUT);

	digitalWrite(MS1, HIGH); 
	digitalWrite(MS2, HIGH);
	digitalWrite(MS3, HIGH);

	digitalWrite(DIR, HIGH);
	digitalWrite(ENABLE, LOW);
	digitalWrite(STEP, HIGH);
	//Serial.println("Completed init IO");
}

void InitData()
{
	 inputValue = analogRead(VOLUME_RESISTOR);
	 //CheckData("inputValue", float(inputValue));
	 SetDirection(true); 

	 if (inputValue < 3)
	 {
		 VirtualTimer.Stop();
		 //Serial.println("Stopped");
		 isStopped = true;
		 return;
	 }

	 Speed = map(inputValue, 0, 1023, 0, MAX_SPEED);
	 //CheckData("Speed", Speed);
	 
	 PulseCycle = roundf(1000000 / (Speed * DEFAULT_STEP_PER_MM));
	 //CheckData("PulseCycle", float(PulseCycle));
}

void InitVirtualTimer()
{
	VirtualTimer.Init();
	VirtualTimer.Add(ToggleStepPin, DEFAULT_PULSE_CYCLE);
	VirtualTimer.Run();
}

void ToggleStepPin()
{
	TOGGLE(STEP);
}

void HandleValueChange(uint32_t *_pulseCycle)
{
	//VirtualTimer.Stop();

	VirtualTimer.Change(ToggleStepPin, (*_pulseCycle / 2));

	//VirtualTimer.Resum(ToggleStepPin);
}

void SetDirection(bool dir)
{
#ifdef INVERT_STEPPER_DIRECTION
	if (dir)
	{
		digitalWrite(DIR, LOW);
	}
	else
	{
		digitalWrite(DIR, HIGH);
	}
#else
	if (dir)
	{
		digitalWrite(DIR, HIGH);
	}
	else
	{
		digitalWrite(DIR, LOW);
	}
#endif
}

void UpdateData()
{
	if (millis() > timer1 + 1000)
	{
		timer1 = millis();
		newInputValue = analogRead(VOLUME_RESISTOR);

		if (newInputValue < 2)
		{
			if (isStopped) 
			{
				return;
			}
			else
			{
				VirtualTimer.Stop();
				//Serial.println("Stopped");
				isStopped = true;
				return;
			}
		}
		else
		{
			if (isStopped) 
			{
				VirtualTimer.Resum(ToggleStepPin);
				isStopped = false;
				delay(200);
			}
		}

		if (newInputValue == inputValue)
		{
			return;
		}
		else
		{
			inputValue = newInputValue;
			Speed = map(newInputValue, 0, 1023, 0, MAX_SPEED);
			PulseCycle = roundf(1000000 / (Speed * DEFAULT_STEP_PER_MM));
			//CheckData("PulseCycle", float(PulseCycle));
			HandleValueChange(&PulseCycle);
		}
	}
}

//void CheckData(String _str, float value) {
//	Serial.print(_str);
//	Serial.println(value);
//}