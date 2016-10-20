#ifndef GPP_GLSL_SHADERS_CLASS_CSHADER
#define GPP_GLSL_SHADERS_CLASS_CSHADER
#define GL_GLEXT_PROTOTYPES
#include <deque>
#include <cstdio>
#include <fstream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <stdexcept>

class CShader{
	bool enableShaders;
	
	struct shaderInst{
		int type;
		unsigned int glID;
		std::string fileName;
		char *shaderContent;
		int shaderSize;
		
		template<class T>
		inline size_t fileSize(T &file){
			file.seekg(0, std::ios::end);
			size_t result = file.tellg();
			file.seekg(0, std::ios::beg);
			return result;
		}
		
		void compile();
		shaderInst(const char *fName, int shaderType);
	};
	
	struct shaderEvent{
		int shaderProg;
		std::string eventName;
		std::deque<shaderInst*> shaders;
		
		shaderEvent(const char *name);
	};
	
	std::deque<shaderEvent> events;
	std::deque<shaderInst> shadersList;
public:
	static CShader &inst();
	
	void processEvent(int id);
	int addEvent(const char *name);
	void addShaderToEvent(const char *shaderEvent, int shaderID);
	int newShader(const char *shaderFile, int type, int eventToLink);
	int newShader(const char *shaderFile, int type, const char *name);
	void linkAllShaders();
	void desactivateShader();

private:
	CShader();
};
#endif