#pragma once
#ifndef _GUITAR_PP_CCONTROLS_h_
#define _GUITAR_PP_CCONTROLS_h_

class CControls{
	CControls(CControls&) = delete;

public:


	static CControls &controls();

	CControls();
};

#endif