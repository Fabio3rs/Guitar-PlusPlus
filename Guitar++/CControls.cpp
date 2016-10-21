#include "CControls.h"
#include "CEngine.h"
#include <GLFW/glfw3.h>

void CControls::update()
{
	auto &engine = CEngine::engine();

	double time = engine.getTime();

	for (int i = 0; i <= GLFW_KEY_LAST; i++)
	{
		auto &k = keys[i];

		if (k.pressed)
		{
			k.lastFramePressed = true;
		}
		else
		{
			k.lastFramePressed = false;
		}

		k.pressed = engine.getKey(i);

		if (k.pressed && !k.lastFramePressed)
		{
			k.t = time;
		}
	}
}

CControls &CControls::controls()
{
	static CControls controls;
	return controls;
}


CControls::CControls()
{



}


CControls::CPlayerControls::CPlayerControls()
{

}

