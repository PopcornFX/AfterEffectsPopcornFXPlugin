//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#pragma once

#ifndef __POPCORNFX_UID_H__
#define __POPCORNFX_UID_H__

#include "PopcornFX_Define.h"

#include <vector>
#include <iostream>
#include <sstream>
#include <random>
#include <climits>
#include <algorithm>
#include <functional>
#include <string>

__AAEPK_BEGIN

//----------------------------------------------------------------------------

class	CUUIDGenerator
{
private:
	CUUIDGenerator() {}

	static unsigned char	_RandomChar()
	{
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_int_distribution<> dis(0, 255);
		return static_cast<unsigned char>(dis(gen));
	}

	static std::string		_GenerateHex(const unsigned int len)
	{
		std::stringstream ss;

		for (unsigned int i = 0; i < len; i++)
		{
			unsigned char		rc = _RandomChar();
			std::stringstream	hexstream;

			hexstream << std::hex << int(rc);

			std::string			hex = hexstream.str();

			ss << (hex.length() < 2 ? '0' + hex : hex);
		}
		return ss.str();
	}
public:
	~CUUIDGenerator() {};

	static std::string		Get32() { return _GenerateHex(32); }
	static std::string		Get16() { return _GenerateHex(16); }
	static std::string		Get8() { return _GenerateHex(8); }

};

//----------------------------------------------------------------------------

__AAEPK_END

#endif
