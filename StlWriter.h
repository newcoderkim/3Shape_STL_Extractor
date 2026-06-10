#pragma once

#include "MeshData.h"

class CStlWriter
{
public:
	static BOOL WriteBinaryStl(const CStringW& path, const CMeshData& mesh, unsigned int& skippedDegenerate, CString& error);
	static BOOL WriteAsciiStl(const CStringW& path, const CMeshData& mesh, unsigned int& skippedDegenerate, CString& error);
};
