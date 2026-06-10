#include "stdafx.h"
#include "MeshData.h"
#include <math.h>
#include <float.h>

CMeshData::CMeshData()
{
	Clear();
}

void CMeshData::Clear()
{
	m_vertices.clear();
	m_faces.clear();
	m_bounds.minX = m_bounds.minY = m_bounds.minZ = 0.0f;
	m_bounds.maxX = m_bounds.maxY = m_bounds.maxZ = 0.0f;
	m_bounds.valid = FALSE;
	m_vertexFormat = VertexFormatUnknown;
	m_facetFormat = FacetFormatUnknown;
}

BOOL CMeshData::DetectVertexFormat(const std::vector<unsigned char>& bytes, unsigned int vertexCount, std::vector<CString>& logs)
{
	m_vertices.clear();
	m_vertexFormat = VertexFormatUnknown;

	size_t floatBytes = (size_t)vertexCount * 3 * sizeof(float);
	size_t doubleBytes = (size_t)vertexCount * 3 * sizeof(double);

	if (vertexCount == 0)
	{
		logs.push_back(_T("Vertices 포맷 판정 실패: vertex_count가 0입니다."));
		return FALSE;
	}

	if (bytes.size() == floatBytes)
	{
		const float* p = (const float*)&bytes[0];
		m_vertices.reserve(vertexCount);
		for (unsigned int i = 0; i < vertexCount; ++i)
		{
			HpsVertex v;
			v.x = p[i * 3 + 0];
			v.y = p[i * 3 + 1];
			v.z = p[i * 3 + 2];
			m_vertices.push_back(v);
		}
		m_vertexFormat = VertexFormatFloat32;
		logs.push_back(_T("vertices 포맷 판정 결과: float x,y,z 배열"));
		return UpdateBoundingBox(logs);
	}

	if (bytes.size() == doubleBytes)
	{
		const double* p = (const double*)&bytes[0];
		m_vertices.reserve(vertexCount);
		for (unsigned int i = 0; i < vertexCount; ++i)
		{
			HpsVertex v;
			v.x = (float)p[i * 3 + 0];
			v.y = (float)p[i * 3 + 1];
			v.z = (float)p[i * 3 + 2];
			m_vertices.push_back(v);
		}
		m_vertexFormat = VertexFormatFloat64;
		logs.push_back(_T("vertices 포맷 판정 결과: double x,y,z 배열"));
		return UpdateBoundingBox(logs);
	}

	m_vertexFormat = VertexFormatCompressed;
	CString line;
	line.Format(_T("vertices 포맷 판정 실패: bytes=%u, float 예상=%u, double 예상=%u"),
		(unsigned int)bytes.size(), (unsigned int)floatBytes, (unsigned int)doubleBytes);
	logs.push_back(line);
	return FALSE;
}

BOOL CMeshData::DetectFacetFormat(const std::vector<unsigned char>& bytes, unsigned int facetCount, unsigned int vertexCount, std::vector<CString>& logs)
{
	m_faces.clear();
	m_facetFormat = FacetFormatUnknown;

	size_t u32Bytes = (size_t)facetCount * 3 * sizeof(unsigned int);
	size_t u16Bytes = (size_t)facetCount * 3 * sizeof(unsigned short);

	if (facetCount == 0)
	{
		logs.push_back(_T("Facets 포맷 판정 실패: facet_count가 0입니다."));
		return FALSE;
	}

	if (bytes.size() == u32Bytes)
	{
		const unsigned int* p = (const unsigned int*)&bytes[0];
		m_faces.reserve(facetCount);
		for (unsigned int i = 0; i < facetCount; ++i)
		{
			HpsFace f;
			f.i1 = p[i * 3 + 0];
			f.i2 = p[i * 3 + 1];
			f.i3 = p[i * 3 + 2];
			if (f.i1 >= vertexCount || f.i2 >= vertexCount || f.i3 >= vertexCount)
			{
				logs.push_back(_T("facets 포맷 판정 실패: uint32 인덱스가 vertex_count 범위를 벗어났습니다."));
				m_faces.clear();
				m_facetFormat = FacetFormatCompressed;
				return FALSE;
			}
			m_faces.push_back(f);
		}
		m_facetFormat = FacetFormatUInt32;
		logs.push_back(_T("facets 포맷 판정 결과: uint32 i1,i2,i3 배열"));
		return TRUE;
	}

	if (bytes.size() == u16Bytes)
	{
		const unsigned short* p = (const unsigned short*)&bytes[0];
		m_faces.reserve(facetCount);
		for (unsigned int i = 0; i < facetCount; ++i)
		{
			HpsFace f;
			f.i1 = p[i * 3 + 0];
			f.i2 = p[i * 3 + 1];
			f.i3 = p[i * 3 + 2];
			if (f.i1 >= vertexCount || f.i2 >= vertexCount || f.i3 >= vertexCount)
			{
				logs.push_back(_T("facets 포맷 판정 실패: uint16 인덱스가 vertex_count 범위를 벗어났습니다."));
				m_faces.clear();
				m_facetFormat = FacetFormatCompressed;
				return FALSE;
			}
			m_faces.push_back(f);
		}
		m_facetFormat = FacetFormatUInt16;
		logs.push_back(_T("facets 포맷 판정 결과: uint16 i1,i2,i3 배열"));
		return TRUE;
	}

	m_facetFormat = FacetFormatCompressed;
	CString line;
	line.Format(_T("facets 포맷 판정 실패: bytes=%u, uint32 예상=%u, uint16 예상=%u"),
		(unsigned int)bytes.size(), (unsigned int)u32Bytes, (unsigned int)u16Bytes);
	logs.push_back(line);
	return FALSE;
}

BOOL CMeshData::ComputeNormal(const HpsFace& face, HpsNormal& normal) const
{
	normal.x = normal.y = normal.z = 0.0f;
	if (face.i1 >= m_vertices.size() || face.i2 >= m_vertices.size() || face.i3 >= m_vertices.size())
		return FALSE;

	const HpsVertex& a = m_vertices[face.i1];
	const HpsVertex& b = m_vertices[face.i2];
	const HpsVertex& c = m_vertices[face.i3];

	float ux = b.x - a.x;
	float uy = b.y - a.y;
	float uz = b.z - a.z;
	float vx = c.x - a.x;
	float vy = c.y - a.y;
	float vz = c.z - a.z;

	normal.x = uy * vz - uz * vy;
	normal.y = uz * vx - ux * vz;
	normal.z = ux * vy - uy * vx;

	float len = (float)sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
	if (len <= 1.0e-12f || !IsFiniteFloat(len))
		return FALSE;

	normal.x /= len;
	normal.y /= len;
	normal.z /= len;
	return TRUE;
}

unsigned int CMeshData::CountWritableTriangles(unsigned int* skippedDegenerate) const
{
	unsigned int count = 0;
	unsigned int skipped = 0;
	for (size_t i = 0; i < m_faces.size(); ++i)
	{
		HpsNormal n;
		if (ComputeNormal(m_faces[i], n))
			++count;
		else
			++skipped;
	}
	if (skippedDegenerate)
		*skippedDegenerate = skipped;
	return count;
}

BOOL CMeshData::UpdateBoundingBox(std::vector<CString>& logs)
{
	if (m_vertices.empty())
		return FALSE;

	HpsBoundingBox box;
	box.minX = box.maxX = m_vertices[0].x;
	box.minY = box.maxY = m_vertices[0].y;
	box.minZ = box.maxZ = m_vertices[0].z;
	box.valid = TRUE;

	for (size_t i = 0; i < m_vertices.size(); ++i)
	{
		const HpsVertex& v = m_vertices[i];
		if (!IsFiniteFloat(v.x) || !IsFiniteFloat(v.y) || !IsFiniteFloat(v.z))
		{
			logs.push_back(_T("좌표값 검사 실패: NaN/INF가 포함되어 있습니다."));
			m_bounds.valid = FALSE;
			return FALSE;
		}
		if (v.x < box.minX) box.minX = v.x;
		if (v.y < box.minY) box.minY = v.y;
		if (v.z < box.minZ) box.minZ = v.z;
		if (v.x > box.maxX) box.maxX = v.x;
		if (v.y > box.maxY) box.maxY = v.y;
		if (v.z > box.maxZ) box.maxZ = v.z;
	}

	m_bounds = box;
	CString line;
	line.Format(_T("bounding box: min(%f, %f, %f), max(%f, %f, %f)"),
		box.minX, box.minY, box.minZ, box.maxX, box.maxY, box.maxZ);
	logs.push_back(line);

	float spanX = box.maxX - box.minX;
	float spanY = box.maxY - box.minY;
	float spanZ = box.maxZ - box.minZ;
	if (!IsReasonableRange(spanX) || !IsReasonableRange(spanY) || !IsReasonableRange(spanZ))
		logs.push_back(_T("좌표값 범위 경고: bounding box 범위가 비정상적으로 보입니다."));

	return TRUE;
}

BOOL CMeshData::IsFiniteFloat(float v) const
{
	return _finite(v) != 0;
}

BOOL CMeshData::IsReasonableRange(float span) const
{
	if (!IsFiniteFloat(span))
		return FALSE;
	if (span < 0.0f)
		return FALSE;
	if (span > 1000000.0f)
		return FALSE;
	return TRUE;
}
