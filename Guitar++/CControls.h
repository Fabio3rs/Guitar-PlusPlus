#pragma once
#ifndef _GUITAR_PP_CCONTROLS_h_
#define _GUITAR_PP_CCONTROLS_h_

#include <vector>
#include <deque>
#include <map>

class CControls{
	CControls(CControls&) = delete;

public:

	struct key{
		double t;
		int pressed, lastFramePressed;
		double data;

		inline key()
		{
			pressed = lastFramePressed = false;
			t = 0.0;
			data = 0.0;
		}
	};

	std::map < int, key > keys;


	static CControls &controls();


	void update();
	CControls();

	class CPlayerControls{

	public:
		std::map < int, int > keys;

		CPlayerControls();
	};
};

#endif