#include "CSaveSystem.h"

CSaveSystem &CSaveSystem::saveSystem()
{
	static CSaveSystem savesystemmgr;
	return savesystemmgr;
}

bool CSaveSystem::CSave::load()
{
	std::fstream svfstream(fpath, std::ios::in);

	if (!svfstream.is_open())
	{
		return false;
	}

	return true;
}

CSaveSystem::CSave::CSave(const std::string &savepath)
{
	fpath = savepath;
	loaded = true;

}

CSaveSystem::CSaveSystem()
{



}
