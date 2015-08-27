#pragma once
#ifndef _GUITAR_PP_CCONTROLS_h_
#define _GUITAR_PP_CCONTROLS_h_

#include <vector>
#include <deque>
#include <map>
#include <unordered_map>

class CControls{
	CControls(CControls&) = delete;

public:

	struct key{
		double t;
		bool pressed, lastFramePressed;

		inline key()
		{
			pressed = lastFramePressed = false;
			t = 0.0;
		}
	};

	std::unordered_map < int, key > keys;


	static CControls &controls();


	void update();
	CControls();

	class CPlayerControls{

	public:
		CPlayerControls();
	};
};

#endif