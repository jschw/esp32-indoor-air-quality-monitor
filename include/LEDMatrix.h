/*
 * LEDMatrix.h
 *
 *  Created on: 20.02.2018
 *      Author: Julian
 */
#include "Arduino.h"

#ifndef LEDMATRIX_H_
#define LEDMATRIX_H_

class LEDMatrix {
public:
	LEDMatrix();
	virtual ~LEDMatrix();
	int ledMatrix[10][11];
	int clockLanguage;
	bool sec1Active;
	bool sec3Active;
	void getMatrix();
	String getDebug();
	void insertWord(int row, int array[]);
	void clearMatrix();
	void setTime(int hour, int min, bool debug);
	void setStandby(String mode);
	int colorTempRgb[3];
	void calcColorTemp(int tempKelvin);
	int getTempRed();
	int getTempBlue();
	int getTempGreen();

private:
	String debugLog;

};

#endif /* LEDMATRIX_H_ */
