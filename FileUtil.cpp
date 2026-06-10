#include "stdafx.h"
#include "FileUtil.h"

CStringW CFileUtil::ToWidePath(const CString& path)
{
#ifdef _UNICODE
	return CStringW(path);
#else
	return CStringW(path);
#endif
}

CString CFileUtil::ToDisplayPath(const CStringW& path)
{
#ifdef _UNICODE
	return CString(path);
#else
	return CString(CStringA(path));
#endif
}

CStringW CFileUtil::CombinePath(const CStringW& folder, const CStringW& fileName)
{
	CStringW result(folder);
	if (!result.IsEmpty())
	{
		wchar_t last = result.GetAt(result.GetLength() - 1);
		if (last != L'\\' && last != L'/')
			result += L"\\";
	}
	result += fileName;
	return result;
}

CStringW CFileUtil::GetFileNameNoExt(const CStringW& path)
{
	int slash1 = path.ReverseFind(L'\\');
	int slash2 = path.ReverseFind(L'/');
	int slash = slash1 > slash2 ? slash1 : slash2;
	int start = slash + 1;
	CStringW name = path.Mid(start);
	int dot = name.ReverseFind(L'.');
	if (dot > 0)
		name = name.Left(dot);
	return name;
}

CStringW CFileUtil::GetDirectoryName(const CStringW& path)
{
	int slash1 = path.ReverseFind(L'\\');
	int slash2 = path.ReverseFind(L'/');
	int slash = slash1 > slash2 ? slash1 : slash2;
	if (slash < 0)
		return L"";
	return path.Left(slash);
}

CStringW CFileUtil::GetExtensionLower(const CStringW& path)
{
	int slash1 = path.ReverseFind(L'\\');
	int slash2 = path.ReverseFind(L'/');
	int slash = slash1 > slash2 ? slash1 : slash2;
	int dot = path.ReverseFind(L'.');
	if (dot < 0 || dot < slash)
		return L"";
	CStringW ext = path.Mid(dot);
	ext.MakeLower();
	return ext;
}

BOOL CFileUtil::EnsureDirectory(const CStringW& folder)
{
	if (folder.IsEmpty())
		return TRUE;

	DWORD attr = ::GetFileAttributesW(folder);
	if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY))
		return TRUE;

	CStringW parent = GetDirectoryName(folder);
	if (!parent.IsEmpty() && parent != folder)
		EnsureDirectory(parent);

	return ::CreateDirectoryW(folder, NULL) || ::GetLastError() == ERROR_ALREADY_EXISTS;
}

BOOL CFileUtil::ReadAllBytes(const CStringW& path, std::vector<unsigned char>& bytes, CString& error)
{
	bytes.clear();
	HANDLE hFile = ::CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		error.Format(_T("파일 열기 실패: %lu"), ::GetLastError());
		return FALSE;
	}

	LARGE_INTEGER size;
	if (!::GetFileSizeEx(hFile, &size) || size.QuadPart < 0 || size.QuadPart > 0x7fffffff)
	{
		error = _T("파일 크기가 너무 크거나 알 수 없습니다.");
		::CloseHandle(hFile);
		return FALSE;
	}

	bytes.resize((size_t)size.QuadPart);
	DWORD readBytes = 0;
	BOOL ok = TRUE;
	if (!bytes.empty())
		ok = ::ReadFile(hFile, &bytes[0], (DWORD)bytes.size(), &readBytes, NULL);
	::CloseHandle(hFile);

	if (!ok || readBytes != bytes.size())
	{
		error.Format(_T("파일 읽기 실패: %lu"), ::GetLastError());
		bytes.clear();
		return FALSE;
	}
	return TRUE;
}

BOOL CFileUtil::WriteAllBytes(const CStringW& path, const std::vector<unsigned char>& bytes, CString& error)
{
	CStringW folder = GetDirectoryName(path);
	if (!EnsureDirectory(folder))
	{
		error = _T("출력 폴더를 만들 수 없습니다.");
		return FALSE;
	}

	HANDLE hFile = ::CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		error.Format(_T("파일 생성 실패: %lu"), ::GetLastError());
		return FALSE;
	}

	DWORD written = 0;
	BOOL ok = TRUE;
	if (!bytes.empty())
		ok = ::WriteFile(hFile, &bytes[0], (DWORD)bytes.size(), &written, NULL);
	::CloseHandle(hFile);

	if (!ok || written != bytes.size())
	{
		error.Format(_T("파일 쓰기 실패: %lu"), ::GetLastError());
		return FALSE;
	}
	return TRUE;
}

BOOL CFileUtil::WriteTextFile(const CStringW& path, const std::string& text, CString& error)
{
	std::vector<unsigned char> bytes(text.begin(), text.end());
	return WriteAllBytes(path, bytes, error);
}

void CFileUtil::FindDcmFilesRecursive(const CStringW& folder, std::vector<CStringW>& files)
{
	CStringW pattern = CombinePath(folder, L"*");
	WIN32_FIND_DATAW fd;
	HANDLE hFind = ::FindFirstFileW(pattern, &fd);
	if (hFind == INVALID_HANDLE_VALUE)
		return;

	do
	{
		if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
			continue;

		CStringW child = CombinePath(folder, fd.cFileName);
		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			FindDcmFilesRecursive(child, files);
		}
		else if (GetExtensionLower(child) == L".dcm")
		{
			files.push_back(child);
		}
	} while (::FindNextFileW(hFind, &fd));

	::FindClose(hFind);
}
