#pragma warning(disable : 4503)
#include "CPlayer.h"
#include "CEngine.h"
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstddef>
#include <climits>
#include <cfloat>
#include <fstream>
#include <algorithm>
#include "CText.h"
#include "CLog.h"
#include "GPPGame.h"
#include "CMultiplayer.h"
#include "CFonts.h"

const int CPlayer::notesEnum = nf_green | nf_red | nf_yellow | nf_blue | nf_orange;
const int CPlayer::notesEnumWithOpenNotes = nf_green | nf_red | nf_yellow | nf_blue | nf_orange | nf_open;

std::string CPlayer::smartChartSearch(const std::string &path){
	auto file_exists = [](const std::string &fileName){
		return std::fstream(fileName).is_open();
	};

	const std::string fullPath = std::string("data/songs/") + path;

	const std::string chartFormats[] = {
		path + ".GPPXT",
		path + ".chart",
		"Chart.GPPXT",
		"Chart.chart",
		"notes.chart",
		"chart.chart"
	};

	for (auto &str : chartFormats){
		if (file_exists(fullPath + std::string("/") + str))
			return str;
	}

	return "";
}

void CPlayer::muteInstrument()
{
	CEngine::engine().setSoundVolume(instrumentSound, 0.0f);
}

void CPlayer::unmuteInstrument()
{
	CEngine::engine().setSoundVolume(instrumentSound, 1.0f);
}

std::string trim(const std::string &str)
{
	size_t first = str.find_first_not_of(' ');
	size_t last = str.find_last_not_of(' ');

	if (first == str.npos)
		return str;

	if (last == str.npos)
		last = str.size();

	return str.substr(first, last - first + 1);
}

std::atomic<int> palhetaNpKey;

void CPlayer::update()
{
	int i = 0;
	int pklast = palhetaKeyLast;

	if ((CEngine::engine().getTime() - npPsetted) > 0.01 && enableBot)
	{
		palhetaNpKey = false;
	}

	bool clearFretsP = true;

	if (enableBot)
	{
		std::copy(fretsPressed, fretsPressed + 5, lastFretsPressed);

		CFonts::fonts().drawTextInScreen("BOT PLAYING", -1.65, -0.9, 0.05);

		for (int i = 0; i < 5; ++i)
		{
			fretsPressed[i] = false;
		}

		bool dngslide = false;

		if (Notes.lastNotePicked != -1 && strklinent != -1)
		{
			auto &note = Notes.gNotes[Notes.lastNotePicked];

			double rtime = (note.time - musicRunningTime);

			if (rtime < 0.1 && rtime > -0.1)
			{
				for (int i = 0; i < 5; ++i)
				{
					if (note.type & (int)pow(2, i))
					{
						fretsPressed[i] = true;
						clearFretsP = false;
					}
				}
			}
			else
			{
				for (int i = 0; i < 5; ++i)
				{
					fretsPressed[i] = false;
				}
			}
		}

		for (int i = 0; i < 5; ++i)
		{
			if (notesSlide[i] != -1)
			{
				fretsPressed[i] = true;
				clearFretsP = false;
				dngslide = true;
			}
		}

		if (strklinent != -1 && !dngslide)
		{
			auto &note = Notes.gNotes[strklinent];

			strklinenttime = (note.time - musicRunningTime);

			if (strklinenttime >= -0.05 && ((strklinenttime < 0.14 && (note.type & notesFlags::nf_not_hopo)) || (strklinenttime < 0.04)))
			{
				for (int i = 0; i < 5; ++i)
				{
					if (note.type & (int)pow(2, i))
					{
						fretsPressed[i] = true;
					}
					else{
						fretsPressed[i] = false;
					}
				}

				if (strklinenttime < 0.04 && !(note.type & notesFlags::nf_picked) && !(note.type & notesFlags::nf_failed) && !(note.type & notesFlags::nf_doing_slide) && !(note.type & notesFlags::nf_slide_picked))
				{
					palhetaKey = true;
					palhetaNpKey = palhetaKey;
					npPsetted = CEngine::engine().getTime();
				}
				else{
					palhetaKey = false;
					palhetaKeyLast = palhetaKey;
				}
			}
		}
		else
		{
			palhetaKey = false;
			palhetaKeyLast = palhetaKey;
		}
	}
	else if (!remoteControls) {
		palhetaNpKey = palhetaKey = CEngine::engine().getKey(GPPGame::GuitarPP().fretOneKey) || CEngine::engine().getKey(GPPGame::GuitarPP().fretTwoKey);

		if (pklast == palhetaKey)
		{
			palhetaKey = false;
		}
		else
		{
			palhetaKeyLast = palhetaKey;
		}

		std::copy(fretsPressed, fretsPressed + 5, lastFretsPressed);

		int countI = 0;

		double timeC = CEngine::engine().getTime();

		for (auto &f : fretsPressed)
		{
			bool ftemp = f;
			f = CEngine::engine().getKey(GPPGame::GuitarPP().strumKeys[i++]);

			if (ftemp != f)
			{
				palhetaKey = false;

				if (f)
					fretsPressedTime[countI] = timeC;
			}

			countI++;
		}
	}
	else
	{
		auto &pData = CMultiplayer::sgetPlData();

		CMultiplayer::playersData pdata;

		for (int i = 0, size = pData.size(); i < size; i++)
		{
			if (plname == pData[i].name)
			{
				//std::cout << pData[i].keys << std::endl;
				pdata = pData[i];
				break;
			}
		}

		palhetaNpKey = palhetaKey = pdata.strum;

		if (pklast == palhetaKey)
		{
			palhetaKey = false;
		}
		else
		{
			palhetaKeyLast = palhetaKey;
		}

		std::copy(fretsPressed, fretsPressed + 5, lastFretsPressed);

		int keys[5];

		for (int j = 0; j < 5; j++)
		{
			keys[j] = 0;

			if (pdata.keys & (int)pow(2, j))
			{
				keys[j] = 1;
			}
		}

		double timeC = CEngine::engine().getTime();
		
		for (auto &f : fretsPressed)
		{
			bool ftemp = f;
			f = keys[i++];

			if (ftemp != f)
			{
				palhetaKey = false;

				if (f)
					fretsPressedTime[i] = timeC;
			}
		}
	}
}

std::string CPlayer::smartSongSearch(const std::string &path){
	auto file_exists = [](const std::string &fileName){
		return std::fstream(fileName).is_open();
	};

	const std::string fullPath = std::string("data/songs/") + path;

	const std::string songsFormats[] = {
		path + ".ogg",
		path + ".mp3",
		"Song.ogg",
		"Song.mp3"
	};

	for (auto &str : songsFormats){
		if (file_exists(fullPath + std::string("/") + str))
			return str;
	}

	return "";
}

bool CPlayer::loadSongOnlyChart(const std::string &path){
	std::string fullFilePath = (std::string("data/songs/") + path + std::string("/") + smartChartSearch(path));

	bool isChartOpen = fullFilePath.find(".chart") != std::string::npos ? Notes.loadFeedbackChart(fullFilePath.c_str()) : Notes.loadChart(fullFilePath.c_str());

	if (songAudioID != -1 && songAudioID)
	{
		CEngine::engine().unloadSoundStream(songAudioID);
	}

	songAudioID = -1;

	return true;
}

bool CPlayer::loadSong(const std::string &path){
	std::string fullFilePath = (std::string("data/songs/") + path + std::string("/") + smartChartSearch(path));

	bool isChartOpen = fullFilePath.find(".chart") != std::string::npos ? Notes.loadFeedbackChart(fullFilePath.c_str()) : Notes.loadChart(fullFilePath.c_str());

	if (songAudioID != -1 && songAudioID)
	{
		CEngine::engine().unloadSoundStream(songAudioID);
	}

	if (instrumentSound != -1 && instrumentSound)
	{
		CEngine::engine().unloadSoundStream(instrumentSound);
	}

	songAudioID = -1;

	if (isChartOpen){
		CLog::log() << smartSongSearch(path);
		Notes.songFullPath = (std::string("data/songs/") + path + std::string("/") + smartSongSearch(path));

		CLog::log() << "loadSoundStream result: " + std::to_string(CEngine::engine().loadSoundStream(Notes.songFullPath.c_str(), songAudioID));

		if (Notes.instrument == "[ExpertSingle]")
		{
			CLog::log() << "loadSoundStream instrumentSound result: " + std::to_string(CEngine::engine().loadSoundStream((std::string("data/songs/") + path + std::string("/guitar.ogg")).c_str(), instrumentSound));
		}
		else if (Notes.instrument == "[ExpertDoubleBass]")
		{
			CLog::log() << "loadSoundStream instrumentSound result: " + std::to_string(CEngine::engine().loadSoundStream((std::string("data/songs/") + path + std::string("/rhythm.ogg")).c_str(), instrumentSound));
		}
	}

	return true;
}

#include <iostream>

int CPlayer::getLevel(){
	return log(experience);
}

bool CPlayer::NotesData::loadFeedbackChart(const char *chartFile){
	typedef std::map < std::string, std::map<std::string, std::deque<std::string>> > parsedChart;
	chartFileName = chartFile;
	plusPos = 0;

	parsedChart feedBackChartMap;

	struct SyncTrackBPM{
		double BPM, offset;

		bool operator <(const SyncTrackBPM &b) const{
			return offset < b.offset;
		}
		bool operator >(const SyncTrackBPM &b) const{
			return offset > b.offset;
		}
	};

	typedef std::deque<Note> noteContainer;
	typedef std::deque<SyncTrackBPM> BPMContainer;
	noteContainer Nts;
	BPMContainer BPMs;

	auto parseFeedBackChart = [](parsedChart &data, std::string chartFile){
		std::ifstream chart(chartFile);
		char temp[1024];

		std::string myScope = "nothing";

		while (chart.getline(temp, sizeof(temp)))
		{
			if (chart.fail())
				return;

			std::string str = temp;

			str = trim(str);

			if (str.c_str()[0] == '['){
				myScope = str;
			}
			else{
				size_t s = str.find_first_of('='), sB, sC;
				size_t d = str.find_first_not_of('	');

				d = (d == str.npos) ? str.find_first_not_of(' ') : d;
				d = (d == str.npos) ? 0uL : d;

				if (s == str.npos)
					continue;

				sB = str.find_first_of(' ', d);
				sB = (sB == str.npos || sB >= s) ? (s - 1) : (sB);

				std::string name;

				if (!isblank(str[0]))
				{
					d = 0;
				}
				
				name.insert(0, str, d, sB);

				name = trim(name);

				sC = str.find_first_not_of(' ', ++s);
				sC = sC == str.npos ? s : sC;

				std::string contents;
				contents.insert(0, str, sC, str.npos);

				data[myScope][name].push_back(contents);
			}
		}
	};

	auto BPMRead = [](BPMContainer &BPMs, parsedChart &chartMap){
		for (auto &SyncTrack : chartMap["[SyncTrack]"]){
			char c[16] = {0};
			int i = 0;
			for (auto &inst : SyncTrack.second){
				if (sscanf(inst.c_str(), "%15s %d", c, &i) == 2){
					if (std::string(c) == "B"){
						SyncTrackBPM bp;
						bp.BPM = i;
						bp.offset = std::stod(SyncTrack.first);

						BPMs.push_back(bp);
					}
				}
			}
		}

		std::sort(BPMs.begin(), BPMs.end());
	};

	auto pureBPMToCalcBPM = [this](double BPM){
		return (BPM / 1000.0 * 3.2) * chartResolutionProp;
	};

	auto getNoteTime = [&pureBPMToCalcBPM](const BPMContainer &BPMs, int64_t pos, int64_t off){
		double timeT = 0.0;
		int i = 0;

		if (pos > (BPMs.size() - 1))
		{
			pos = BPMs.size() - 1;
		}

		for (; i < pos; i++){
			timeT += (BPMs[i + 1].offset - BPMs[i].offset) / pureBPMToCalcBPM(BPMs[i].BPM);
		}

		/*if (off < BPMs[pos].offset)
		{
			timeT -= (BPMs[i + 1].offset - BPMs[i].offset) / pureBPMToCalcBPM(BPMs[i].BPM);
			return timeT + (off - BPMs[i].offset) / pureBPMToCalcBPM(BPMs[i].BPM);
		}*/

		return timeT + (off - BPMs[pos].offset) / pureBPMToCalcBPM(BPMs[pos].BPM);
	};

	auto getRefBPM = [](const BPMContainer &BPMs, int64_t tick){
		int result = 0;
		int irbp = 0;

		for (auto &b : BPMs)
		{
			if (b.offset < tick)
			{
				result = irbp;
			}
			else{
				break;
			}
			irbp++;
		}

		return result;
	};

	auto noteRead = [&pureBPMToCalcBPM, &getNoteTime, &getRefBPM](noteContainer &NTS, const BPMContainer &BPMs, parsedChart &chartMap, std::string difficulty){
		for (auto &scopeData : chartMap[difficulty]){
			char c[16] = { 0 };
			int i = 0, j = 0;
			for (auto &inst : scopeData.second){
				if (sscanf(inst.c_str(), "%15s %d %d", c, &i, &j) == 3)
				{
					if (std::string(c) == "N"){
						Note nt;
						nt.time = std::stod(scopeData.first);
						nt.lTime = j;
						nt.type = i;

						NTS.push_back(nt);
					}

					if (std::string(c) == "S" && i == 2){
						Note nt;
						nt.time = std::stod(scopeData.first);
						nt.lTime = j;
						nt.type = -1;

						NTS.push_back(nt);
					}
				}
			}
		}

		std::sort(NTS.begin(), NTS.end());

		uint64_t loffset = -1uL;
		double lnotet = -1.0;
		double llngnotet = -1.0;


		int BPM = 0;
		for (auto &nt : NTS)
		{
			uint64_t loffsettmp = nt.time;
			if (nt.type == -1)
			{
				nt.time = getNoteTime(BPMs, getRefBPM(BPMs, nt.time), nt.time);
				nt.lTime = getNoteTime(BPMs, getRefBPM(BPMs, loffsettmp + nt.lTime), loffsettmp + nt.lTime) - nt.time;
				continue;
			}

			if (BPM < (BPMs.size() - 1)){
				if (BPMs[BPM + 1].offset < nt.time)
					++BPM;
			}

			if (loffset == nt.time)
			{
				nt.time = lnotet;
				nt.lTime = llngnotet;
			}
			else
			{
				nt.time = getNoteTime(BPMs, getRefBPM(BPMs, nt.time), nt.time);
				nt.lTime = getNoteTime(BPMs, getRefBPM(BPMs, loffsettmp + nt.lTime), loffsettmp + nt.lTime) - nt.time;
			}

			lnotet = nt.time;
			llngnotet = nt.lTime;
			loffset = loffsettmp;
		}
	};

	auto fillChartInfo = [&](parsedChart &chartMap)
	{
		auto chk = [](std::deque<std::string> &s, int i)
		{
			std::string result;

			if (i < s.size())
				result = s[i];

			return result;
		};

		auto &Song = chartMap["[Song]"];
		
		chartResolutionProp = std::stod(Song["Resolution"][0]) / 192.0;
		songName = chk(Song["Name"], 0);
		songArtist = chk(Song["Artist"], 0);
		songCharter = chk(Song["Charter"], 0);

		std::cout << chartResolutionProp << std::endl;
	};




	parseFeedBackChart(feedBackChartMap, chartFile);

	fillChartInfo(feedBackChartMap);

	BPMRead(BPMs, feedBackChartMap);
	noteRead(Nts, BPMs, feedBackChartMap, instrument); // Default: "[ExpertSingle]"

	/*
	int bpdqpos = 0;
	for (auto &bpdq : BPMs)
	{
		std::cout << "BPM " << bpdq.BPM / 1000.0 << "  offset " << bpdq.offset << "   " << getNoteTime(BPMs, bpdqpos, bpdq.offset) << std::endl;
		bpdqpos++;
	}
	*/
	int p = 0;
	for (auto &BP : BPMs){
		Note newNote;
		newNote.time = getNoteTime(BPMs, p, (int64_t)BP.offset);
		newNote.lTime = BP.BPM / 1000.0;

		BPM.push_back(newNote);

		++p;
	}

	auto hopoTest = [this](Note &note){
		size_t size = gNotes.size(), i;
		i = size - 1;
		if (size > 0){
			size_t BPMsize = BPM.size(), BPMi;
			BPMi = BPMsize - 1;

			double BPMStepCalc = 60.0 / 120.0;

			if (BPMsize > 0){
				BPMStepCalc = 60.0 / BPM[BPMi].lTime;
			}

			for (auto &b : BPM)
			{
				if (b.time > note.time)
					break;

				BPMStepCalc = 60.0 / b.lTime;
			}

			BPMStepCalc /= 2.05;

			if ((note.time - gNotes[i].time) >= BPMStepCalc)
			{
				note.type |= nf_not_hopo;
			}

			int type1 = note.type & notesEnumWithOpenNotes, type2 = gNotes[i].type & notesEnumWithOpenNotes;

			if (type1 == type2)
			{
				note.type |= nf_not_hopo;
			}
		}
		else
		{
			note.type |= nf_not_hopo;
		}
	};

	auto roundTo = [](double num, double dec)
	{
		double v = pow(10.0, dec);
		num *= v;
		num = floor(num);
		num /= v;
		return num;
	};

	for (size_t i = 0, size = Nts.size(); i < size; i++){
		if (isnan(Nts[i].time) || isnan(Nts[i].lTime)){
			continue;
		}

		if (Nts[i].type >= 0 && Nts[i].type < 5 && Nts[i].time > 0.0)
		{
			if (gNotes.size() == 0){
				Note newNote;
				newNote.time = Nts[i].time;
				//newNote.unmodifiedTime = Nts[i].time;
				newNote.lTime = Nts[i].lTime;
				newNote.type = (int)pow(2, Nts[i].type);

				if (newNote.lTime > 0.0) newNote.type |= nf_slide;

				hopoTest(newNote);

				gNotes.push_back(newNote);
			}
			else{
				if (gNotes[gNotes.size() - 1].time == Nts[i].time){
					gNotes[gNotes.size() - 1].type |= (int)pow(2, Nts[i].type);
					gNotes[gNotes.size() - 1].type |= nf_not_hopo;
				}
				else{
					Note newNote;
					newNote.time = Nts[i].time;
					//newNote.unmodifiedTime = Nts[i].time;
					newNote.lTime = Nts[i].lTime;
					newNote.type = (int)pow(2, Nts[i].type);
					if (newNote.lTime > 0.0) newNote.type |= nf_slide;

					hopoTest(newNote);

					gNotes.push_back(newNote);
				}
			}
		}
		else if (Nts[i].type == 6)
		{
			if (gNotes.size() > 0)
			{
				if (gNotes[gNotes.size() - 1].time == Nts[i].time)
				{
					gNotes[gNotes.size() - 1].type |= noteTap;
				}
			}
		}
		else if (Nts[i].type == 7)
		{
			if (gNotes.size() == 0)
			{
				Note newNote;
				newNote.time = Nts[i].time;
				newNote.lTime = 0;
				newNote.type = nf_open;

				hopoTest(newNote);

				gNotes.push_back(newNote);
			}
			else
			{
				if (gNotes[gNotes.size() - 1].time == Nts[i].time)
				{
					gNotes[gNotes.size() - 1].type |= nf_not_hopo;
				}
				else
				{
					Note newNote;
					newNote.time = Nts[i].time;
					newNote.lTime = 0;
					newNote.type = nf_open;

					hopoTest(newNote);

					gNotes.push_back(newNote);
				}
			}
		}
		else if (Nts[i].type == -1)
		{
			plusNote plusT;

			plusT.time = Nts[i].time;
			plusT.lTime = Nts[i].lTime;
			plusT.type = 0;

			gPlus.push_back(plusT);
		}
	}

	deducePlusLastNotes();

	return true;
}

void CPlayer::NotesData::deducePlusLastNotes()
{
	size_t plusPosTemp = 0;

	for (int64_t i = 0, size = gNotes.size(); i < size; ++i)
	{
		const auto &note = gNotes[i];

		if (plusPosTemp < gPlus.size())
		{
			auto &plusNote = gPlus[plusPosTemp];

			if (note.time >= plusNote.time && note.time < (plusNote.time + plusNote.lTime))
			{
				if (plusNote.firstNote == -1)
				{
					plusNote.firstNote = i;
				}

				plusNote.lastNote = i;
			}

			if (note.time >= (plusNote.time + plusNote.lTime))
			{
				++plusPosTemp;
			}
		}
		else
		{
			break;
		}
	}
}

bool CPlayer::NotesData::loadChart(const char *chartFile){
	chartFileName = chartFile;

	CText ACTEXTChart(chartFile, true);

	std::deque<CText::field_t> &textArray = ACTEXTChart[""].fields;

	double ntInfoTime, ntInfoLTime;
	int ntIDInfo, scanResult;

	Note PlusNow;

	PlusNow.type = -1;

	auto plusFunc = [&PlusNow, this](Note &note){
		if (PlusNow.type == -1)
			return;

		if (note.time >= PlusNow.time && note.time <= PlusNow.lTime){
			note.type |= plus_mid;
		}
	};

	auto hopoTest = [this](Note &note){
		size_t size = gNotes.size(), i;
		i = size - 1;
		if (size > 0){
			size_t BPMsize = BPM.size(), BPMi;
			BPMi = BPMsize - 1;

			double BPMStepCalc = 60.0 / 120.0;

			if (BPMsize > 0){
				BPMStepCalc = 60.0 / BPM[BPMi].lTime;
			}

			for (auto &b : BPM)
			{
				if (b.time > note.time)
					break;

				BPMStepCalc = 60.0 / b.lTime;
			}

			BPMStepCalc /= 2.05;

			if ((note.time - gNotes[i].time) >= BPMStepCalc){
				note.type |= nf_not_hopo;
			}

			int type1 = note.type & notesEnum, type2 = gNotes[i].type & notesEnum;

			if (type1 == type2){
				note.type |= nf_not_hopo;
			}
		}
		else{
			note.type |= nf_not_hopo;
		}
	};

	for(size_t i = 0, size = textArray.size(); i < size; i++){
		if(textArray[i].name[0] == 'N'){
			scanResult = sscanf(textArray[i].content.c_str(), "%d %lf %lf", &ntIDInfo, &ntInfoTime, &ntInfoLTime);
			
			if(isnan(ntInfoTime) || isnan(ntInfoLTime)){
				continue;
			}

			if(scanResult == 3){
				if(ntIDInfo >= 0 && ntIDInfo < 5 && ntInfoTime > 0.0){
					if(gNotes.size() == 0){
						Note newNote;
						newNote.time = ntInfoTime;
						//newNote.unmodifiedTime = ntInfoTime;
						newNote.lTime = ntInfoLTime;
						newNote.type = (int)pow(2, ntIDInfo);
						if(newNote.lTime > 0.0) newNote.type |= nf_slide;

						plusFunc(newNote);
						hopoTest(newNote);

						gNotes.push_back(newNote);
					}else{
						if(gNotes[gNotes.size() - 1].time == ntInfoTime){
							gNotes[gNotes.size() - 1].type |= (int)pow(2, ntIDInfo);
							gNotes[gNotes.size() - 1].type |= nf_not_hopo;
						}else{
							Note newNote;
							newNote.time = ntInfoTime;
							//newNote.unmodifiedTime = ntInfoTime;
							newNote.lTime = ntInfoLTime;
							newNote.type = (int)pow(2, ntIDInfo);
							if(newNote.lTime > 0.0) newNote.type |= nf_slide;

							plusFunc(newNote);
							hopoTest(newNote);

							gNotes.push_back(newNote);
						}
					}
				}
				else if (ntIDInfo == 5){
					PlusNow.type = 1;
					PlusNow.time = ntInfoTime;
					PlusNow.lTime = ntInfoLTime;
				}
				else if (ntIDInfo == 6){
					Note newNote;
					newNote.time = ntInfoTime;
					newNote.lTime = ntInfoLTime;

					BPM.push_back(newNote);
				}
			}
		}
	}

	deducePlusLastNotes();

	return true;
}

void CPlayer::NotesData::unloadChart(){
	notePos = 0;
	lastNotePicked = -1;
	longNoteComb = 0;
	chartResolutionProp = 1.0;
	plusPos = 0;

	for (auto &fretNotePickedTime : fretsNotePickedTime){
		fretNotePickedTime = 0.0;
	}

	for (auto &inLN : inLongNote){
		inLN = false;
	}

	for (auto &LNID : longNoteID){
		LNID = -1;
	}

	if (gNotes.size() > 0){
		gNotes.clear();
		chartFileName = "";
	}

	BPM.clear();
	gPlus.clear();
}

void CPlayer::resetData()
{
	correctNotes = 0;
	bRenderP = bUpdateP = true;
}

CPlayer::NotesData::NotesData(){
	notePos = 0;
	lastNotePicked = -1;
	longNoteComb = 0;
	chartResolutionProp = 1.0;
	plusPos = 0;
	BPMMinPosition = 0.0;

	instrument = "[ExpertSingle]";

	for(auto &fretNotePickedTime : fretsNotePickedTime)
	{
		fretNotePickedTime = 0.0;
	}

	for(auto &inLN : inLongNote)
	{
		inLN = false;
	}

	for(auto &LNID : longNoteID)
	{
		LNID = -1;
	}
}

void CPlayer::updateControls()
{
	if (playerType & (int)playerTypes::local_main || playerType & (int)playerTypes::local){
		//Controls.updateInst();
	}
}

void CPlayer::addPointsByNoteDoed()
{

}

void CPlayer::addPointsByDoingLongNote()
{
	points += comboToMultiplier() * 200.0 * CEngine::engine().getDeltaTime();
}

void CPlayer::processError()
{
	breakCombo();
	aError = true;
	muteInstrument();

	if (publicAprov > 0) publicAprov--;
}

void CPlayer::releaseSong()
{
	if (songAudioID != -1 && songAudioID)
	{
		CEngine::engine().unloadSoundStream(songAudioID);
	}

	if (instrumentSound != -1 && instrumentSound)
	{
		CEngine::engine().unloadSoundStream(instrumentSound);
	}

	songAudioID = -1;
}

void CPlayer::breakCombo()
{
	combo = 0;
}

double CPlayer::comboToMultiplierWM()
{
	return comboToMultiplier();
}

int64_t CPlayer::getCombo()
{
	return combo;
}

int64_t CPlayer::getPoints()
{
	return points;
}

bool CPlayer::isSongChartFinished()
{
	auto size = Notes.gNotes.size();
	if (Notes.gNotes.size() > 0)
	{
		auto &lastNote = Notes.gNotes[size - 1];

		double finalTime = lastNote.time + lastNote.lTime;

		if ((musicRunningTime - finalTime) > 2.0)
		{
			return true;
		}
	}
	else
	{
		return true;
	}

	return false;
}

double CPlayer::comboToMultiplier()
{
	double result = double(combo) / 10.0;

	result = result < 1.0 ? 1.0 : result;
	result = result > 4.0 ? 4.0 : result;

	return result * (plusEnabled + 1.0);
}

int CPlayer::getLastFretsPressedFlags()
{
	int result = 0;
	for (int i = 0; i < 5; ++i)
	{
		if (lastFretsPressed[i])
		{
			result |= (int)pow(2, i);
		}
	}

	return result;
}

int CPlayer::getFretsPressedFlags()
{
	int result = 0;
	for (int i = 0; i < 5; ++i)
	{
		if (fretsPressed[i])
		{
			result |= (int)pow(2, i);
		}
	}

	return result;
}

void CPlayer::doNote(int64_t i)
{
	if (!(Notes.gNotes[i].type & notesFlags::nf_picked))
	{
		++combo;
		++correctNotes;

		Notes.lastNotePicked = i;

		points += comboToMultiplier() * 200.0;

		publicAprov++;

		aError = false;

		unmuteInstrument();

		if (publicAprov > maxPublicAprov)
		{
			publicAprov = maxPublicAprov;
		}
	}
}

CPlayer::NotesData::~NotesData(){
	unloadChart();
}

void CPlayer::instrumentPlay()
{
	CEngine::engine().playSoundStream(instrumentSound);
}

void CPlayer::instrumentPause()
{
	CEngine::engine().pauseSoundStream(instrumentSound);
}

CPlayer::CPlayer(const char *name)
{
	correctNotesMarathon = 0;
	guitar = nullptr;
	npPsetted = 0;
	playerHudOffsetX = playerHudOffsetY = 0.0;
	remoteControls = false;
	bRenderP = bUpdateP = true;
	plname = name;
	songAudioID = -1;
	points = combo = 0;
	startTime = CEngine::engine().getTime();
	plusEnabled = false;
	musicRunningTime = 0.0;
	rangle = 0;
	palhetaKeyLast = palhetaKey = false;
	aError = false;
	lastHOPO = 0;
	enableBot = false;

	memset(notesSlide, -1, sizeof(notesSlide));
	memset(lastFretsPressed, 0, sizeof(lastFretsPressed));
	memset(fretsPressed, 0, sizeof(fretsPressed));
	memset(fretsPressedTime, 0.0, sizeof(fretsPressedTime));
	
	instrumentSound = 0;
	strklinenttime = -5.0;

	BPMNowBuffer = 0;

	plusThunterStrikeStart = plusPower = plusParticleEffectPosition = maxPlusPower = plusLoadF = plusLoadB = plusLoadInterval = 0.0;
	plusCircleBuffer = plusLoadBuffer = publicApprovBuffer = correctNotesBuffer = multiplierBuffer = -1uL;

	correctNotes = 0;

	maxPlusPower = 1.0;
	maxPublicAprov = 120.0;
	publicAprov = maxPublicAprov / 2.0;

	strklinent = -1;

	playerCamera.eyex = 0.0;
	playerCamera.eyey = 0.2;
	playerCamera.eyez = 2.3;
	playerCamera.centerx = 0;
	playerCamera.centery = -0.2;
	playerCamera.centerz = 0;
	playerCamera.upx = 0;
	playerCamera.upy = 1;
	playerCamera.upz = 0;

	targetCamera = playerCamera;
}
