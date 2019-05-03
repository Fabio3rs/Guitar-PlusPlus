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
#include <sstream>
#include <cereal/cereal.hpp>
#include <cereal/archives/portable_binary.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/deque.hpp>

class CSaveSystem
{

public:
	class CSave
	{
		friend class cereal::access;

		std::string fpath;

		bool loaded;

		struct variable
		{
			std::string name;
			size_t size;
			bool dynamic;
			int type;
			void *ptr;
			std::vector < uint8_t > svcontent;

			template<class Archive>
			void load(Archive & archive)
			{
				archive(name, size, dynamic, type, svcontent);
			}

			template<class Archive>
			void save(Archive & archive) const
			{
				archive(name, size, dynamic, type, svcontent);
			}

			inline variable()
			{
				type = 0;
				size = 0;
				dynamic = false;
				ptr = nullptr;
			}
		};

		std::map < std::string, variable > values;

		template<class Archive>
		void load(Archive & archive)
		{
			archive(values);
		}

		template<class Archive>
		void save(Archive & archive) const
		{
			archive(values);
		}

	public:

		bool loads();
		bool loadn(const std::string &savepath);
		bool saves();

		template <class T>
		inline T getVarContent(const std::string &var)
		{
			T result;

			auto &v = values[var];

			if (sizeof(T) != v.size)
			{
				std::cout << "Error\n";
				return result;
			}

			for (int i = 0; i < v.size; i++)
			{
				((uint8_t*)(&result))[i] = v.svcontent[i];
			}

			return result;
		}

		template<class T>
		inline bool addVariableAttData(const std::string &name, T &var, bool dynamic)
		{
			if (!loaded)
				return false;

			variable v = values[name];
			v.name = name;
			v.size = sizeof(var);
			v.dynamic = dynamic;
			v.ptr = (void*)&var;

			if (v.svcontent.size() != 0)
			{
				if (v.size != v.svcontent.size())
				{
					return false;
				}

				/*for (size_t i = 0; i < v.size; i++)
				{
					((uint8_t*)(&var))[i] = v.svcontent[i];
				}*/

				memcpy(&var, &(v.svcontent[0]), v.size);
			}
			else{
				for (int i = 0; i < sizeof(T); ++i)
				{
					v.svcontent.push_back(0);
				}

				memcpy(&(v.svcontent[0]), &var, sizeof(T));
			}

			v.type = 0;

			values[name] = v;
			return true;
		}

		inline bool addVariableAttData(const std::string &name, std::string &var, bool dynamic)
		{
			if (!loaded)
				return false;

			variable v = values[name];
			v.name = name;
			v.size = var.size();
			v.dynamic = dynamic;
			v.ptr = (void*)&var;

			if (v.svcontent.size() != 0)
			{
				var = "";
				for (size_t i = 0u; i < v.svcontent.size(); i++)
				{
					var.push_back((char)v.svcontent[i]);
				}
				v.size = v.svcontent.size();
			}
			else{
				for (size_t i = 0u; i < v.size; ++i)
				{
					v.svcontent.push_back((int8_t)var[i]);
				}
			}

			v.type = 1;

			values[name] = v;
			return true;
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

			v.type = 0;

			values[name] = v;
			return;
		}

		CSave(const std::string &savepath);
		CSave() = default;
	};


	static CSaveSystem &saveSystem();

private:
	CSaveSystem();
};


#endif
