#pragma SystemFile
#define _EV3FILEIO 1
#include "PC_FileIO.c"

//global vars
const int DRIVE_POWER = 30;
const int ROTATE_POWER = 10;

const int F_LINES = 4;
string medInfoList[F_LINES];	// Ryan told us to use global string array

void configure()
{
	// gryo
	SensorType[S3] = sensorEV3_Gyro;
	wait1Msec(50) ;
	SensorMode[S3] = modeEV3Gyro_Calibration;
	wait1Msec(50);
	SensorMode[S3] = modeEV3Gyro_RateAndAngle;
	wait1Msec(50);
	// touch
	SensorType[S1] = sensorEV3_Touch;
	wait1Msec(50);
	// ir sensor
	SensorType[S4] = sensorEV3_IRSensor; //must configure sensor type AND MODE
	wait1Msec(50);
	SensorMode[S4] = modeEV3IR_Calibration;
	wait1Msec(50);
	SensorMode[S4] = modeEV3IR_Seeker;
	wait1Msec(50);
}

void rotate(int angle) {
	resetGyro(S3);
	if (angle < 0)
	{
		motor[motorA] = ROTATE_POWER;
		motor[motorD] = -ROTATE_POWER;
	}
	else
	{
		motor[motorA] = -ROTATE_POWER;
		motor[motorD] = ROTATE_POWER;
	}

	while (abs(getGyroDegrees(S3)) < abs(angle))	{}

	motor[motorA] = motor[motorD] = 0;
}

void rotate(bool towardsBeacon)
{
	eraseDisplay();
	displayCenteredBigTextLine(5, "Locate Beacon");
	if (towardsBeacon)
	{
		int direction = 0;
		while (direction == 0 || direction == 100) {
			direction = getIRBeaconDirection(S4);
		}
		int distance = 0;

		wait1Msec(1000);

		do
		{
			direction = getIRBeaconDirection(S4);
			distance = getIRBeaconStrength(S4);

			wait1Msec(1);
			if (direction > 0)
			{
				motor[motorA] = -ROTATE_POWER;
				motor[motorD] = ROTATE_POWER;
			}
			else if (direction < 0)
			{
				motor[motorA] = ROTATE_POWER;
				motor[motorD] = -ROTATE_POWER;
			}

			eraseDisplay();
		} while (!(direction < 2 && direction > -2 && distance != 0 && distance != 100));
		motor[motorA] = motor[motorD] = 0;
		wait1Msec(1000);
	}
}

void drive(int stopProximity)
{
	nMotorEncoder[motorA] = 0;
	motor[motorA] = motor[motorD] = DRIVE_POWER;
	while(getIRBeaconStrength(S4) > stopProximity) {}
	motor[motorA] = motor[motorD] = 0;
}

void dispense(int dosage)
{
	for (int i = 0; i < dosage; i++)
	{
		nMotorEncoder[motorB] = 0;

		motor[motorB] = 10;
		while (abs(nMotorEncoder[motorB]) < 360) {}
	}

	motor[motorB] = 0;
	wait1Msec(25);	// avoid turning off did not work, wait and try again
	motor[motorB] = 0;
}

void returnRobot(int encoderCount, int turnAngle)
{
	wait1Msec(1000);

	rotate(180);

	wait1Msec(500);

	nMotorEncoder[motorA] = 0;
	motor[motorA] = motor[motorD] = DRIVE_POWER;

	while (abs(nMotorEncoder[motorA]) < encoderCount) {}

	motor[motorA] = motor[motorD] = 0;

	if (turnAngle < 0)
	{
		rotate(-(180+turnAngle));
	}
	else
	{
		rotate((180-turnAngle));
	}
}

bool readQuantity(int &totalNumPills, int &dosage, int &doseTime)
{
	string errorMsg = "CAN'T OPEN QTY INFO!";
	string errorMsg1 = "Fix information file!";

	TFileHandle fin;
  bool fileOkay = openReadPC(fin, "dosageInfo.txt");

	if (fileOkay)
	{
		readIntPC(fin, totalNumPills);
		wait1Msec(100);
		readIntPC(fin, dosage);
		wait1Msec(100);
		readIntPC(fin, doseTime);
		wait1Msec(100);
	}
	else
	{
		return false;
	}
	closeFilePC(fin);
	return true;
}

void readMedInfo()
{
	TFileHandle medFile;
	bool fileOkay = openReadPC(medFile, "medInfo.txt");

	//read from file
	if (fileOkay)
	{
		for (int i = 0; i < F_LINES; i++)
		{
			readTextPC(medFile, medInfoList[i]);
		}
	}
	else
	{
		displayTextLine(0, "could not open medFile");
		wait1Msec(2000);
	}
	//close file
	closeFilePC(medFile);
}

bool pillTaken()
{
	int numLoops = 2;
	for (int i = 0; i < numLoops; i++)
	{
		for (int lineNum = 0; lineNum < F_LINES; lineNum++)
		{
			playSound(soundFastUpwardTones);
			displayCenteredBigTextLine(2, medInfoList[lineNum]);
			clearTimer(T1);
			while(time1[T1] < 2000)
			{
				//checks if pill taken button is pressed
				if (SensorValue[S1] == 1)
				{
					eraseDisplay();
					displayCenteredBigTextLine(1, "BUTTON PRESSED");
					wait1Msec(2000);
					//pill taken
					return true;
				}
			}
		}
	}
	//pill not taken
	return false;
}

void signalError(string errorMsg)
{
	eraseDisplay();
	displayCenteredBigTextLine(4, "%s", errorMsg);
	playSound(soundException);
	wait1Msec(10000);
}

void updateFile(int totalNumPills, int dosage, int doseTime)
{
	TFileHandle fout;
	bool fileOkay = openWritePC(fout, "dosageInfo.txt");

	if (fileOkay)
	{
		writeLongPC(fout, totalNumPills);
		writeEndlPC(fout);
		writeLongPC(fout, dosage);
		writeEndlPC(fout);
		writeLongPC(fout, doseTime);
	} else
	{
		displayBigTextLine(1, "bad dose file");
	}
	closeFilePC(fout);
}

task main()
{
	setSoundVolume(50);
	int totalPills = 0; //total pills in container
	int dosageFreq = 0; // pills dispensed per day
	int dosageNum = 0; //how many pills dispensed at each increment
	int doseTime = 0; // amount of time (in seconds) per dosage
	bool pillInTray = false; // if there is pill left on tray not taken
	bool error = false; // if there is error
	string errorMsg = "";

	int startTime = nPgmTime;

	readMedInfo();
	error = !readQuantity(totalPills, dosageNum, doseTime);
	if (error)
	{
		errorMsg = "CAN'T OPEN QTY";
	}
	else if (dosageNum <= 0 || doseTime <= 0)
	{
		errorMsg = "Fix QTY file!";
		error = true;
	}
	else
	{
		dosageFreq = 120/doseTime; //store 120 in const
	}

	configure();
	resetGyro(S3);
	int leaveTime = 10;

	// repeat for the number of pill needs to be taken unless there is an error
	for (int i = 0; i < dosageFreq && !error; i++) {
		resetGyro(S3);
		eraseDisplay();

		// show number of pills left
		displayCenteredBigTextLine(7, "# Pill: %d", totalPills);
		// show time from beginning of the program
		while ((nPgmTime-startTime)/1000 < leaveTime)
		{
			displayCenteredBigTextLine(5, "Time: %d", (nPgmTime-startTime)/1000);
		}

		// make sure beacon is turned on, checking multiple times
		int proximity = getIRBeaconStrength(S4);
		for (int j = 0; j < 5 && proximity == 0; j++)
		{
			proximity = getIRBeaconStrength(S4);
			wait1Msec(100);
		}

		// check for errors before driving
		if (totalPills < dosageNum || proximity == 0)
		{
			error = true;
			if (totalPills < dosageNum)
			{
				errorMsg = "ADD PILLS";
			}
			else
			{
				errorMsg = "IS BEACON ON";
			}
		}
		else
		{
			rotate(true);
			int gyroAngle = getGyroDegrees(S3);
			drive(10);
			int travelEncoderCount = abs(nMotorEncoder[motorA]);

			if (!pillInTray)
			{
				dispense(dosageNum);
				totalPills -= dosageNum*2;
			}


			pillInTray = !pillTaken();
			eraseDisplay();
			returnRobot(travelEncoderCount, gyroAngle);

			leaveTime += doseTime;
		}
	}

	if (error) {
		signalError(errorMsg);
	}
	updateFile(totalPills, dosageNum, doseTime);
	playSound(soundDownwardTones);
	while(bSoundActive) {} // wait until the powering off sound finish playing
}
