#pragma once
#include <iostream>

#define NO_SCORE -1

// string�� ������ �ٲ��ֱ�
// ��¥�� �����ϱ� �׳� ���� �ƴ� ������ ����
int StringToInt(std::string str);
std::string BytesToString(const char* data, int len);
void PrintBytes(const char* data, int len);