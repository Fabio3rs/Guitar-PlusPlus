/*****************************************************************************************************
*		GUITAR++
*		PROGRAMADO POR FÁBIO
*		BMS - Brazilian Modding Studio - http://brmodstudio.forumeiros.com
*****************************************************************************************************/
#pragma once
#ifndef LOGGING_SYSTEM_CLOG_H
#define LOGGING_SYSTEM_CLOG_H
#include <string>
#include <fstream>
#include <exception>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <ctime>

class CLogException : std::exception{
	std::string Str;
	
public:
	const char *what(){
		return Str.c_str();
	}
	
	CLogException(const char *exceptionString) : std::exception(){
		Str = exceptionString;
	}
	
	~CLogException() throw(){
		Str.clear();
	}
};

class CLog{
	std::string		FileName;
	std::string		LogContents;
	std::fstream	LogFile;

public:
	//static char *TimeStringBuffer;
	static char *multiRegisterBuffer;

	bool Finished;
	
	~CLog();
	void AddToLog(const std::string &Text);
	void multiRegister(const char *format, ...);
	void FinishLog();
	void SaveBuffer();
	inline void operator << (const std::string &Text){AddToLog(Text);}
	std::string GetDateAndTime();

	static CLog &log();

	CLog(const CLog&) = delete;

private:
	CLog(const std::string &NameOfFile);
};
#endif