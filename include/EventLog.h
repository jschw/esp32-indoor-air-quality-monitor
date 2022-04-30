/*
 * EventLog.h
 *
 *  Created on: 15.12.2019
 *      Author: Julian
 */
#include "Arduino.h"


#ifndef EVENTLOG_H_
#define EVENTLOG_H_

class EventLog{
public:
	EventLog();
	virtual ~EventLog();

	void println(String msg, int loglevel = 0);

	String getNotes();
	String getWarnings();
	String getErrors();
	int getNumNotes();
	int getNumWarnings();
	int getNumErrors();

	void setTimestamp(String time);
	void setLineBreak(String lb);


private:

};



#endif /* EVENTLOG_H_ */
