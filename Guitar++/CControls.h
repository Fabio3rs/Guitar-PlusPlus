#pragma once
#ifndef _GUITAR_PP_CCONTROLS_h_
#define _GUITAR_PP_CCONTROLS_h_

#include <vector>
#include <deque>
#include <map>
#include <string>
#include <functional>

class CControls{
	CControls(CControls&) = delete;

public:
	static CControls &controls();

	unsigned int lastChar;
	std::function<void(unsigned int)> textCallback;

	void update();
	CControls();

	struct joystick
	{
		std::string identification;
		int numKeys;
		int numAnalogs;
	};

	struct keyState{
		double t;
		double data;
		bool pressed, lastFramePressed;

		inline keyState()
		{
			t = 0.0;
			data = 0.0;
			pressed = lastFramePressed = false;
		}
	};

	struct key
	{
		int device;
		int keyID;
	};

	std::map < int, keyState > keys;

	bool isKeyPressed(const key &k);
	double getAnalogKeyState(const key &k);

	class CPlayerControls{

	public:
		std::vector < int > keys;

		template<class Archive>
		void load(Archive & archive)
		{
			archive(keys);
		}

		template<class Archive>
		void save(Archive & archive) const
		{
			archive(keys);
		}

		CPlayerControls();
	};

	struct shortcutKeys
	{
		std::vector < key > keys;

		bool isPressed();
	};
};

#endif