#pragma once
#ifndef GuitarPP_CAudio_h
#define GuitarPP_CAudio_h
#include <memory>
#include <string>
#include <cstdint>

class audiostream
{
	class audioInternalHandle;
	std::unique_ptr<audioInternalHandle> handle;

public:
	audiostream();
};

class CAudio
{
	CAudio();
	~CAudio();
	CAudio(CAudio&) = delete;

public:
	audiostream load(const std::string &path);

	bool init();
	static CAudio &audio();

	static const int64_t prescan, async, decode;
};


#endif