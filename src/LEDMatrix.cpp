/*
 * LEDMatrix.cpp
 *
 *  Created on: 20.02.2018
 *      Author: Julian
 */

#include "LEDMatrix.h"
#include "Arduino.h"

int ledMatrix[10][11];
String debugLog;

int clockLanguage;
bool sec1Active;
bool sec3Active;

int colorTempRgb[3];

LEDMatrix::LEDMatrix() {
	String debugLog="";
	colorTempRgb[0]=255;
	colorTempRgb[1]=255;
	colorTempRgb[2]=255;

	clockLanguage=0;
	sec1Active=true;
	sec3Active=true;
}

LEDMatrix::~LEDMatrix() {
	// TODO Auto-generated destructor stub
}

String LEDMatrix::getDebug(){
	return debugLog;
}

void LEDMatrix::insertWord(int row, int array[]) {
	for(int i=0; i<11;i++){
		if(array[i]>-1){
			ledMatrix[row][i] = array[i];
		}
	}
}

void LEDMatrix::clearMatrix() {
	for(int i=0;i<10;i++){
		//rows
		for(int j=0;j<11;j++){
			//Cols
			ledMatrix[i][j]=-1;
		}
	}
}

void LEDMatrix::setStandby(String mode){
	//clear matrix
	//memset(ledMatrix, -1, sizeof(ledMatrix[0][0]) * 10 * 11);
	clearMatrix();

	int* tmpArray;
	tmpArray = new int[11] {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};

	if(mode.equals("JUS")){
		tmpArray = new int[11] {-1,-1,-1,-1,70,71,72,-1,-1,-1,-1};
		insertWord(6,tmpArray);
		delete [] tmpArray;
	}
	else if(mode.equals("CHE")){
		tmpArray = new int[11] {-1,-1,-1,-1,48,49,50,-1,-1,-1,-1};
		insertWord(4,tmpArray);
		delete [] tmpArray;
	}
	else if(mode.equals("XMASTREE")){
		tmpArray = new int[11] {-1,-1,-1,-1,-1,104,-1,-1,-1,-1,-1};
		insertWord(9,tmpArray);
		delete [] tmpArray;

		tmpArray = new int[11] {-1,-1,-1,-1,92,93,94,-1,-1,-1,-1};
		insertWord(8,tmpArray);
		delete [] tmpArray;

		tmpArray = new int[11] {-1,-1,-1,84,83,82,81,80,-1,-1,-1};
		insertWord(7,tmpArray);
		delete [] tmpArray;

		tmpArray = new int[11] {-1,-1,-1,-1,70,71,72,-1,-1,-1,-1};
		insertWord(6,tmpArray);
		delete [] tmpArray;

		tmpArray = new int[11] {-1,-1,63,62,61,60,59,58,57,-1,-1};
		insertWord(5,tmpArray);
		delete [] tmpArray;

		tmpArray = new int[11] {-1,-1,-1,47,48,49,50,51,-1,-1,-1};
		insertWord(4,tmpArray);
		delete [] tmpArray;

		tmpArray = new int[11] {-1,42,41,40,39,38,37,36,35,34,-1};
		insertWord(3,tmpArray);
		delete [] tmpArray;

		tmpArray = new int[11] {-1,-1,24,25,26,27,28,29,30,-1,-1};
		insertWord(2,tmpArray);
		delete [] tmpArray;

		tmpArray = new int[11] {21,20,19,18,17,16,15,14,13,12,11};
		insertWord(1,tmpArray);
		delete [] tmpArray;

		tmpArray = new int[11] {-1,-1,-1,-1,4,5,6,-1,-1,-1,-1};
		insertWord(0,tmpArray);
		delete [] tmpArray;

	}
	else if(mode.equals("NULL")){
		//Do nothing, matrix was set to -1
	}

	//delete [] tmpArray;
}

void LEDMatrix::setTime(int hour, int min, bool debugmode) {
	String tmpDebug = "";
	int* tmpArray;

	//set Array to -1
	//memset(ledArray,0,sizeof(ledArray));
	//Geht
	//memset(ledMatrix, -1, sizeof(ledMatrix[0][0]) * 10 * 11);
	clearMatrix();

	//Ganze Reihe einfügen:
	//setLedRow(0,new int[11]{0,1,2,3,-1,-1,-1,-1,-1,-1,-1});

	bool nextHour = false;

	if((min>=0 && min<15) || (min>=20 && min<25)) nextHour = false;
	else nextHour = true;

	if(clockLanguage==0){
		//Display other words
		if(sec1Active){
			if(debugmode) tmpDebug += "ES IST ";
			tmpArray = new int[11] {109,108,-1,106,105,104,-1,-1,-1,-1,-1};
			insertWord(9,tmpArray);
			delete [] tmpArray;
		}

		//Vorlage:
		//insertWord(9,new int[11]{-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1});

		//Display minutes
		if(min>=0 && min<5){
			//Dunkel
			//LED-Matrix command
		}else if(min>=5 && min<10){
			if(debugmode) tmpDebug += "FÜNF1 NACH";
			tmpArray = new int[11] {-1,-1,-1,-1,-1,-1,-1,102,101,100,99};
			insertWord(9,tmpArray);
			delete [] tmpArray;
			tmpArray = new int[11] {-1,-1,-1,-1,-1,-1,-1,73,74,75,76};
			insertWord(6,tmpArray);
			delete [] tmpArray;

		}else if(min>=10 && min<15){
			if(debugmode) tmpDebug += "ZEHN1 NACH";
			tmpArray = new int[11] {88,89,90,91,-1,-1,-1,-1,-1,-1,-1};
			insertWord(8,tmpArray);
			delete [] tmpArray;
			tmpArray = new int[11] {-1,-1,-1,-1,-1,-1,-1,73,74,75,76};
			insertWord(6,tmpArray);
			delete [] tmpArray;

		}else if(min>=15 && min<20){
			if(debugmode) tmpDebug += "VIERTEL";
			tmpArray = new int[11] {-1,-1,-1,-1,83,82,81,80,79,78,77};
			insertWord(7,tmpArray);
			delete [] tmpArray;

		}else if(min>=20 && min<25){
			if(debugmode) tmpDebug += "ZWANZIG NACH";
			tmpArray = new int[11] {-1,-1,-1,-1,92,93,94,95,96,97,98};
			insertWord(8,tmpArray);
			delete [] tmpArray;
			tmpArray = new int[11] {-1,-1,-1,-1,-1,-1,-1,73,74,75,76};
			insertWord(6,tmpArray);
			delete [] tmpArray;

		}else if(min>=25 && min<30){
			if(debugmode) tmpDebug += "FÜNF1 VOR HALB";
			tmpArray = new int[11] {-1,-1,-1,-1,-1,-1,-1,102,101,100,99};
			insertWord(9,tmpArray);
			delete [] tmpArray;
			tmpArray = new int[11] {66,67,68,-1,-1,-1,-1,-1,-1,-1,-1};
			insertWord(6,tmpArray);
			delete [] tmpArray;
			tmpArray = new int[11] {65,64,63,62,-1,-1,-1,-1,-1,-1,-1};
			insertWord(5,tmpArray);
			delete [] tmpArray;

		}else if(min>=30 && min<35){
			if(debugmode) tmpDebug += "HALB";
			tmpArray = new int[11] {65,64,63,62,-1,-1,-1,-1,-1,-1,-1};
			insertWord(5,tmpArray);
			delete [] tmpArray;

		}else if(min>=35 && min<40){
			if(debugmode) tmpDebug += "FÜNF1 NACH HALB";
			tmpArray = new int[11] {-1,-1,-1,-1,-1,-1,-1,102,101,100,99};
			insertWord(9,tmpArray);
			delete [] tmpArray;
			tmpArray = new int[11] {-1,-1,-1,-1,-1,-1,-1,73,74,75,76};
			insertWord(6,tmpArray);
			delete [] tmpArray;
			tmpArray = new int[11] {65,64,63,62,-1,-1,-1,-1,-1,-1,-1};
			insertWord(5,tmpArray);
			delete [] tmpArray;

		}else if(min>=40 && min<45){
			if(debugmode) tmpDebug += "ZWANZIG VOR";
			tmpArray = new int[11] {-1,-1,-1,-1,92,93,94,95,96,97,98};
			insertWord(8,tmpArray);
			delete [] tmpArray;
			tmpArray = new int[11] {66,67,68,-1,-1,-1,-1,-1,-1,-1,-1};
			insertWord(6,tmpArray);
			delete [] tmpArray;

		}else if(min>=45 && min<50){
			if(debugmode) tmpDebug += "DREIVIERTEL";
			tmpArray = new int[11] {87,86,85,84,83,82,81,80,79,78,77};
			insertWord(7,tmpArray);
			delete [] tmpArray;

		}else if(min>=50 && min<55){
			if(debugmode) tmpDebug += "ZEHN1 VOR";
			tmpArray = new int[11] {88,89,90,91,-1,-1,-1,-1,-1,-1,-1};
			insertWord(8,tmpArray);
			delete [] tmpArray;
			tmpArray = new int[11] {66,67,68,-1,-1,-1,-1,-1,-1,-1,-1};
			insertWord(6,tmpArray);
			delete [] tmpArray;

		}else if(min>=55){
			if(debugmode) tmpDebug += "FÜNF1 VOR";
			tmpArray = new int[11] {-1,-1,-1,-1,-1,-1,-1,102,101,100,99};
			insertWord(9,tmpArray);
			delete [] tmpArray;
			tmpArray = new int[11] {66,67,68,-1,-1,-1,-1,-1,-1,-1,-1};
			insertWord(6,tmpArray);
			delete [] tmpArray;
		}


		//Display hour
		if(hour==12 || hour==0){
			if(!nextHour){
				if(debugmode) tmpDebug += " ZWÖLF ";
				tmpArray = new int[11] {-1,-1,-1,-1,-1,-1,15,14,13,12,11};
				insertWord(1,tmpArray);
				delete [] tmpArray;
			}else {
				if(debugmode) tmpDebug += " EINS ";
				tmpArray = new int[11] {44,45,46,47,-1,-1,-1,-1,-1,-1,-1};
				insertWord(4,tmpArray);
				delete [] tmpArray;
			}
		}else if(hour==1 && min>=0 && min<5){
			if(debugmode) tmpDebug += " EIN ";
			tmpArray = new int[11] {44,45,46,-1,-1,-1,-1,-1,-1,-1,-1};
			insertWord(4,tmpArray);
			delete [] tmpArray;
		}else if(hour==1){
			if(!nextHour){
				if(debugmode) tmpDebug += " EINS ";
				tmpArray = new int[11] {44,45,46,47,-1,-1,-1,-1,-1,-1,-1};
				insertWord(4,tmpArray);
				delete [] tmpArray;
			}else {
				if(debugmode) tmpDebug += " ZWEI ";
				tmpArray = new int[11] {-1,-1,-1,-1,-1,-1,-1,51,52,53,54};
				insertWord(4,tmpArray);
				delete [] tmpArray;
			}
		}else if(hour==2){
			if(!nextHour){
				if(debugmode) tmpDebug += " ZWEI ";
				tmpArray = new int[11] {-1,-1,-1,-1,-1,-1,-1,51,52,53,54};
				insertWord(4,tmpArray);
				delete [] tmpArray;
			}else {
				if(debugmode) tmpDebug += " DREI ";
				tmpArray = new int[11] {43,42,41,40,-1,-1,-1,-1,-1,-1,-1};
				insertWord(3,tmpArray);
				delete [] tmpArray;
			}
		}else if(hour==3){
			if(!nextHour){
				if(debugmode) tmpDebug += " DREI ";
				tmpArray = new int[11] {43,42,41,40,-1,-1,-1,-1,-1,-1,-1};
				insertWord(3,tmpArray);
				delete [] tmpArray;
			}else {
				if(debugmode) tmpDebug += " VIER ";
				tmpArray = new int[11] {-1,-1,-1,-1,-1,-1,-1,36,35,34,33};
				insertWord(3,tmpArray);
				delete [] tmpArray;
			}
		}else if(hour==4){
			if(!nextHour){
				if(debugmode) tmpDebug += " VIER ";
				tmpArray = new int[11] {-1,-1,-1,-1,-1,-1,-1,36,35,34,33};
				insertWord(3,tmpArray);
				delete [] tmpArray;
			}else {
				if(debugmode) tmpDebug += " FÜNF2 ";
				tmpArray = new int[11] {-1,-1,-1,-1,-1,-1,-1,58,57,56,55};
				insertWord(5,tmpArray);
				delete [] tmpArray;
			}
		}else if(hour==5){
			if(!nextHour){
				if(debugmode) tmpDebug += " FÜNF2 ";
				tmpArray = new int[11] {-1,-1,-1,-1,-1,-1,-1,58,57,56,55};
				insertWord(5,tmpArray);
				delete [] tmpArray;
			}else {
				if(debugmode) tmpDebug += " SECHS ";
				tmpArray = new int[11] {22,23,24,25,26,-1,-1,-1,-1,-1,-1};
				insertWord(2,tmpArray);
				delete [] tmpArray;
			}
		}else if(hour==6){
			if(!nextHour){
				if(debugmode) tmpDebug += " SECHS ";
				tmpArray = new int[11] {22,23,24,25,26,-1,-1,-1,-1,-1,-1};
				insertWord(2,tmpArray);
				delete [] tmpArray;
			}else {
				if(debugmode) tmpDebug += " SIEBEN ";
				tmpArray = new int[11] {21,20,19,18,17,16,-1,-1,-1,-1,-1};
				insertWord(1,tmpArray);
				delete [] tmpArray;
			}
		}else if(hour==7){
			if(!nextHour){
				if(debugmode) tmpDebug += " SIEBEN ";
				tmpArray = new int[11] {21,20,19,18,17,16,-1,-1,-1,-1,-1};
				insertWord(1,tmpArray);
				delete [] tmpArray;
			}else {
				if(debugmode) tmpDebug += " ACHT ";
				tmpArray = new int[11] {-1,-1,-1,-1,-1,-1,-1,29,30,31,32};
				insertWord(2,tmpArray);
				delete [] tmpArray;
			}
		}else if(hour==8){
			if(!nextHour){
				if(debugmode) tmpDebug += " ACHT ";
				tmpArray = new int[11] {-1,-1,-1,-1,-1,-1,-1,29,30,31,32};
				insertWord(2,tmpArray);
				delete [] tmpArray;
			}else {
				if(debugmode) tmpDebug += " NEUN ";
				tmpArray = new int[11] {-1,-1,-1,3,4,5,6,-1,-1,-1,-1};
				insertWord(0,tmpArray);
				delete [] tmpArray;
			}
		}else if(hour==9){
			if(!nextHour){
				if(debugmode) tmpDebug += " NEUN ";
				tmpArray = new int[11] {-1,-1,-1,3,4,5,6,-1,-1,-1,-1};
				insertWord(0,tmpArray);
				delete [] tmpArray;
			}else {
				if(debugmode) tmpDebug += " ZEHN2 ";
				tmpArray = new int[11] {0,1,2,3,-1,-1,-1,-1,-1,-1,-1};
				insertWord(0,tmpArray);
				delete [] tmpArray;
			}
		}else if(hour==10){
			if(!nextHour){
				if(debugmode) tmpDebug += " ZEHN2 ";
				tmpArray = new int[11] {0,1,2,3,-1,-1,-1,-1,-1,-1,-1};
				insertWord(0,tmpArray);
				delete [] tmpArray;
			}else {
				if(debugmode) tmpDebug += " ELF ";
				tmpArray = new int[11] {-1,-1,-1,-1,-1,60,59,58,-1,-1,-1};
				insertWord(5,tmpArray);
				delete [] tmpArray;
			}
		}else if(hour==11){
			if(!nextHour){
				if(debugmode) tmpDebug += " ELF ";
				tmpArray = new int[11] {-1,-1,-1,-1,-1,60,59,58,-1,-1,-1};
				insertWord(5,tmpArray);
				delete [] tmpArray;
			}else {
				if(debugmode) tmpDebug += " ZWÖLF ";
				tmpArray = new int[11] {-1,-1,-1,-1,-1,-1,15,14,13,12,11};
				insertWord(1,tmpArray);
				delete [] tmpArray;
			}
		}

		if(sec3Active){
			if(min>=0 && min<5){
				if(debugmode) tmpDebug += " UHR.";
				tmpArray = new int[11] {-1,-1,-1,-1,-1,-1,-1,-1,8,9,10};
				insertWord(0,tmpArray);
				delete [] tmpArray;
			}
		}
	}else if(clockLanguage==1){
		//set matrix for Schwaebisch


	}

	debugLog = tmpDebug;
}

void LEDMatrix::calcColorTemp(int tempKelvin) {
	tempKelvin = tempKelvin/100;

	//Calculate the red value
	if(tempKelvin <= 66) colorTempRgb[0]=255;
	else {
		colorTempRgb[0] = int(329.698727446 * (pow((tempKelvin-60.0),-0.1332047592)));
		if(colorTempRgb[0]<0) colorTempRgb[0] = 0;
		if(colorTempRgb[0]>255) colorTempRgb[0] = 255;
	}

	//Calculate the green value
	if(tempKelvin <= 66){
		colorTempRgb[1] = int(99.4708025861 * log(tempKelvin) - 161.1195681661);
		if(colorTempRgb[1]<0) colorTempRgb[1] = 0;
		if(colorTempRgb[1]>255) colorTempRgb[1] = 255;
	}
	else {
		colorTempRgb[1] = int(288.1221695283 * (pow(tempKelvin-60,-0.0755148492)));
		if(colorTempRgb[1]<0) colorTempRgb[1] = 0;
		if(colorTempRgb[1]>255) colorTempRgb[1] = 255;
	}

	//Calculate blue value
	if(tempKelvin >= 66){
		colorTempRgb[2] = 255;
	}
	else {
		if(tempKelvin<=19) colorTempRgb[2] = 0;
		else {
			colorTempRgb[2] = int(138.5177312231 * log(tempKelvin-10) - 305.0447927307);
			if(colorTempRgb[2]<0) colorTempRgb[2] = 0;
			if(colorTempRgb[2]>255) colorTempRgb[2] = 255;
		}

	}
}

int LEDMatrix::getTempRed() {
	return colorTempRgb[0];
}

int LEDMatrix::getTempGreen() {
	return colorTempRgb[1];
}

int LEDMatrix::getTempBlue() {
	return colorTempRgb[2];
}


