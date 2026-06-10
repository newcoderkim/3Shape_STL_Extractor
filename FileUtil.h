#pragma once

#include <vector>
#include <string>

class CFileUtil
{
public:
	static CStringW ToWidePath(const CString& path);
	static CString ToDisplayPath(const CStringW& path);
	static CStringW CombinePath(const CStringW& folder, const CStringW& fileName);
	static CStringW GetFileNameNoExt(const CStringW& path);
	static CStringW GetDirectoryName(const CStringW& path);
	static CStringW GetExtensionLower(const CStringW& path);
	static BOOL EnsureDirectory(const CStringW& folder);
	static BOOL ReadAllBytes(const CStringW& path, std::vector<unsigned char>& bytes, CString& error);
	static BOOL WriteAllBytes(const CStringW& path, const std::vector<unsigned char>& bytes, CString& error);
	static BOOL WriteTextFile(const CStringW& path, const std::string& text, CString& error);
	static void FindDcmFilesRecursive(const CStringW& folder, std::vector<CStringW>& files);
};
