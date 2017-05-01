#pragma once
#ifndef _GPPENCRYPTION_h_
#define _GPPENCRYPTION_h_

#include <cstdint>
#include <algorithm>

void gppDecrypt(unsigned char *ch, size_t s)
{
	size_t sdz = s / 2;

	if (sdz > 0)
		sdz -= 1;

	for (size_t i = 0; i < sdz; i++)
	{
		std::swap(ch[i], ch[sdz - i]);
		ch[i] ^= 0x77;
	}
}

void gppEncrypt(unsigned char *ch, size_t s)
{
	gppDecrypt(ch, s);
}

#endif