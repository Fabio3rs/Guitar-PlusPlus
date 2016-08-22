#pragma once
#ifndef __GUITARPP_CMULTIPLAYER_H__
#define __GUITARPP_CMULTIPLAYER_H__

#include <thread>

class CMultiplayer
{
	static void mpThread();
	std::thread instThread;

public:

	CMultiplayer();
	~CMultiplayer();
};


#endif
