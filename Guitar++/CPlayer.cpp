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

const int CPlayer::notesEnum = nf_green | nf_red | nf_yellow | nf_blue | nf_orange;

std::string CPlayer::smartChartSearch(const std::string &path){
	auto file_exists = [](const std::string &fileName){
		return std::fstream(fileName).is_open();
	};

	const std::string fullPath = std::string("data/songs/") + path;

	const std::string chartFormats[] = {
		path + ".GPPXT",
		path + ".chart",
		"Chart.GPPXT",
		"Chart.chart"
	};

	for (auto &str : chartFormats){
		if (file_exists(fullPath + std::string("/") + str))
			return str;
	}

	return "";
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

	if (isChartOpen){
		//songAudioID = -1;
	}

	return true;
}

bool CPlayer::loadSong(const std::string &path){
	std::string fullFilePath = (std::string("data/songs/") + path + std::string("/") + smartChartSearch(path));

	bool isChartOpen = fullFilePath.find(".chart") != std::string::npos ? Notes.loadFeedbackChart(fullFilePath.c_str()) : Notes.loadChart(fullFilePath.c_str());

	if (isChartOpen){
		//CEngine::engine().loadSoundStream((std::string("data/songs/") + path + std::string("/") + smartSongSearch(path)).c_str(), songAudioID);

	}

	return true;
}

#include <iostream>

int CPlayer::getLevel(){
	return log(experience);
}

// TODO Plus load
bool CPlayer::NotesData::loadFeedbackChart(const char *chartFile){
	typedef std::map < std::string, std::map<std::string, std::deque<std::string>> > parsedChart;
	chartFileName = chartFile;

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

	auto &parseFeedBackChart = [](parsedChart &data, std::string chartFile){
		std::ifstream chart(chartFile);
		char temp[1024];

		std::string myScope = "nothing";

		while (chart.getline(temp, sizeof(temp))){
			if (chart.fail())
				return;

			std::string str = temp;

			if (*temp == '['){
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
				sB = (sB == str.npos || sB >= s) ? (s - 1) : (sB - 1);

				std::string name;
				name.insert(0, str, d, sB);

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

	auto pureBPMToCalcBPM = [](double BPM){
		return BPM / 1000.0 * 3.2;
	};

	auto getNoteTime = [&pureBPMToCalcBPM](const BPMContainer &BPMs, int pos, int off){
		double timeT = 0.0;
		int i = 0;

		for (; i < pos; i++){
			timeT += (BPMs[i + 1].offset - BPMs[i].offset) / pureBPMToCalcBPM(BPMs[i].BPM);
		}

		return timeT + (off - BPMs[pos].offset) / pureBPMToCalcBPM(BPMs[pos].BPM);
	};

	auto noteRead = [&pureBPMToCalcBPM, &getNoteTime](noteContainer &NTS, const BPMContainer &BPMs, parsedChart &chartMap, std::string difficulty){
		for (auto &scopeData : chartMap[difficulty]){
			char c[16] = { 0 };
			int i = 0, j = 0;
			for (auto &inst : scopeData.second){
				if (sscanf(inst.c_str(), "%15s %d %d", c, &i, &j) == 3){
					if (std::string(c) == "N"){
						Note nt;
						nt.time = std::stod(scopeData.first);
						nt.lTime = j;
						nt.type = i;

						NTS.push_back(nt);
					}
				}
			}
		}

		std::sort(NTS.begin(), NTS.end());

		int BPM = 0;
		for (auto &nt : NTS){
			if (BPM < (BPMs.size() - 1)){
				if (BPMs[BPM + 1].offset < nt.time)
					++BPM;
			}

			nt.time = getNoteTime(BPMs, BPM, nt.time);
			nt.lTime = nt.lTime / pureBPMToCalcBPM(BPMs[BPM].BPM);
		}
	};

	parseFeedBackChart(feedBackChartMap, chartFile);
	BPMRead(BPMs, feedBackChartMap);
	noteRead(Nts, BPMs, feedBackChartMap, "[ExpertSingle]");

	int p = 0;
	for (auto &BP : BPMs){
		Note newNote;
		newNote.time = getNoteTime(BPMs, p, BP.offset);
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

	for (size_t i = 0, size = Nts.size(); i < size; i++){
		if (isnan(Nts[i].time) || isnan(Nts[i].lTime)){
			continue;
		}

		if (Nts[i].type >= 0 && Nts[i].type < 5 && Nts[i].time > 0.0){
			if (gNotes.size() == 0){
				Note newNote;
				newNote.time = Nts[i].time;
				newNote.unmodifiedTime = Nts[i].time;
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
					newNote.unmodifiedTime = Nts[i].time;
					newNote.lTime = Nts[i].lTime;
					newNote.type = (int)pow(2, Nts[i].type);
					if (newNote.lTime > 0.0) newNote.type |= nf_slide;

					hopoTest(newNote);

					gNotes.push_back(newNote);
				}
			}
		}
	}

	return true;
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
						newNote.unmodifiedTime = ntInfoTime;
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
							newNote.unmodifiedTime = ntInfoTime;
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

	return true;
}

void CPlayer::NotesData::unloadChart(){
	notePos = 0;
	lastNotePicked = -1;
	longNoteComb = 0;

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
}

void CPlayer::resetData(){

}

CPlayer::NotesData::NotesData(){
	notePos = 0;
	lastNotePicked = -1;
	longNoteComb = 0;

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

}

void CPlayer::breakCombo()
{

}

double CPlayer::comboToMultiplierWM()
{

}

double CPlayer::comboToMultiplier()
{
	double result = double(combo) / 10.0;

	result = result < 1.0 ? 1.0 : result;
	result = result > 4.0 ? 4.0 : result;

	return result * (plusEnabled + 1.0);
}

CPlayer::NotesData::~NotesData(){
	unloadChart();
}

CPlayer::CPlayer(const char *name){
	combo = 0;
	plusEnabled = false;
}
