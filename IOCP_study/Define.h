#pragma once
#define NO_SCORE -1

enum DataStatus : int
{
	Start,
	Swap,
	Destroy,
	Generate,
	Hide,
	Result,

	Finish,
	RivalConnectionClosed,
	ExitMatch,
};

enum Result :int
{
	Win,
	Lose,
	Draw,
};