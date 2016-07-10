#include "CCharter.h"
#include "CFonts.h"

void CCharter::prepareDemoGamePlay(CGamePlay &gp)
{
	workingNote = nullptr;

	for (auto &p : gpModule.players)
	{
		eraseNulls(p);
		p.Notes.deducePlusLastNotes();
	}

	for (auto np : gpModule.players)
	{
		np.enableBot = true;
		gp.players.push_back(np);
	}
}

void CCharter::eraseNulls(CPlayer &player)
{
	for (auto it = player.Notes.gNotes.begin(); it != player.Notes.gNotes.end(); /******/)
	{
		int ncount = 0;
		for (int ji = 0; ji < 5; ji++)
		{
			if ((*it).type & CGamePlay::notesFlagsConst[ji])
			{
				++ncount;
			}
		}

		if (ncount > 1)
		{
			(*it).type |= nf_not_hopo;
		}

		if (((*it).type & CPlayer::notesEnum) != 0)
		{
			++it;
		}
		else
		{
			it = player.Notes.gNotes.erase(it);
		}
	}
}

void CCharter::preRender()
{
	bool kfor = CEngine::engine().getKey(GLFW_KEY_UP), kback = CEngine::engine().getKey(GLFW_KEY_DOWN);
	bool kleft = CEngine::engine().getKey(GLFW_KEY_LEFT), kright = CEngine::engine().getKey(GLFW_KEY_RIGHT);

	auto roundTo = [](double t, double d)
	{
		double d0 = pow(10.0, d);
		t *= d0;
		t = floor(t);
		return t / d0;
	};

	auto &processingP = gpModule.players.back();

	double BPM = 120.0 / 60.0;

	if (processingP.Notes.BPM.size() > 1)
	{
		BPM = 30.0 / gpModule.getBPMAt(processingP, atMusicTime);
	}

	BPM = roundTo(BPM, 2);

	double BPMDiv64 = BPM / 64.0;
	//std::cout << BPMDiv64 << "   " << BPM * 0.125 << std::endl;

	if (kleft || kright)
	{
		if (!bLeftRightK)
		{
			if (kright)
			{
				if (divTime < 4.0)
				{
					divTime *= 2.0;
				}
			}
			else
			{
				if (divTime > 0.125)
				{
					divTime /= 2.0;
				}
			}
			bLeftRightK = true;
		}
	}
	else
	{
		bLeftRightK = false;
	}

	if (kfor || kback)
	{
		if (!bForwardBackwardK)
		{
			if (kfor)
				atMusicTime += divTime * BPM;
			else{
				if (atMusicTime > 0.0) atMusicTime -= divTime * BPM;
				
				if (atMusicTime < 0.0)
					atMusicTime = 0.0;
			}
			bForwardBackwardK = true;
		}
	}
	else
	{
		bForwardBackwardK = false;
	}

	for (auto &p : gpModule.players)
	{
		p.musicRunningTime = atMusicTime;

		auto &notes = p.Notes;
		auto &gNotes = p.Notes.gNotes;
		auto &engine = CEngine::engine();

		while (true)
		{
			if (notes.plusPos > 0)
			{
				auto pp = notes.plusPos - 1;

				auto &plPos = notes.gPlus[notes.plusPos];

				if ((plPos.time + plPos.lTime) > (atMusicTime - 2.0))
				{
					notes.plusPos = pp;
				}
			}

			if (notes.notePos > 0)
			{
				auto np = notes.notePos - 1;

				auto &gnote = gNotes[np];

				if ((gnote.time + gnote.lTime) > (atMusicTime - 2.0))
				{
					notes.notePos = np;
				}
				else
				{
					break;
				}
			}
			else
			{
				break;
			}
		}
	}

	typedef std::deque<CPlayer::NotesData::Note> notesType;

	auto searchNoteAt = [roundTo, BPMDiv64](notesType &gNotes, double time, notesType::iterator &beforeTime, notesType::iterator &afterTime)
	{
		auto begin = gNotes.begin();
		auto it = gNotes.begin();
		auto lit = it;
		auto end = gNotes.begin();

		time = roundTo(time, 3);

		for (; it != gNotes.end(); it++)
		{
			auto &note = *it;

			if (note.time < time)
			{
				beforeTime = it;
			}

			if (note.time > time)
			{
				afterTime = it;
			}

			double deltaNotes = note.time - time;

			if (deltaNotes > (-BPMDiv64) && deltaNotes < BPMDiv64)
			{
				lit = it;
				lit++;
				afterTime = lit;
				break;
			}



			lit = it;
		}

		return it;
	};

	auto &gNotes = processingP.Notes.gNotes;

	notesType::iterator beforeTime = gNotes.begin(), afterTime = gNotes.end(), result;

	auto check = [&](notesType::iterator it, notesType::iterator end)
	{
		if (it != end)
		{
			std::cout << "  " << (*it).time << "  ";
		}
		else
		{
			std::cout << " it fail ";
		}
	};

	int nowkeys = processingP.getFretsPressedFlags();

	if (nowkeys == 0)
	{
		workingNote = nullptr;
		processNewNote = true;
	}
	
	if (workingNote && (workingNoteIt->type & CPlayer::notesEnum) != processingP.getLastFretsPressedFlags())
	{
		workingNote = nullptr;
	}

	bool addedNow = false;

	if (nowkeys != processingP.getLastFretsPressedFlags())
	{
		result = searchNoteAt(gNotes, atMusicTime, beforeTime, afterTime);
	}

	if (nowkeys > 0 && nowkeys != processingP.getLastFretsPressedFlags() && workingNote == nullptr && processNewNote)
	{
		if (result == gNotes.end())
		{
			CPlayer::NotesData::Note newNote;
			newNote.time = atMusicTime;
			newNote.lTime = 0.0;
			newNote.type = processingP.getFretsPressedFlags();

			workingNoteIt = gNotes.insert(beforeTime, newNote);
			result = workingNoteIt;
			workingNote = &(*workingNoteIt);
			addedNow = true;
		}
		else
		{
			workingNote = &(*result);
			workingNoteIt = result;
		}
	}
	else
	{
		if (workingNote)
		{
			double deltaT = atMusicTime - (workingNoteIt->time);
			if (deltaT > 0.05)
			{
				workingNoteIt->lTime = atMusicTime - (workingNoteIt->time);
				workingNoteIt->type |= nf_slide;
				processNewNote = false;
			}
			else if (deltaT < 0.0)
			{
				workingNote = nullptr;
			}
			else if ((workingNoteIt->type & CPlayer::notesEnum) == processingP.getLastFretsPressedFlags())
			{
				workingNoteIt->type &= ~(nf_slide);
				workingNoteIt->lTime = 0;
			}
		}
	}

	for (int i = 0; i < 5; i++)
	{
		if (!addedNow)
		{
			if (!fretKeys[i] && processingP.fretsPressed[i])
			{
				result->type ^= CGamePlay::notesFlagsConst[i];
			}
		}
		fretKeys[i] = processingP.fretsPressed[i];
	}

	if (addedNow)
	{
		std::sort(gNotes.begin(), gNotes.end());
	}

	/*if (nowkeys != processingP.getLastFretsPressedFlags() && atMusicTime > 0.0 && keypts < nowkeys)
	{
		keypts = nowkeys;
		result = searchNoteAt(gNotes, atMusicTime, beforeTime, afterTime);

		CPlayer::NotesData::Note newNote;
		newNote.time = atMusicTime;
		newNote.lTime = 0.0;
		newNote.type = processingP.getFretsPressedFlags();
		newNote.type |= nf_not_hopo;

		if (result == gNotes.end())
		{
			gNotes.insert(beforeTime, newNote);
		}
		else
		{
			if (processingP.getFretsPressedFlags() != 0) result->type ^= processingP.getFretsPressedFlags();
		}

		std::sort(gNotes.begin(), gNotes.end());*/
		/*check(result, gNotes.end());
		check(beforeTime, gNotes.end());
		check(afterTime, gNotes.end());*/
		/*std::cout << std::endl;
	}
	else
	{
		keypts = 0;
	}
	*/

	gpModule.update();

}

void CCharter::render()
{
	gpModule.render();
}

void CCharter::renderInfo()
{
	CEngine::engine().setSoundTime(songAudioID, atMusicTime);
	CFonts::fonts().drawTextInScreen("div x " + std::to_string(divTime), 0.5, 0.3, 0.1);
	CFonts::fonts().drawTextInScreen("BPM " + std::to_string(gpModule.getBPMAt(gpModule.players.back(), atMusicTime)), -1.4, 0.3, 0.1);
	CFonts::fonts().drawTextInScreen("Song BPM " + std::to_string(CEngine::engine().getSoundBPM(songAudioID)), -1.4, 0.15, 0.1);


}

void CCharter::renderAll()
{
	preRender();
	render();
	renderInfo();
}

CCharter::CCharter()
{
	workingNote = nullptr;
	bForwardBackwardK = false;
	bLeftRightK = false;
	processNewNote = true;
	atMusicTime = 0.0;
	gpModule.players.push_back(CPlayer("guitar charter"));
	gpModule.bIsACharterGP = true;
	divTime = 0.5;

	std::string song = GPPGame::GuitarPP().songToLoad;
	//module.players[0].loadSongOnlyChart(song);
	//module.players[1].loadSong(song);
	gpModule.players.back().loadSong(song);
	std::cout << "audio id " << CEngine::engine().loadSoundStream(gpModule.players.back().Notes.songFullPath.c_str(), songAudioID, true) << std::endl;
	gpModule.bRenderHUD = false;
	gpModule.showBPMVlaues = true;

	for (auto &b : fretKeys)
	{
		b = false;
	}
	keypts = 0;
}

