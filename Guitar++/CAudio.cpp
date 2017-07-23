#include "CAudio.h"
#include <bass.h>

const int64_t CAudio::prescan = BASS_STREAM_PRESCAN, CAudio::async = BASS_ASYNCFILE, CAudio::decode = BASS_STREAM_DECODE;

class audiostream::audioInternalHandle
{
	HSTREAM bassHandle;
	auto &internalGetHandle()
	{
		return bassHandle;
	}

public:
	const auto &getHandle() const
	{
		return bassHandle;
	}

	audioInternalHandle()
	{
		bassHandle = NULL;
	}

	~audioInternalHandle()
	{
		BASS_StreamFree(getHandle());
		internalGetHandle() = NULL;
	}
};

audiostream CAudio::load(const std::string &path)
{
	audiostream astream;


	return astream;
}

bool CAudio::init()
{
	auto result = BASS_Init(-1, 44100, 0, 0, NULL);
	return result;
}

CAudio &CAudio::audio()
{
	static CAudio inst;
	return inst;
}

CAudio::CAudio()
{

}

CAudio::~CAudio()
{
	BASS_Stop();
	BASS_Free();
}

audiostream::audiostream() : handle(std::make_unique<audioInternalHandle>())
{


}
