#define __CSHADER_CPP

#include <iostream>
#include <GL/glew.h>
#include "CShader.h"
#include <Windows.h>
#include "CLog.h"
#include "CLuaFunctions.hpp"

CShader &CShader::inst()
{
	static CShader shaders;
	return shaders;
}

CShader::shaderEvent::shaderEvent(const char *name){
	if (!inst().enableShaders)
		return;

	shaderProg = glCreateProgram();
	eventName = name;
}

CShader::shaderInst::shaderInst(const char *fName, int shaderType){
	if (!inst().enableShaders)
		return;

	std::fstream shaderFile(fName);
	fileName = fName;
	
	if(!shaderFile.good()) std::logic_error("Can't load shader's file");
	
	shaderSize = fileSize(shaderFile);
	
	shaderContent = new char[shaderSize + 32];
	memset(shaderContent, 0, shaderSize + 32);
	shaderFile.read(shaderContent, shaderSize);
	//*(short*)&shaderContent[shaderSize] = 0;
	
	type = shaderType? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER;
	
	compile();
}

void CShader::shaderInst::compile()
{
	if (!inst().enableShaders)
		return;
	int length = shaderSize;

	const GLchar **ch = (const char**)&shaderContent;

	glID = glCreateShader(type);
	glShaderSource(glID, 1, ch, 0);
	glCompileShader(glID);
	GLint compiled;
	glGetShaderiv(glID, GL_COMPILE_STATUS, &compiled);
	if (!compiled) {
		glGetShaderiv(glID, GL_INFO_LOG_LENGTH, &length);
		char shaderErrorDetail[1024] = {0, 0};
		glGetShaderInfoLog(glID, length, &length, shaderErrorDetail);

		CLog::log() << shaderErrorDetail;
		std::cout << shaderErrorDetail << std::endl;
	}
}

int CShader::addEvent(const char *name){
	if (!inst().enableShaders)
		return -1;

	shaderEvent newEvent(name);
	events.push_back(newEvent);
	return events.size() - 1;
}

void CShader::addShaderToEvent(const char *shaderEvent, int shaderID){
	if (!inst().enableShaders)
		return;

	int ID = -1;
	for(int i = 0, size = events.size(); i < size; i++){
		if (events[i].eventName == shaderEvent){
			ID = i;
			break;
		}
	}
	
	if(ID != -1){
		glAttachShader(events[ID].shaderProg, shadersList[shaderID].glID);
	}
	usingProgram = false;
	glUseProgram(0);
}

void CShader::processEvent(int id){
	if (!enableShaders)
		return;

	int shaderProg = events[id].shaderProg;

	glUseProgram(shaderProg);

	if (shaderProg != 0)
		usingProgram = true;
}

int CShader::newShader(const char *shaderFile, int type, int eventToLink){
	if (!inst().enableShaders)
		return 0;

	shadersList.push_back(shaderInst(shaderFile, type));
	glAttachShader(events[eventToLink].shaderProg, shadersList.back().glID);
	usingProgram = false;
	glUseProgram(0);
	return shadersList.size() - 1;
}

void CShader::desactivateShader()
{
	if (!inst().enableShaders)
		return;

	usingProgram = false;
	glUseProgram(0);
}

int CShader::newShader(const char *shaderFile, int type, const char *name){
	if (!inst().enableShaders)
		return 0;

	int ID = -1;
	for(int i = 0, size = events.size(); i < size; i++){
		if(events[i].eventName == name){
			ID = i;
			break;
		}
	}
	
	if(ID != -1){
		return newShader(shaderFile, type, ID);
	}
	
	glUseProgram(0);
	return -1;
}

void CShader::linkAllShaders(){
	if (!inst().enableShaders)
		return;

	for(int i = 0, size = events.size(); i < size; i++)
	{
		glLinkProgram(events[i].shaderProg);
	}
}

CShader::CShader()
{
	enableShaders = false;
	usingProgram = false;
	CLuaFunctions::GameVariables::gv().pushVar("enableShaders", enableShaders);
	
	GLenum err = glewInit();
	if (GLEW_OK != err){
	   /*Problem: glewInit failed, something is seriously wrong. */
		MessageBoxA(0, (char*)glewGetErrorString(err), "Error: %s\n", 0);
	}
	//glUseProgram(0);
	/*
	GLint maxPatchVertices = 0;
	glGetIntegerv(GL_MAX_PATCH_VERTICES, &maxPatchVertices);

	glPatchParameteri(GL_PATCHES, maxPatchVertices);
	glPatchParameteri(GL_PATCH_VERTICES, maxPatchVertices);*/
}