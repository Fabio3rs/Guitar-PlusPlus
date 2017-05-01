#include "CGPPFileMGR.h"
#include "gppEncryption.h"

void GPPPackage::decrypt()
{
	for (auto &i : items)
	{
		if (!i.decrypted && i.encrypted)
		{
			gppDecrypt(&i.bytes[0], i.bytes.size());
		}
	}
}

void GPPPackage::pushFile(const std::string &file, const std::string &name, bool encrypt)
{
	item ni;

	std::fstream f(file, std::ios::in | std::ios::binary);

	size_t s = CGPPFileMGR::fileSize(f);

	ni.bytes.insert(ni.bytes.begin(), s, '\0');
	f.read((char*)&ni.bytes[0], s);

	ni.encrypted = encrypt;
	ni.decrypted = false;
	gppEncrypt(&ni.bytes[0], ni.bytes.size());

	items.push_back(std::move(ni));
}

GPPPackage::GPPPackage()
{
	loaded = false;

}

GPPPackage::item::item()
{
	encrypted = false;
	decrypted = false;
}

GPPPackage CGPPFileMGR::loadPackage(const std::string &file)
{
	GPPPackage result;
	std::fstream fs(file);

	cereal::BinaryInputArchive iarchive(fs);

	iarchive(result);

	result.loaded = true;

	return result;
}

CGPPFileMGR::CGPPFileMGR()
{


}
