#include "CSaveSystem.h"
#include "CLog.h"

CSaveSystem &CSaveSystem::saveSystem()
{
	static CSaveSystem savesystemmgr;
	return savesystemmgr;
}

bool CSaveSystem::CSave::loads()
{
	try{
		std::fstream svfstream(fpath, std::ios::in | std::ios::binary);

		if (!svfstream.is_open())
		{
			return false;
		}

		cereal::BinaryInputArchive iarchive(svfstream);

		iarchive(*this);

		loaded = true;
	}
	catch (const std::exception &e)
	{
		CLog::log() << e.what();
		loaded = false;
		return false;
	}
	catch (...)
	{
		CLog::log() << "Fail to load save " + fpath;
		return false;
	}

	return true;
}

bool CSaveSystem::CSave::saves()
{
	try{
		std::fstream svfstream(fpath, std::ios::out | std::ios::trunc | std::ios::binary);

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
	}
	catch (const std::exception &e)
	{
		CLog::log() << e.what();
		return false;
	}
	catch (...)
	{
		return false;
	}
	return true;
}

bool CSaveSystem::CSave::loadn(const std::string &savepath)
{
	fpath = savepath;
	return loads();
}

CSaveSystem::CSave::CSave(const std::string &savepath)
{
	fpath = savepath;

}

CSaveSystem::CSaveSystem()
{



}
