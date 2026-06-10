#pragma once

#include <vector>
#include <string>

class CBase64Decoder
{
public:
	static BOOL Decode(const std::string& input, std::vector<unsigned char>& output, CString& error);

private:
	static int DecodeChar(unsigned char ch);
	static BOOL IsIgnored(unsigned char ch);
};
