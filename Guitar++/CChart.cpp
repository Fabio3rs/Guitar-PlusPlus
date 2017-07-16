#include "CChart.h"

static std::string trim(const std::string &str)
{
	size_t first = str.find_first_not_of(' ');
	size_t last = str.find_last_not_of(' ');

	if (first == str.npos)
		return str;

	if (last == str.npos)
		last = str.size();

	return str.substr(first, last - first + 1);
}

bool CChart::parseFeebackChart(std::istream &chartStream)
{
	typedef std::map < std::string, std::map<std::string, std::deque<std::string>> > parsedChart;
	double chartOffset = 0.0;

	parsedChart feedBackChartMap;

	struct SyncTrackBPM {
		double BPM, offset;

		bool operator <(const SyncTrackBPM &b) const {
			return offset < b.offset;
		}
		bool operator >(const SyncTrackBPM &b) const {
			return offset > b.offset;
		}
	};

	typedef std::deque<Note> noteContainer;
	typedef std::vector<SyncTrackBPM> BPMContainer;
	noteContainer Nts;
	BPMContainer BPMs;

	auto parseFeedBackChart = [&chartStream](parsedChart &data)
	{
		auto &chart = chartStream;
		char temp[1024];

		std::string myScope = "nothing";

		while (chart.getline(temp, sizeof(temp)))
		{
			if (chart.fail())
				return;

			std::string str = temp;

			str = trim(str);

			if (str.c_str()[0] == '[') {
				myScope = str;
			}
			else {
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

	auto BPMRead = [](BPMContainer &BPMs, parsedChart &chartMap) {
		for (auto &SyncTrack : chartMap["[SyncTrack]"])
		{
			char c[32] = { 0 };
			int i = 0;
			for (auto &inst : SyncTrack.second) {
				if (sscanf(inst.c_str(), "%31s %d", c, &i) == 2) {
					if (std::string(c) == "B") {
						SyncTrackBPM bp;
						bp.BPM = i;
						bp.offset = std::stod(SyncTrack.first);

						BPMs.push_back(bp);
					}
				}
			}
		}

		std::sort(BPMs.begin(), BPMs.end());

		if (BPMs.size() == 0)
		{
			SyncTrackBPM bp;
			bp.BPM = 120000;
			bp.offset = 0.0;
		}
	};

	auto pureBPMToCalcBPM = [this](double BPM) {
		return (BPM / 1000.0 * 3.2) * chartResolutionProp;
	};

	auto getNoteTime = [&pureBPMToCalcBPM](const BPMContainer &BPMs, size_t pos, int64_t off) {
		double timeT = 0.0;
		size_t i = 0;

		if (pos > (BPMs.size() - 1))
		{
			pos = BPMs.size() - 1;
		}

		for (; i < pos; i++) {
			timeT += (BPMs[i + 1].offset - BPMs[i].offset) / pureBPMToCalcBPM(BPMs[i].BPM);
		}

		/*if (off < BPMs[pos].offset)
		{
		timeT -= (BPMs[i + 1].offset - BPMs[i].offset) / pureBPMToCalcBPM(BPMs[i].BPM);
		return timeT + (off - BPMs[i].offset) / pureBPMToCalcBPM(BPMs[i].BPM);
		}*/

		return timeT + (off - BPMs[pos].offset) / pureBPMToCalcBPM(BPMs[pos].BPM);
	};

	auto getRefBPM = [](const BPMContainer &BPMs, int64_t tick) {
		size_t result = 0;
		size_t irbp = 0;

		for (auto &b : BPMs)
		{
			if (b.offset < tick)
			{
				result = irbp;
			}
			else {
				break;
			}
			irbp++;
		}

		return result;
	};

	auto noteRead = [&pureBPMToCalcBPM, &getNoteTime, &getRefBPM, &chartOffset](noteContainer &NTS, const BPMContainer &BPMs, parsedChart &chartMap, std::string difficulty)
	{
		std::vector<NoteInt> ntsI;
		for (auto &scopeData : chartMap[difficulty])
		{
			char c[32] = { 0 };
			int i = 0, j = 0;
			for (auto &inst : scopeData.second)
			{
				if (sscanf(inst.c_str(), "%31s %d %d", c, &i, &j) == 3)
				{
					if (std::string(c) == "N")
					{
						NoteInt nt;
						nt.time = std::stoll(scopeData.first);
						nt.lTime = j;
						nt.type = i;

						ntsI.push_back(nt);
					}

					if (std::string(c) == "S" && i == 2)
					{
						NoteInt nt;
						nt.time = std::stoll(scopeData.first);
						nt.lTime = j;
						nt.type = -1;

						ntsI.push_back(nt);
					}
				}
			}
		}

		std::sort(ntsI.begin(), ntsI.end());

		uint64_t loffset = ~((uint64_t)0uLL);
		double lnotet = -1.0;
		double llngnotet = -1.0;


		size_t BPM = 0;
		for (auto &nt : ntsI)
		{
			NTS.push_back({});

			auto &nt0 = NTS.back();
			nt0 = nt;

			uint64_t loffsettmp = nt.time;
			if (nt0.type == -1)
			{
				nt0.time = getNoteTime(BPMs, getRefBPM(BPMs, nt.time), nt.time);
				nt0.lTime = getNoteTime(BPMs, getRefBPM(BPMs, loffsettmp + nt.lTime), loffsettmp + nt.lTime) - nt0.time;

				nt0.time += chartOffset;
				continue;
			}

			if (BPM < (BPMs.size() - 1))
			{
				if (BPMs[BPM + 1].offset < nt.time)
					++BPM;
			}

			if (loffset == nt.time)
			{
				nt0.time = lnotet;
				nt0.lTime = llngnotet;
			}
			else
			{
				nt0.time = getNoteTime(BPMs, getRefBPM(BPMs, nt.time), nt.time);
				nt0.lTime = getNoteTime(BPMs, getRefBPM(BPMs, loffsettmp + nt.lTime), loffsettmp + nt.lTime) - nt0.time;

				nt0.time += chartOffset;
			}

			lnotet = nt0.time;
			llngnotet = nt0.lTime;
			loffset = loffsettmp;
		}
	};

	auto fillChartInfo = [&](parsedChart &chartMap)
	{
		auto chk = [](std::deque<std::string> &s, size_t i)
		{
			std::string result;

			if (i < s.size())
				result = s[i];

			return result;
		};

		auto &Song = chartMap["[Song]"];

		chartResolutionProp = std::stod(Song["Resolution"][0]) / 192.0;
		songName = chk(Song["Name"], 0u);
		songArtist = chk(Song["Artist"], 0u);
		songCharter = chk(Song["Charter"], 0u);
		try {
			chartOffset = std::stod(Song["Offset"][0]);
		}
		catch (...)
		{
			chartOffset = 0;
		}

		std::cout << chartResolutionProp << std::endl;
	};


	parseFeedBackChart(feedBackChartMap);

	fillChartInfo(feedBackChartMap);

	BPMRead(BPMs, feedBackChartMap);
	noteRead(Nts, BPMs, feedBackChartMap, "[ExpertSingle]"); // Default: "[ExpertSingle]"

															 /*
															 int bpdqpos = 0;
															 for (auto &bpdq : BPMs)
															 {
															 std::cout << "BPM " << bpdq.BPM / 1000.0 << "  offset " << bpdq.offset << "   " << getNoteTime(BPMs, bpdqpos, bpdq.offset) << std::endl;
															 bpdqpos++;
															 }
															 */
	int p = 0;
	for (auto &BP : BPMs) {
		Note newNote;
		newNote.time = getNoteTime(BPMs, p, (int64_t)BP.offset);
		newNote.lTime = BP.BPM / 1000.0;

		if (newNote.time != 0.0)
		{
			newNote.time += chartOffset;
		}

		BPM.push_back(newNote);

		++p;
	}

	auto roundTo = [](double num, double dec)
	{
		double v = pow(10.0, dec);
		num *= v;
		num = floor(num);
		num /= v;
		return num;
	};

	auto posProcess = [this](noteContainer &Nts, instrumentNotes &in)
	{
		auto &gNotes = in.gNotes;
		auto &gPlus = in.gPlus;

		auto hopoTest = [this, &gNotes](Note &note)
		{
			size_t size = gNotes.size(), i;
			i = size - 1;
			if (size > 0) {
				size_t BPMsize = BPM.size(), BPMi;
				BPMi = BPMsize - 1;

				double BPMStepCalc = 60.0 / 120.0;

				if (BPMsize > 0) {
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

				int type1 = note.type & CPlayer::notesEnumWithOpenNotes, type2 = gNotes[i].type & CPlayer::notesEnumWithOpenNotes;

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

		for (size_t i = 0, size = Nts.size(); i < size; i++) {
			if (isnan(Nts[i].time) || isnan(Nts[i].lTime)) {
				continue;
			}

			if (Nts[i].type >= 0 && Nts[i].type < 5 && Nts[i].time > 0.0)
			{
				if (gNotes.size() == 0) {
					Note newNote;
					newNote.time = Nts[i].time;
					//newNote.unmodifiedTime = Nts[i].time;
					newNote.lTime = Nts[i].lTime;
					newNote.type = (int)pow(2, Nts[i].type);

					if (newNote.lTime > 0.0) newNote.type |= nf_slide;

					hopoTest(newNote);

					gNotes.push_back(newNote);
				}
				else {
					if (gNotes[gNotes.size() - 1].time == Nts[i].time) {
						gNotes[gNotes.size() - 1].type |= (int)pow(2, Nts[i].type);
						gNotes[gNotes.size() - 1].type |= nf_not_hopo;
					}
					else {
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

		in.deducePlusLastNotes();
	};

	return true;
}

bool CChart::open(const std::string &chartFile)
{
	std::ifstream chart(chartFile);
	return parseFeebackChart(chart);
}

bool CChart::openFromMemory(const char *chart)
{
	std::stringstream schart(chart);
	return parseFeebackChart(schart);
}

void CChart::fillPlayerData(CPlayer & player, const std::string & instrument)
{

}

CChart::CChart()
{
	chartResolutionProp = 1.0;
	chartOffset = 0.0;

}

CChart::instrumentNotes::instrumentNotes()
{
	present = false;
}

void CChart::instrumentNotes::deducePlusLastNotes()
{
	size_t plusPosTemp = 0;

	for (size_t i = 0, size = gNotes.size(); i < size; ++i)
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
