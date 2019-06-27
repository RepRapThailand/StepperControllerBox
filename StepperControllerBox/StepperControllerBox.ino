/*
 Name:		StepperControllerBox.ino
 Created:	6/25/2019 10:01:15 PM
 Author:	Mr. H
*/
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>
#include "VirtualTimer.h"
#include "StableButton.h"
#include "LCDMenu.h"
#include "fastio.h"

#define STEP 5
#define DIR A0
#define ENABLE A2

#define CLK 4
#define DT 3
#define SW 2

#define DT_PORT PIND
#define DT_BIT PIND3
#define CLK_PORT PIND
#define CLK_BIT PIND4

#define DEFAULT_POSITION 0
#define DEFAULT_STEP_PER_MM 12.0f
#define DEFAULT_PULSE_CYCLE 80
#define DEFAULT_SPEED 0.1f // mm/s

#define INVERT_STEPPER_DIRECTION
#define INVERT_ENCODER_DIRECTION

//#define SPEED_RESOLUTION 0.01
//#define POSITION_RESOLUTION 0.1
//#define STEP_PER_MM_RESOLUTION 100

struct Data
{
	float lastPos;
	float stepPerMm;
	float lastSpeed;
};

//Set up Rotary Encoder
uint8_t ButtonArray[] = { SW };
uint8_t lastFlag = 0;
uint8_t dataFlag = 0;
uint8_t clockFlag = 0;
signed long currentEncoderPosition = 0;
signed long lastEncoderPosition = 0;

//Set up LCD
LiquidCrystal_I2C *lcd = new LiquidCrystal_I2C(0x27, 16, 2);

OriginMenu* firstMenu;
Label* lbPosition;
VariableText* vbPosition;
FunctionText* ftStart;
OriginMenu* secondMenu;
Label* lbStepPerMm;
VariableText* vbStepPerMm;
Label* lbSpeed;
VariableText* vbSpeed;
OriginMenu* thirdMenu;
FunctionText* ftSaveAll;

FunctionText* ftReturn;
FunctionText* ftReturn1;
FunctionText* ftReturn2;
FunctionText* ftReturn3;

//set up Stepper
float DesiredPosition;
float CurrentPosition;
int32_t DesiredSteps;
volatile int32_t CurrentSteps;

float StepPerMm;
float Speed;
float LastSpeed;

uint32_t PulseCycle;
int32_t Direction;

bool isStarted = false;
unsigned long lastMillis;

void setup() {
	LCDMenu.Init(lcd, "3D Ceramic");
	EEPROM.begin();
	//Serial.begin(115200);
	StableButton.Init(ButtonArray, 1);

	InitEveryThing();
	InitVirtualTimer();
}

void loop() {
	Count();
	ExecuteButon();
	LCDMenu.ExecuteEffect();
	LCDMenu.UpdateScreen();
	UpdateData();
}

void InitEveryThing() {
	InitIO();
	InitData();
	//CheckData();
	InitLcdMenu();
	lastMillis = millis();
}

void InitLcdMenu()
{
	firstMenu = new OriginMenu();
	{
		lbPosition = new Label(firstMenu, "MoveTo:", 0, 0);
		vbPosition = new VariableText(firstMenu, 0, 11, 0);
		vbPosition->Resolution = 0.1;
		vbPosition->SetValue(CurrentPosition);
		vbPosition->HandleWhenValueChange = HandleValueChanger;
		ftStart = new FunctionText(firstMenu, "Start", 0, 1);
		ftStart->Function = StartMoving;
		ftReturn = new FunctionText(firstMenu, "<-", 14, 1);
		ftReturn->Function = LCDReturn;
	}

	secondMenu = new OriginMenu();
	{
		lbStepPerMm = new Label(secondMenu, "STEP/MM:", 0, 0);
		vbStepPerMm = new VariableText(secondMenu, 0, 9, 0);
		vbStepPerMm->Resolution = 100;
		vbStepPerMm->SetValue(StepPerMm);
		vbStepPerMm->HandleWhenValueChange = HandleValueChanger;
		lbSpeed = new Label(secondMenu, "Speed:", 0, 1);
		vbSpeed = new VariableText(secondMenu, 0, 7, 1);
		vbSpeed->Resolution = 0.01;
		vbSpeed->SetValue(Speed);
		vbSpeed->HandleWhenValueChange = HandleValueChanger;
		ftReturn1 = new FunctionText(secondMenu, "<-", 14, 1);
		ftReturn1->Function = LCDReturn;
	}

	thirdMenu = new OriginMenu();
	{
		ftSaveAll = new FunctionText(thirdMenu, "Save All", 0, 0);
		ftSaveAll->Function = SaveDataToEEPROM;
		ftReturn2 = new FunctionText(thirdMenu, "<-", 14, 1);
		ftReturn2->Function = LCDReturn;
	}

	LCDMenu.AddMenu(firstMenu);
	LCDMenu.AddMenu(secondMenu);
	LCDMenu.AddMenu(thirdMenu);
	LCDMenu.SetCurrentMenu(firstMenu);
	LCDMenu.UpdateScreen();
}

void InitIO()
{
	pinMode(DT, INPUT);
	pinMode(CLK, INPUT);
	lastFlag = bitRead(DT_PORT, DT_BIT);

	pinMode(DIR, OUTPUT);
	pinMode(ENABLE, OUTPUT);
	pinMode(STEP, OUTPUT);

	digitalWrite(DIR, HIGH);
	digitalWrite(ENABLE, LOW);
	digitalWrite(STEP, HIGH);
}

void InitData()
{
	Data data;
	EEPROM.get(0, data);

	if (data.stepPerMm != 0)
	{
		Speed = data.lastSpeed;
		LastSpeed = Speed;
		StepPerMm = data.stepPerMm;
		CurrentPosition = data.lastPos;
		DesiredPosition = CurrentPosition;
		CurrentSteps = CurrentPosition * StepPerMm;
		DesiredSteps = CurrentSteps;
		PulseCycle = roundf(1000000 / (Speed * StepPerMm));
	}
	else
	{
		DesiredPosition = CurrentPosition = DEFAULT_POSITION;
		DesiredSteps = CurrentSteps = CurrentSteps * StepPerMm;
		Speed = DEFAULT_SPEED;
		StepPerMm = DEFAULT_STEP_PER_MM;
		PulseCycle = DEFAULT_PULSE_CYCLE;
	}
}

void InitVirtualTimer()
{
	VirtualTimer.Init();
	VirtualTimer.Add(ToggleStepPin, PulseCycle / 2);
	VirtualTimer.Run();
}

void Count()
{
	dataFlag = bitRead(DT_PORT, DT_BIT);
	if (lastFlag != dataFlag)
	{
		clockFlag = bitRead(CLK_PORT, CLK_BIT);
#ifdef INVERT_ENCODER_DIRECTION
		if (dataFlag == clockFlag)
		{
			currentEncoderPosition++;
		}
		else
		{
			currentEncoderPosition--;
		}
#else
		if (dataFlag == clockFlag)
		{
			currentEncoderPosition--;
		}
		else
		{
			currentEncoderPosition++;
		}
#endif
		lastFlag = dataFlag;
	}
}

void ExecuteButon()
{
	if (StableButton.IsPressed(SW))
	{
		LCDMenu.Enter();
	}

	if (currentEncoderPosition - lastEncoderPosition > 1)
	{
		LCDMenu.MoveCursorLeft();
		lastEncoderPosition = currentEncoderPosition;
	}

	if (lastEncoderPosition - currentEncoderPosition > 1)
	{
		LCDMenu.MoveCursorRight();
		lastEncoderPosition = currentEncoderPosition;
	}

	//SaveValueAfterEndEditing();
}

void LCDReturn(DisplayElement* element)
{
	LCDMenu.Return();
}

void ToggleStepPin()
{
	if (CurrentSteps == DesiredSteps)
		return;

	TOGGLE(STEP);
	CurrentSteps += Direction;
	//Serial.println(CurrentSteps);
}

void SaveDataToEEPROM(DisplayElement* element)
{
	Data data;
	data.lastPos = CurrentPosition;
	data.lastSpeed = Speed;
	data.stepPerMm = StepPerMm;
	EEPROM.put(0, data);

	//Serial.println("Data saved");
}

void HandleValueChanger()
{
	if (vbPosition->GetValue() != CurrentPosition)
	{
		DesiredPosition = vbPosition->GetValue();
	}

	if (vbSpeed->GetValue() != Speed)
	{
		Speed = vbSpeed->GetValue();
	}

	if (vbStepPerMm->GetValue() != StepPerMm)
	{
		StepPerMm = vbStepPerMm->GetValue();
	}
}

void StartMoving(DisplayElement * dis)
{
	//checkValue("currentPos", CurrentPosition);
	//checkValue("DesiredPos", DesiredPosition);

	if (!isStarted)
	{
		isStarted = true;
		ftStart->SetText("Stop");

		if (LastSpeed != Speed)
		{
			LastSpeed = Speed;
			PulseCycle = roundf(1000000.0 / (LastSpeed * StepPerMm));
			VirtualTimer.Change(ToggleStepPin, PulseCycle / 2);
			//checkValue("PulseCycle", PulseCycle);
		}

		MoveTo(DesiredPosition);
	}
	else
	{
		isStarted = false;
		ftStart->SetText("Start");
		DesiredSteps = CurrentSteps;
	}
	
}

void MoveTo(float pos)
{
	int32_t tempDesiredSteps;
	tempDesiredSteps = int32_t(pos * StepPerMm);

	//Serial.print("DesiredSteps: ");
	//Serial.println(tempDesiredSteps);

	int32_t jump = tempDesiredSteps - CurrentSteps;
	float dir = jump / abs(jump);
	if (dir > 0)
	{
		SetDirection(true);
	}
	else
	{
		SetDirection(false);
	}

	//Serial.print("CurrentSteps: ");
	//Serial.println(CurrentSteps);


	//Serial.print("dir: ");
	//Serial.println(dir);

	DesiredSteps = tempDesiredSteps;
}

void SetDirection(bool dir)
{
#ifdef INVERT_STEPPER_DIRECTION
	if ( dir)
	{
		Direction = 1;
		digitalWrite(DIR, LOW);
	}
	else
	{
		Direction = -1;
		digitalWrite(DIR, HIGH);
	}
#else
	if (dir > 0)
	{
		Direction = 1;
		digitalWrite(DIR, HIGH);
	}
	else
	{
		Direction = -1;
		digitalWrite(DIR, LOW);
	}
#endif
}

void UpdateData()
{
	if (millis() > lastMillis + 300)
	{
		lastMillis = millis();
		
		if (CurrentSteps == DesiredSteps)
		{
			CurrentPosition = DesiredPosition;
			isStarted = false;
			ftStart->SetText("Start");
		}

		float crtSteps = (float)CurrentSteps;
		CurrentPosition = crtSteps / StepPerMm;
	}
}

//void checkValue(String name, float val)
//{
//	//Serial.print(name + ":  ");
//	//Serial.println(val);
//}
//
//void CheckData()
//{
//	checkValue("currentPos", CurrentPosition);
//	checkValue("DesiredPos", DesiredPosition);
//	checkValue("CurrentSteps", CurrentSteps);
//	checkValue("DesiredSteps", DesiredSteps);
//	checkValue("StepPerMm", StepPerMm);
//	checkValue("Speed", Speed);
//	checkValue("PulseCycle", PulseCycle);
//}