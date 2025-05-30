#include "function.h"

int StringToInt(std::string str)
{
	if (str.empty())
		return NO_SCORE;
	int result = 0;
	int mul = 1;
	for (int i = str.size() - 1; i >= 0; i--)
	{
		result += mul * (str[i] - '0');
		mul *= 10;
	}
	return result;
}

std::string BytesToString(const char* data, int len)
{
    std::string result = "";
    for (int i = 0; i < len; i += 16)
    {
        for (int j = 0; j < 16; ++j)
        {
            if (i + j < len)
            {
                char ch = data[i + j];
                if (isprint((unsigned char)ch))
                {
                    result += ch;
                }
                else
                {
                    std::cout << "머고 이건\n";
                    return "";
                }
            }
        }
    }
    return result;
}

void PrintBytes(const char* data, int len)
{
    for (int i = 0; i < len; i += 16) 
    {

        // ASCII 출력
        for (int j = 0; j < 16; ++j) 
        {
            if (i + j < len) 
            {
                char ch = data[i + j];
                if (isprint((unsigned char)ch))
                    printf("%c", ch);
                else
                    printf(".");
            }
        }
        printf("\n");
    }
    printf("\n");
}