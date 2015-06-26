/*
 * patternmatch.cpp
 *
 *  Created on: Oct 23, 2010
 *      Author: uli
 */

#include "WOscPatternMatch.h"
#include <iostream>

int main()
{
	typedef struct
	{
		const char* addr;
		const char* msg;
		bool expected;
	} TestCase;

	// "addr" is the address tree pattern (usually the container names) and
	// "msg" is the address pattern which is contained in the TXed messages
	// and matched against containers and methods.
	//
	//		addr, 		msg,		expected
	//
	TestCase testCases[] = {
			{"hello",	"hello*",	true},
			{"hello",	"hello*w",	false},
			{"a",		"b",		false},	// 12

			{"ab",		"a",		false},
			{"a",		"ab",		false},
			{"av",		"*",		true},	// 16
			{"aKc",		"*[a-z]c",	false},	// 24

#if WOSC_USE_ADDR_WILDCARDS
			{"hello*",	"hellow",	true},	// 0
			{"*",		"av",		true},
			{"hello*",	"hello",	true},
			{"hello*",	"hello*",	true},

			{"hello*w",	"hello",	false},	// 4
			{"hello*",	"hello*",	true},
			{"hello*w",	"hello*",	true},
			{"hello*",	"hello*w",	true},	// 8

			{"hello*d",	"hello*w",	false},
			{"hello?w",	"hello*w",	true},
			{"hello?w",	"hellosw",	true},
			{"a*c",		"*{a,b}c",	true},	// 20

			{"a*c",		"*[ab]c",	true},
			{"a*c",		"*[!ab]c",	true},
			{"a*c",		"*[a-z]c",	true},
			{"?",		"*",		true},

			{"*",		"{a,b}c",	true},
			{"*k",		"{a,b}c",	false},
#endif
	};

	for (unsigned int i = 0; i < sizeof(testCases)/sizeof(TestCase); i++ ) {
		bool  res = WOscPatternMatch::PatternMatch(testCases[i].msg, testCases[i].addr);
		if ( res != testCases[i].expected ) {
			std::cerr<<"Pattern match test case "<<i<<" failed"<<std::endl;
			return -1;
		}
	}

	std::cout<<"Pattern match test passed"<<std::endl;

	return 0;
}
