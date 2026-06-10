#include "stdafx.h"
#include "Base64Decoder.h"

int CBase64Decoder::DecodeChar(unsigned char ch)
{
	if (ch >= 'A' && ch <= 'Z') return ch - 'A';
	if (ch >= 'a' && ch <= 'z') return ch - 'a' + 26;
	if (ch >= '0' && ch <= '9') return ch - '0' + 52;
	if (ch == '+') return 62;
	if (ch == '/') return 63;
	return -1;
}

BOOL CBase64Decoder::IsIgnored(unsigned char ch)
{
	return ch == ' ' || ch == '\r' || ch == '\n' || ch == '\t';
}

BOOL CBase64Decoder::Decode(const std::string& input, std::vector<unsigned char>& output, CString& error)
{
	output.clear();
	int quartet[4];
	int q = 0;
	BOOL seenPadding = FALSE;

	for (size_t i = 0; i < input.size(); ++i)
	{
		unsigned char ch = (unsigned char)input[i];
		if (IsIgnored(ch))
			continue;

		if (ch == '=')
		{
			seenPadding = TRUE;
			quartet[q++] = -2;
		}
		else
		{
			if (seenPadding)
			{
				error = _T("Base64 padding 뒤에 데이터가 있습니다.");
				return FALSE;
			}
			int value = DecodeChar(ch);
			if (value < 0)
			{
				error.Format(_T("잘못된 Base64 문자: 0x%02X"), ch);
				return FALSE;
			}
			quartet[q++] = value;
		}

		if (q == 4)
		{
			if (quartet[0] < 0 || quartet[1] < 0)
			{
				error = _T("잘못된 Base64 padding 위치입니다.");
				return FALSE;
			}

			unsigned char b1 = (unsigned char)((quartet[0] << 2) | (quartet[1] >> 4));
			output.push_back(b1);

			if (quartet[2] != -2)
			{
				if (quartet[2] < 0)
				{
					error = _T("잘못된 Base64 3번째 값입니다.");
					return FALSE;
				}
				unsigned char b2 = (unsigned char)(((quartet[1] & 0x0f) << 4) | (quartet[2] >> 2));
				output.push_back(b2);
			}

			if (quartet[3] != -2)
			{
				if (quartet[2] == -2 || quartet[3] < 0)
				{
					error = _T("잘못된 Base64 4번째 값입니다.");
					return FALSE;
				}
				unsigned char b3 = (unsigned char)(((quartet[2] & 0x03) << 6) | quartet[3]);
				output.push_back(b3);
			}

			q = 0;
		}
	}

	if (q != 0)
	{
		error = _T("Base64 길이가 4의 배수가 아닙니다.");
		return FALSE;
	}
	return TRUE;
}
