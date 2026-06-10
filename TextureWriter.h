#pragma once

#include <vector>
#include "HpsMeshDocument.h"

class CTextureWriter
{
public:
	static BOOL WriteTexture(const CStringW& outputFolder, const CStringW& baseName, const HpsTextureData& texture, CStringW& savedPath, CString& error);
	static BOOL WriteBmpFromRgb(const CStringW& path, const std::vector<unsigned char>& rgb, unsigned int width, unsigned int height, unsigned int bytesPerPixel, CString& error);
};
