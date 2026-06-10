#include "stdafx.h"
#include "TextureWriter.h"
#include "FileUtil.h"

static void AppendRaw(std::vector<unsigned char>& out, const void* data, size_t size)
{
	const unsigned char* p = (const unsigned char*)data;
	out.insert(out.end(), p, p + size);
}

BOOL CTextureWriter::WriteTexture(const CStringW& outputFolder, const CStringW& baseName, const HpsTextureData& texture, CStringW& savedPath, CString& error)
{
	savedPath.Empty();
	if (texture.bytes.empty())
	{
		error = _T("TextureImage 데이터가 없습니다.");
		return FALSE;
	}

	if (texture.bytes.size() >= 3 &&
		texture.bytes[0] == 0xFF && texture.bytes[1] == 0xD8 && texture.bytes[2] == 0xFF)
	{
		savedPath = CFileUtil::CombinePath(outputFolder, baseName + L"_texture.jpg");
		return CFileUtil::WriteAllBytes(savedPath, texture.bytes, error);
	}

	if (texture.bytes.size() >= 4 &&
		texture.bytes[0] == 0x89 && texture.bytes[1] == 0x50 &&
		texture.bytes[2] == 0x4E && texture.bytes[3] == 0x47)
	{
		savedPath = CFileUtil::CombinePath(outputFolder, baseName + L"_texture.png");
		return CFileUtil::WriteAllBytes(savedPath, texture.bytes, error);
	}

	savedPath = CFileUtil::CombinePath(outputFolder, baseName + L"_texture.bmp");
	return WriteBmpFromRgb(savedPath, texture.bytes, texture.width, texture.height, texture.bytesPerPixel, error);
}

BOOL CTextureWriter::WriteBmpFromRgb(const CStringW& path, const std::vector<unsigned char>& rgb, unsigned int width, unsigned int height, unsigned int bytesPerPixel, CString& error)
{
	if (width == 0 || height == 0 || (bytesPerPixel != 3 && bytesPerPixel != 4))
	{
		error = _T("BMP 저장 실패: Width/Height/BytesPerPixel 값이 올바르지 않습니다.");
		return FALSE;
	}

	size_t expected = (size_t)width * height * bytesPerPixel;
	if (rgb.size() < expected)
	{
		error.Format(_T("BMP 저장 실패: RGB 데이터 부족 expected=%u actual=%u"), (unsigned int)expected, (unsigned int)rgb.size());
		return FALSE;
	}

	unsigned int outRowBytes = width * 3;
	unsigned int outStride = (outRowBytes + 3) & ~3U;
	unsigned int imageBytes = outStride * height;
	unsigned int fileSize = 14 + 40 + imageBytes;
	unsigned int pixelOffset = 14 + 40;

	std::vector<unsigned char> out;
	out.reserve(fileSize);

	unsigned short bfType = 0x4D42;
	unsigned short reserved = 0;
	unsigned int dibSize = 40;
	int bmpWidth = (int)width;
	int bmpHeight = (int)height;
	unsigned short planes = 1;
	unsigned short bitCount = 24;
	unsigned int compression = 0;
	unsigned int ppm = 2835;
	unsigned int colors = 0;

	AppendRaw(out, &bfType, sizeof(bfType));
	AppendRaw(out, &fileSize, sizeof(fileSize));
	AppendRaw(out, &reserved, sizeof(reserved));
	AppendRaw(out, &reserved, sizeof(reserved));
	AppendRaw(out, &pixelOffset, sizeof(pixelOffset));

	AppendRaw(out, &dibSize, sizeof(dibSize));
	AppendRaw(out, &bmpWidth, sizeof(bmpWidth));
	AppendRaw(out, &bmpHeight, sizeof(bmpHeight));
	AppendRaw(out, &planes, sizeof(planes));
	AppendRaw(out, &bitCount, sizeof(bitCount));
	AppendRaw(out, &compression, sizeof(compression));
	AppendRaw(out, &imageBytes, sizeof(imageBytes));
	AppendRaw(out, &ppm, sizeof(ppm));
	AppendRaw(out, &ppm, sizeof(ppm));
	AppendRaw(out, &colors, sizeof(colors));
	AppendRaw(out, &colors, sizeof(colors));

	std::vector<unsigned char> padding(outStride - outRowBytes, 0);
	for (int y = (int)height - 1; y >= 0; --y)
	{
		const unsigned char* src = &rgb[(size_t)y * width * bytesPerPixel];
		for (unsigned int x = 0; x < width; ++x)
		{
			const unsigned char* px = src + (size_t)x * bytesPerPixel;
			out.push_back(px[2]);
			out.push_back(px[1]);
			out.push_back(px[0]);
		}
		if (!padding.empty())
			out.insert(out.end(), padding.begin(), padding.end());
	}

	return CFileUtil::WriteAllBytes(path, out, error);
}
