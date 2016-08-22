#include "CMultiplayer.h"

void CMultiplayer::mpThread()
{




}


CMultiplayer::CMultiplayer()
{



}


CMultiplayer::~CMultiplayer()
{

	if (instThread.joinable())
		instThread.join();

}


