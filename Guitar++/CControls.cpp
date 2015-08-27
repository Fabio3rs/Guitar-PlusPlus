#include "CControls.h"
#include "CEngine.h"
#include <GLFW/glfw3.h>

CControls &CControls::controls()
{
	static CControls controls;
	return controls;
}


CControls::CControls()
{



}
