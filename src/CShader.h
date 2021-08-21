#pragma once
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
#include <functional>
#include <memory>
#include <map>

namespace ShaderProject
{
	enum shaderTypes { FRAGMENT, VERTEX };

	struct GLvb
	{
		unsigned int VBO, VAO;
	};

	template<class T, const T nullValue, class Deleter>
	class unique_object
	{
		T object;
		std::function<Deleter> deleter;

		unique_object &operator=(const unique_object &obj) = delete;
		unique_object(const unique_object &obj) = delete;
	public:
		const T &get() const { return object; }
		const T &operator()() const { return object; }

		void deleteObject()
		{
			deleter(this->object);
			this->object = nullValue;
		}

		unique_object &operator=(unique_object &&obj)
		{
			if (this == std::addressof(obj))
				return *this;

			deleteObject();
			object = obj.object;
			obj.object = nullValue;

			deleter = std::move(obj.deleter);

			return *this;
		}

		unique_object() : object(nullValue) { }

		unique_object(T &&value, Deleter delFun) : unique_object()
		{
			object = std::move(value);
			deleter = delFun;
		}

		unique_object(unique_object &&obj) : unique_object() {
			object = obj.object;
			obj.object = nullValue;
			deleter = std::move(obj.deleter);
		}

		~unique_object()
		{
			deleteObject();
		}
	};

	typedef unique_object<unsigned int, 0u, void(unsigned int)> ProgramObject;
	typedef unique_object<unsigned int, 0u, void(unsigned int)> ShaderObject;
	typedef unique_object<unsigned int, 0u, void(unsigned int)> VertexObject;
	ProgramObject make_program();
	ShaderObject make_shader(shaderTypes type);
	VertexObject make_vertexObject(VertexObject type);

	class CShader {
		bool enableShaders;
		bool usingProgram;

		struct shaderInst {
			shaderTypes type;
			ShaderObject glID;
			std::string fileName;
			std::unique_ptr<char[]> shaderContent;
			int shaderSize;

			template<class T>
			inline auto fileSize(T &file) {
				file.seekg(0, std::ios::end);
				auto result = file.tellg();
				file.seekg(0, std::ios::beg);
				return result;
			}

			void compile();
			shaderInst(const char *fName, shaderTypes shaderType);
		};

		struct shaderEvent {
			ProgramObject shaderProg;
			std::string eventName;
			std::deque<shaderInst*> shaders;

			shaderEvent(const shaderEvent&) = delete;
			shaderEvent(shaderEvent&&) = default;

			shaderEvent &operator=(shaderEvent &&obj)
			{
				if (this == std::addressof(obj))
					return *this;

				shaderProg.deleteObject();

				shaderProg = std::move(obj.shaderProg);
				eventName = std::move(obj.eventName);
				shaders = std::move(obj.shaders);

				obj.shaders.clear();

				return *this;
			}

			shaderEvent(const char *name);
		};

		std::deque<shaderEvent> events;
		std::deque<shaderInst> shadersList;
	public:
		static CShader &inst();

		bool isShadersEnabled() const {
			return enableShaders;
		}

		bool isUsingProgram() const {
			return usingProgram;
		}

		size_t eventsSize() const {
			return events.size();
		}

		size_t shadersListSize() const {
			return shadersList.size();
		}

		void					processEvent(int id);
		int						addEvent(const char *name);
		void					addShaderToEvent(const char *shaderEvent, int shaderID);
		int						newShader(const char *shaderFile, shaderTypes type, int eventToLink);
		int						newShader(const char *shaderFile, shaderTypes type, const char *name);
		void					linkAllShaders();
		void					deactivateShader();

		static void bindProgram(const ProgramObject &programID);
		static void bindProgram(unsigned int programID);
		static ProgramObject createShaderProgram();
		static ShaderObject compileShader(const char *shaderSource, int length, shaderTypes type);
		static void attachShaderToProgram(const ProgramObject &programID, const ShaderObject &shaderID);
		static void linkShaderProgram(const ProgramObject &programID);

		static int getProgramUniform(const ProgramObject &program, const char *string);
		static void setFloatUniform(int uniform, float val);

		static void vertexAttribPointerFloat(unsigned int index, int elements, bool normalized, int stride, const void *pointer);
		static void vertexAttribPointerDouble(unsigned int index, int elements, bool normalized, int stride, const void *pointer);
		static void bindVertexArray(const VertexObject &vertexArray);
		static void bindVertexArray(unsigned int vertexArray);

	private:
		CShader();
		CShader(const CShader&) = delete;
	};
}

#endif

