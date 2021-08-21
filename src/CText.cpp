// OLD CTEXT CLASS

#include "CText.h"

#include <cctype>
#include <iostream>
#include <memory>
#include "CGPPFileMGR.h"

void CText::Parse()
{
	if(fileName.length() == 0){
		return;
	}
	
	if(is_open()){
		file.close();
	}

	file.open(fileName, std::ios::in | std::ios::out | std::ios::binary);

	if(!is_open()){
		throw std::logic_error("Can't open file");
	}

	tables.clear();
	
	fileSize = CGPPFileMGR::fileSize(file);

	if(fileSize == -1L || fileSize == 0)
	{
		return;
	}
	
	std::unique_ptr<char[]> content(std::make_unique<char[]>(fileSize + 16));
	memset(content.get(), 0, fileSize + 16);
	
	file.read(content.get(), fileSize);
	
	content[fileSize] = '\n';
	content[fileSize + 1] = '\n';
	content[fileSize + 2] = 0;
	
	char bufferA[128], bufferB[2048];
	
	integer workingInScope = 0;
	
	table_t globalTable;
	globalTable.name = "GLOBAL";
	tables.push_back(globalTable);

	for(size_t i = 0; i < fileSize; i++){
		while(!isprint((unsigned char)content[i])) i++;

		*bufferA = 0;
		*bufferB = 0;

		int scanResult = sscanf(&content[i], "%127s %2047[^\t\n\r]", bufferA, bufferB);
		
		if (std::string(bufferA).size() > 0) {
			if (*bufferA == '@') {
				integer tempWorkingScope = 0;

				if ((tempWorkingScope = getTableIDByName(&bufferA[1])) != -1) {
					workingInScope = tempWorkingScope;
				}
				else if (bufferA[1]) {
					table_t newTable;
					newTable.name = &bufferA[1];

					tables.push_back(newTable);

					workingInScope = tables.size() - (int64_t)1L;
				}
				else {
					workingInScope = 0;
				}
			}
			else if (*bufferA == '#') {
			}
			else {
				field_t newField;

				switch (scanResult) {
				case 2:
					newField.content = bufferB;

				case 1:
					newField.name = bufferA;
					tables[workingInScope].fields.push_back(newField);
					break;
				}
			}
		}

		while(content[i] != '\n' && content[i] != '\r' && content[i] != 0) i++;
		i--;
	}
}

void CText::open(const char *name, bool autoParse){
	fileName = name;
	if (autoParse) Parse();
}

CText::CText(){
	fileSize = 0;

}

void CText::save(){
	if (is_open()){
		file.close();
	}

	file.open(fileName, std::ios::in | std::ios::out | std::ios::trunc);

	for (int i = 0, size = tables.size(); i < size; i++){
		file << "@" << tables[i].name << "\n";
		for (int j = 0, jsize = tables[i].fields.size(); j < jsize; j++){
			file << tables[i].fields[j].name << " " << tables[i].fields[j].content << "\n";
		}
		file << "\n###### fstream bugs everywhere ######";
	}
}

CText::CText(const char *name, const std::string &pushIfEmpty, bool autoParse)
{
	fileName = name;

	fileSize = ~((size_t)0);

	push = pushIfEmpty;

	auto pushContentsIfEmpty = [this](std::string pushIfEmpty, bool checkFile) {
		if (pushIfEmpty.size() != 0) {

			if (checkFile) {
				if(!is_open())
					file.open(fileName, std::ios::in | std::ios::out | std::ios::binary);

				fileSize = CGPPFileMGR::fileSize(file);
			}

			if (!checkFile || (checkFile && fileSize == 0)) {
				file.close();
				file.open(fileName, std::ios::in | std::ios::out | std::ios::trunc);

				file.write(pushIfEmpty.c_str(), pushIfEmpty.size());
			}
			file.close();
		}
	};

	pushContentsIfEmpty(pushIfEmpty, true);
	if (autoParse) Parse();


	if (tables.size() <= 1 && autoParse) {
		if (tables.size() == 1) {
			if (tables[0].fields.size() < 1) {
				pushContentsIfEmpty(pushIfEmpty, false);
			}
		}
		else {
			pushContentsIfEmpty(pushIfEmpty, false);
		}
		Parse();
	}
}

CText::CText(const char *name, bool autoParse){
	fileName = name;
	if(autoParse) Parse();
}

