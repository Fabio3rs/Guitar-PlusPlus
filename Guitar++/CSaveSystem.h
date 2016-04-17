#pragma once
#ifndef _CSAVESYSTEM_H_
#define _CSAVESYSTEM_H_

#include <functional>
#include <utility>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>
#include <deque>
#include <map>

class CSaveSystem
{

public:
	class CSave
	{
		std::string fpath;

		bool loaded;

		struct variable
		{
			std::string name;
			size_t size;
			bool dynamic;
			void *ptr;
			std::vector < uint8_t > svcontent;
		};

		std::map < std::string, variable > values;

	public:
		bool load();
		bool save();

		template<class T>
		inline bool addVariableAttData(const std::string &name, T &var, bool dynamic)
		{
			if (!loaded)
				return false;

			variable v;
			v.name = name;
			v.size = sizeof(var);
			v.dynamic = dynamic;
			v.ptr = (void*)&var;

			if (v.size != v.svcontent.size)
			{
				return false;
			}

			for (size_t i = 0; i < v.size; i++)
			{
				((uint8_t*)(&var))[i] = v.svcontent[i];
			}
		}

		template<class T>
		inline void addVariable(const std::string &name, const T &var, bool dynamic)
		{
			variable v;
			v.name = name;
			v.size = sizeof(var);
			v.dynamic = dynamic;
			v.ptr = (void*)&var;

			for (size_t i = 0; i < v.size; i++)
			{
				v.svcontent.push_back(((uint8_t*)(&var))[i]);
			}
		}

		CSave(const std::string &savepath);
	};


	static CSaveSystem &saveSystem();

private:
	CSaveSystem();
};


#endif