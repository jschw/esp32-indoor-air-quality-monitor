/*
 * EventLog.cpp
 *
 *  Created on: 15.12.2019
 *      Author: Julian
 */
#include "EventLog.h"
#include "Arduino.h"

String logNotes;
String logWarnings;
String logErrors;
int numNotes;
int numWarnings;
int numErrors;

String lineBreak;
String timestamp;

EventLog::EventLog() {
	//init vars
	logNotes="";
	logWarnings="";
	logErrors="";
	numNotes=0;
	numWarnings=0;
	numErrors=0;

	//set defaults
	lineBreak = "\r\n";
	timestamp = "-";
}

EventLog::~EventLog() {
	// TODO Auto-generated destructor stub
}

void EventLog::println(String msg, int loglevel){
	switch(loglevel){
	case 0:
		//loglevel = 0 = Note
		logNotes += timestamp + msg + lineBreak;
		numNotes++;
		break;

	case 1:
		//loglevel = 1 = Warning
		logWarnings += timestamp + msg + lineBreak;
		numWarnings++;
		break;

	case 2:
		//loglevel = 2 = Error
		logErrors += timestamp + msg + lineBreak;
		numErrors++;
		break;
	}
}

void EventLog::setLineBreak(String lb){
	lineBreak = lb;
}

void EventLog::setTimestamp(String time){
	timestamp = time;
}

String EventLog::getNotes(){
	if(logNotes.equals("")) return "-";
	else return logNotes;
}

String EventLog::getWarnings(){
	if(logWarnings.equals("")) return "-";
	else return logWarnings;
}

String EventLog::getErrors(){
	if(logErrors.equals("")) return "-";
	else return logErrors;
}

int EventLog::getNumNotes(){
	return numNotes;
}

int EventLog::getNumWarnings(){
	return numWarnings;
}

int EventLog::getNumErrors(){
	return numErrors;
}


