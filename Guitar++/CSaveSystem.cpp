#include "CSaveSystem.h"

CSaveSystem &CSaveSystem::saveSystem()
{
	static CSaveSystem savesystemmgr;
	return savesystemmgr;
}

bool CSaveSystem::CSave::loads()
{
	std::fstream svfstream(fpath, std::ios::in);

	if (!svfstream.is_open())
	{
		return false;
	}

	cereal::BinaryInputArchive iarchive(svfstream); // Create an output archive

	iarchive(*this);

	loaded = true;

	return true;
}

bool CSaveSystem::CSave::saves()
{
	std::fstream svfstream(fpath, std::ios::out | std::ios::trunc);

	if (!svfstream.is_open())
	{
		return false;
	}

	for (auto &vardata : values)
	{
		auto &v = vardata.second;

		if (v.dynamic && v.ptr)
		{
			for (size_t i = 0; i < v.size; i++)
			{
				v.svcontent[i] = ((uint8_t*)(v.ptr))[i];
			}
		}
	}

	cereal::BinaryOutputArchive oarchive(svfstream); // Create an output archive

	oarchive(*this);

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
