#pragma once
#include <iostream>

#define NO_SCORE -1

// string을 점수로 바꿔주기
// 어짜피 점수니까 그냥 음이 아닌 정수로 가정
int StringToInt(std::string str);
std::string BytesToString(const char* data, int len);
void PrintBytes(const char* data, int len);