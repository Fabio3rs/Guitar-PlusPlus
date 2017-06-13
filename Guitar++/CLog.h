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
#include <deque>
#include <memory>
#include <utility>

class CLogException : public std::exception{
	std::string str;
	
public:
	const char *what() const noexcept {
		return str.c_str();
	}
	
	CLogException(const char *except) noexcept : std::exception(), str(except) { }
	
	~CLogException() noexcept { }
};

class CLog{
	std::string		FileName;
	std::fstream	LogFile;

	class argToString
	{
		std::string str;

	public:
		const std::string &getStr() const { return str; }

		argToString(const char *str) : str(str) { }

		argToString(const std::string &str) : str(str) { }

		template<class T>
		argToString(const T &value) : str(std::to_string(value)) { }
	};

	bool Finished;

public:
	~CLog();
	void AddToLog(const std::string &Text);

	template<class... Types>
	void multiRegister(const std::string &format, Types&&... args)
	{
		const std::deque < argToString > a = { std::forward<Types>(args)... };
		std::string printbuf, numbuf;

		bool ignoreNext = false;

		for (int i = 0, size = format.size(); i < size; i++)
		{
			auto ch = format[i];
			int ti = i + 1;

			switch (ch)
			{
			case '\\':
				if (ignoreNext)
				{
					printbuf.insert(printbuf.end(), 1, ch);
					ignoreNext = false;
					break;
				}

				ignoreNext = true;
				break;

			case '%':
				if (ignoreNext)
				{
					printbuf.insert(printbuf.end(), 1, ch);
					ignoreNext = false;
					break;
				}

				numbuf = "";

				{
					bool stringEnd = true;
					while (ti < size)
					{
						if (format[ti] < '0' || format[ti] > '9')
						{
							i = ti - 1;
							if (numbuf.size() > 0)
							{
								int argId = std::stoi(numbuf);

								if (argId < a.size())
								{
									printbuf += a[argId].getStr();
								}
								else
								{
									printbuf += "%";
									printbuf += numbuf;
								}

								stringEnd = false;

								break;
							}
						}
						else
						{
							numbuf.insert(numbuf.end(), 1, format[ti]);
						}

						ti++;
					}

					if (stringEnd)
					{
						i = size;
						if (numbuf.size() > 0)
						{
							int argId = std::stoi(numbuf);

							if (argId < a.size())
							{
								printbuf += a[argId].getStr();
							}
							else
							{
								printbuf += "%";
								printbuf += numbuf;
							}
						}
					}
				}
				break;

			default:
				printbuf.insert(printbuf.end(), 1, ch);
				break;
			}
		}

		AddToLog(printbuf);
	}

	void FinishLog();
	void SaveBuffer();
	void operator << (const std::string &Text) { AddToLog(Text); }
	std::string GetDateAndTime();

	static CLog &log();

	CLog(const CLog&) = delete;

private:
	CLog(const std::string &NameOfFile);
};
#endif