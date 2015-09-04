/*****************************************************************************************************
*		GUITAR++
*		PROGRAMADO POR FÁBIO
*		BMS - Brazilian Modding Studio - http://brmodstudio.forumeiros.com
*****************************************************************************************************/
#include "CLog.h"
#include <stdio.h>
#include <time.h>
#include <Windows.h>
#include "CLuaH.hpp"

int InstallExceptionCatcher(void(*cb)(const char* buffer));

//char *CLog::TimeStringBuffer = new char[0x400];
char *CLog::multiRegisterBuffer = new char[0x2000];

CLog &CLog::log(void){
	static CLog Log("Guitar++.log");
	return Log;
}

CLog::CLog(const std::string &NameOfFile){
	Finished = false;
	FileName = NameOfFile;
	LogFile.rdbuf()->pubsetbuf(0, 0);
	LogFile.open(NameOfFile, std::ios::in | std::ios::out | std::ios::trunc);
	
	if(!LogFile.good()) throw CLogException("Impossivel criar ou abrir o arquivo de log");
	
	LogContents = "*********************************************** \nLog started at: ";
	LogContents += GetDateAndTime();
	LogContents += "\n*********************************************** \n\n";

	InstallExceptionCatcher([](const char *buffer){
		CLuaH::Lua().runEvent("emergencyLogSave");
		log() << buffer;
		log().SaveBuffer();
		CLuaH::Lua().runEvent("emergencyExit");
	});
}

CLog::~CLog(){
	FinishLog();
}

std::string CLog::GetDateAndTime(){
	std::time_t tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	return std::string(ctime(&tt));
}

void CLog::AddToLog(const std::string &Text){
	std::string Temp;
	Temp += GetDateAndTime();
	Temp += ": ";
	Temp += Text;
	Temp += "\n";
	LogContents += Temp;
	//LogFile << Temp;
}

void CLog::multiRegister(const char *format, ...){
	va_list va;
	va_start(va, format);
	*multiRegisterBuffer = 0;
	vsprintf(multiRegisterBuffer, format, va);
	AddToLog(multiRegisterBuffer);
}

void CLog::FinishLog(){
	if(Finished) return;
	LogContents += "\n*********************************************** \nLog Finished at: ";
	LogContents += GetDateAndTime();
	LogContents += "\n*********************************************** \n";
	LogFile.clear();
	LogFile.write(LogContents.c_str(), LogContents.size());
	Finished = true;
}

void CLog::SaveBuffer(){
	if(Finished) return;
	std::string LogContentBackup = LogContents;
	LogContents += "\n*********************************************** \nLog Finished at: ";
	LogContents += GetDateAndTime();
	LogContents += "\n*********************************************** \n";
	//LogFile.clear();
	LogFile.write(LogContents.c_str(), LogContents.size());
	LogContents = LogContentBackup;
	LogFile.flush();
}