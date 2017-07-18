#pragma once
#ifndef _GUITARPP_CGPPFILEMGR_H_
#define _GUITARPP_CGPPFILEMGR_H_

#include <fstream>
#include <string>
#include <cereal/cereal.hpp>
#include <cereal/archives/portable_binary.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/deque.hpp>
#include <limits>

class GPPPackage
{
	friend class CGPPFileMGR;
	bool loaded;

public:
	struct item
	{
		bool encrypted;
		std::string name;
		std::vector<unsigned char> bytes;

		template<class Archive>
		void load(Archive &archive)
		{
			archive(encrypted, name, bytes);
		}

		template<class Archive>
		void save(Archive &archive) const
		{
			archive(encrypted, name, bytes);
		}

		item();

	private:
		friend class GPPPackage;
		bool decrypted;
	};

	std::string packageDescription;
	std::deque<item> items;

	template<class Archive>
	void load(Archive &archive)
	{
		archive(packageDescription, items);
	}

	template<class Archive>
	void save(Archive &archive) const
	{
		archive(packageDescription, items);
	}

	void decrypt();
	void pushFile(const std::string &file, const std::string &name, bool encrypt);

	GPPPackage();
};

class CGPPFileMGR
{

public:
	static CGPPFileMGR &mgr();

	GPPPackage loadPackage(const std::string &file);

	template<class T>
	inline static std::streamsize fileSize(T &file)
	{
		file.seekg(0, std::ios::beg);
		file.ignore(std::numeric_limits<std::streamsize>::max());
		std::streamsize offset = file.gcount();
		file.seekg(0, std::ios::beg);
		file.clear();
		return offset;
	}

	GPPPackage newPackageFromDirectory(const std::string &packName, const std::string &dir);

private:
	CGPPFileMGR();
};


#endif
