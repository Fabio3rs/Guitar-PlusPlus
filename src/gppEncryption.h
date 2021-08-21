#pragma once
#ifndef _GPPENCRYPTION_h_
#define _GPPENCRYPTION_h_

#include <cstdint>
#include <algorithm>

void gppDecrypt(unsigned char *ch, size_t s)
{
	size_t sdz = s / 2;
	size_t s1 = s;

	if (s1 > 0)
		s1 -= 1;

	for (size_t i = 0; i < sdz; i++)
	{
		size_t r = s1 - i;
		if (r != i)
		{
			ch[i] ^= 0x77;
			ch[r] ^= 0x77;
			std::swap(ch[i], ch[r]);
		}
	}
}

void gppEncrypt(unsigned char *ch, size_t s)
{
	gppDecrypt(ch, s);
}

#endif