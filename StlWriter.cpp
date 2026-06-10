#include "stdafx.h"
#include "StlWriter.h"
#include "FileUtil.h"
#include <sstream>

static void AppendBytes(std::vector<unsigned char>& out, const void* data, size_t size)
{
	const unsigned char* p = (const unsigned char*)data;
	out.insert(out.end(), p, p + size);
}

BOOL CStlWriter::WriteBinaryStl(const CStringW& path, const CMeshData& mesh, unsigned int& skippedDegenerate, CString& error)
{
	skippedDegenerate = 0;
	unsigned int triangleCount = mesh.CountWritableTriangles(&skippedDegenerate);
	if (triangleCount == 0)
	{
		error = _T("저장 가능한 triangle이 없습니다.");
		return FALSE;
	}

	const std::vector<HpsVertex>& vertices = mesh.GetVertices();
	const std::vector<HpsFace>& faces = mesh.GetFaces();
	std::vector<unsigned char> bytes;
	bytes.reserve(84 + (size_t)triangleCount * 50);

	char header[80];
	memset(header, 0, sizeof(header));
	strcpy_s(header, sizeof(header), "3Shape HPS Mesh extracted by STL_Extractor");
	AppendBytes(bytes, header, sizeof(header));
	AppendBytes(bytes, &triangleCount, sizeof(triangleCount));

	for (size_t i = 0; i < faces.size(); ++i)
	{
		HpsNormal n;
		if (!mesh.ComputeNormal(faces[i], n))
			continue;

		const HpsVertex& a = vertices[faces[i].i1];
		const HpsVertex& b = vertices[faces[i].i2];
		const HpsVertex& c = vertices[faces[i].i3];
		unsigned short attr = 0;

		AppendBytes(bytes, &n.x, sizeof(float));
		AppendBytes(bytes, &n.y, sizeof(float));
		AppendBytes(bytes, &n.z, sizeof(float));
		AppendBytes(bytes, &a.x, sizeof(float));
		AppendBytes(bytes, &a.y, sizeof(float));
		AppendBytes(bytes, &a.z, sizeof(float));
		AppendBytes(bytes, &b.x, sizeof(float));
		AppendBytes(bytes, &b.y, sizeof(float));
		AppendBytes(bytes, &b.z, sizeof(float));
		AppendBytes(bytes, &c.x, sizeof(float));
		AppendBytes(bytes, &c.y, sizeof(float));
		AppendBytes(bytes, &c.z, sizeof(float));
		AppendBytes(bytes, &attr, sizeof(attr));
	}

	return CFileUtil::WriteAllBytes(path, bytes, error);
}

BOOL CStlWriter::WriteAsciiStl(const CStringW& path, const CMeshData& mesh, unsigned int& skippedDegenerate, CString& error)
{
	skippedDegenerate = 0;
	const std::vector<HpsVertex>& vertices = mesh.GetVertices();
	const std::vector<HpsFace>& faces = mesh.GetFaces();

	std::ostringstream ss;
	ss.setf(std::ios::fixed);
	ss.precision(7);
	ss << "solid 3shape_hps_mesh\n";

	unsigned int written = 0;
	for (size_t i = 0; i < faces.size(); ++i)
	{
		HpsNormal n;
		if (!mesh.ComputeNormal(faces[i], n))
		{
			++skippedDegenerate;
			continue;
		}

		const HpsVertex& a = vertices[faces[i].i1];
		const HpsVertex& b = vertices[faces[i].i2];
		const HpsVertex& c = vertices[faces[i].i3];
		ss << "  facet normal " << n.x << " " << n.y << " " << n.z << "\n";
		ss << "    outer loop\n";
		ss << "      vertex " << a.x << " " << a.y << " " << a.z << "\n";
		ss << "      vertex " << b.x << " " << b.y << " " << b.z << "\n";
		ss << "      vertex " << c.x << " " << c.y << " " << c.z << "\n";
		ss << "    endloop\n";
		ss << "  endfacet\n";
		++written;
	}

	ss << "endsolid 3shape_hps_mesh\n";

	if (written == 0)
	{
		error = _T("저장 가능한 triangle이 없습니다.");
		return FALSE;
	}

	return CFileUtil::WriteTextFile(path, ss.str(), error);
}
