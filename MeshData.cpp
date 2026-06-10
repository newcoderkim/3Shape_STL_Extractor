#include "stdafx.h"
#include "MeshData.h"
#include <math.h>
#include <float.h>

struct HpsEdge
{
	unsigned int start;
	unsigned int end;

	HpsEdge() : start(0), end(0) {}
	HpsEdge(unsigned int s, unsigned int e) : start(s), end(e) {}
};

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
		if (UpdateBoundingBox(logs))
		{
			logs.push_back(_T("vertices 포맷 판정 결과: float x,y,z 배열"));
			return TRUE;
		}
		m_vertices.clear();
		m_vertexFormat = VertexFormatCompressed;
		logs.push_back(_T("vertices 포맷 판정 실패: float 크기는 일치하지만 좌표값이 유효하지 않습니다."));
		return FALSE;
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
		if (UpdateBoundingBox(logs))
		{
			logs.push_back(_T("vertices 포맷 판정 결과: double x,y,z 배열"));
			return TRUE;
		}
		m_vertices.clear();
		m_vertexFormat = VertexFormatCompressed;
		logs.push_back(_T("vertices 포맷 판정 실패: double 크기는 일치하지만 좌표값이 유효하지 않습니다."));
		return FALSE;
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

	CString commandError;
	if (DecodeCcFacetCommands(bytes, facetCount, vertexCount, FALSE, commandError))
	{
		m_facetFormat = FacetFormatCcCommands16;
		logs.push_back(_T("facets 포맷 판정 결과: CC compressed face command stream (16-bit payload)"));
		return TRUE;
	}
	logs.push_back(_T("facets CC 16-bit command 해석 실패: ") + commandError);

	if (DecodeCcFacetCommands(bytes, facetCount, vertexCount, TRUE, commandError))
	{
		m_facetFormat = FacetFormatCcCommands32;
		logs.push_back(_T("facets 포맷 판정 결과: CC compressed face command stream (32-bit payload)"));
		return TRUE;
	}
	logs.push_back(_T("facets CC 32-bit command 해석 실패: ") + commandError);

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

BOOL CMeshData::DecodeCcFacetCommands(const std::vector<unsigned char>& bytes, unsigned int facetCount, unsigned int vertexCount, BOOL use32BitPayload, CString& error)
{
	m_faces.clear();
	if (bytes.empty())
	{
		error = _T("command stream이 비어 있습니다.");
		return FALSE;
	}

	std::vector<HpsEdge> edges;
	size_t pos = 0;
	unsigned int currentEdge = 0;
	unsigned int globalVertex = 0;
	unsigned int commands = 0;

	while (pos < bytes.size())
	{
		unsigned char commandByte = bytes[pos++];
		if ((commandByte >> 4) != 0)
		{
			error.Format(_T("offset %u: command 상위 4비트가 0이 아닙니다. byte=0x%02X"), (unsigned int)(pos - 1), commandByte);
			m_faces.clear();
			return FALSE;
		}

		unsigned int opcode = commandByte & 0x0F;
		++commands;

		if (opcode == 0)
		{
			if (edges.empty())
			{
				error.Format(_T("offset %u: VertexList 전에 edge list가 없습니다."), (unsigned int)(pos - 1));
				m_faces.clear();
				return FALSE;
			}
			if (globalVertex >= vertexCount)
			{
				error = _T("VertexList가 vertex_count 범위를 초과했습니다.");
				m_faces.clear();
				return FALSE;
			}
			HpsEdge edge = edges[currentEdge];
			unsigned int v = globalVertex++;
			HpsFace face;
			face.i1 = v;
			face.i2 = edge.end;
			face.i3 = edge.start;
			m_faces.push_back(face);
			edges.erase(edges.begin() + currentEdge);
			edges.insert(edges.begin() + currentEdge, HpsEdge(v, edge.end));
			edges.insert(edges.begin() + currentEdge, HpsEdge(edge.start, v));
			currentEdge = (currentEdge + 2) % edges.size();
		}
		else if (opcode == 1)
		{
			if (edges.size() < 2)
			{
				error = _T("Previous command에 필요한 edge가 부족합니다.");
				m_faces.clear();
				return FALSE;
			}
			unsigned int n = (unsigned int)edges.size();
			unsigned int prevIdx = (currentEdge + n - 1) % n;
			HpsEdge prevEdge = edges[prevIdx];
			HpsEdge currEdge = edges[currentEdge];
			HpsFace face;
			face.i1 = currEdge.start;
			face.i2 = prevEdge.start;
			face.i3 = currEdge.end;
			m_faces.push_back(face);

			unsigned int high = currentEdge > prevIdx ? currentEdge : prevIdx;
			unsigned int low = currentEdge > prevIdx ? prevIdx : currentEdge;
			edges.erase(edges.begin() + high);
			edges.erase(edges.begin() + low);
			edges.insert(edges.begin() + low, HpsEdge(prevEdge.start, currEdge.end));
			currentEdge = (low + 1) % edges.size();
		}
		else if (opcode == 2)
		{
			if (edges.size() < 2)
			{
				error = _T("Next command에 필요한 edge가 부족합니다.");
				m_faces.clear();
				return FALSE;
			}
			unsigned int n = (unsigned int)edges.size();
			unsigned int nextIdx = (currentEdge + 1) % n;
			HpsEdge currEdge = edges[currentEdge];
			HpsEdge nextEdge = edges[nextIdx];
			HpsFace face;
			face.i1 = currEdge.start;
			face.i2 = nextEdge.end;
			face.i3 = currEdge.end;
			m_faces.push_back(face);

			unsigned int high = nextIdx > currentEdge ? nextIdx : currentEdge;
			unsigned int low = nextIdx > currentEdge ? currentEdge : nextIdx;
			edges.erase(edges.begin() + high);
			edges.erase(edges.begin() + low);
			edges.insert(edges.begin() + low, HpsEdge(currEdge.start, nextEdge.end));
			currentEdge = (low + 1) % edges.size();
		}
		else if (opcode == 3)
		{
			if (edges.empty())
			{
				error = _T("Ignore command 전에 edge list가 없습니다.");
				m_faces.clear();
				return FALSE;
			}
			currentEdge = (currentEdge + 1) % edges.size();
		}
		else if (opcode == 4)
		{
			if (globalVertex + 2 >= vertexCount)
			{
				error = _T("Restart command가 vertex_count 범위를 초과했습니다.");
				m_faces.clear();
				return FALSE;
			}
			unsigned int v0 = globalVertex++;
			unsigned int v1 = globalVertex++;
			unsigned int v2 = globalVertex++;
			HpsFace face;
			face.i1 = v0;
			face.i2 = v1;
			face.i3 = v2;
			m_faces.push_back(face);
			edges.clear();
			edges.push_back(HpsEdge(v0, v1));
			edges.push_back(HpsEdge(v1, v2));
			edges.push_back(HpsEdge(v2, v0));
			currentEdge = 0;
		}
		else if (opcode == 5 || opcode == 6)
		{
			unsigned int v0 = 0, v1 = 0, v2 = 0;
			if (opcode == 6 || use32BitPayload)
			{
				if (!ReadUInt32(bytes, pos, v0) || !ReadUInt32(bytes, pos, v1) || !ReadUInt32(bytes, pos, v2))
				{
					error = _T("Restart index payload를 읽을 수 없습니다.");
					m_faces.clear();
					return FALSE;
				}
			}
			else
			{
				if (!ReadUInt16(bytes, pos, v0) || !ReadUInt16(bytes, pos, v1) || !ReadUInt16(bytes, pos, v2))
				{
					error = _T("Restart16 index payload를 읽을 수 없습니다.");
					m_faces.clear();
					return FALSE;
				}
			}
			if (v0 >= vertexCount || v1 >= vertexCount || v2 >= vertexCount)
			{
				error = _T("Restart index가 vertex_count 범위를 벗어났습니다.");
				m_faces.clear();
				return FALSE;
			}
			HpsFace face;
			face.i1 = v0;
			face.i2 = v1;
			face.i3 = v2;
			m_faces.push_back(face);
			edges.clear();
			edges.push_back(HpsEdge(v0, v1));
			edges.push_back(HpsEdge(v1, v2));
			edges.push_back(HpsEdge(v2, v0));
			currentEdge = 0;
		}
		else if (opcode == 7 || opcode == 8)
		{
			if (edges.empty())
			{
				error = _T("Absolute command 전에 edge list가 없습니다.");
				m_faces.clear();
				return FALSE;
			}
			unsigned int v = 0;
			if (opcode == 8 || use32BitPayload)
			{
				if (!ReadUInt32(bytes, pos, v))
				{
					error = _T("Absolute index payload를 읽을 수 없습니다.");
					m_faces.clear();
					return FALSE;
				}
			}
			else
			{
				if (!ReadUInt16(bytes, pos, v))
				{
					error = _T("Absolute16 index payload를 읽을 수 없습니다.");
					m_faces.clear();
					return FALSE;
				}
			}
			if (v >= vertexCount)
			{
				error = _T("Absolute index가 vertex_count 범위를 벗어났습니다.");
				m_faces.clear();
				return FALSE;
			}
			HpsEdge edge = edges[currentEdge];
			HpsFace face;
			face.i1 = v;
			face.i2 = edge.end;
			face.i3 = edge.start;
			m_faces.push_back(face);
			edges.erase(edges.begin() + currentEdge);
			edges.insert(edges.begin() + currentEdge, HpsEdge(v, edge.end));
			edges.insert(edges.begin() + currentEdge, HpsEdge(edge.start, v));
			currentEdge = (currentEdge + 2) % edges.size();
		}
		else if (opcode == 9)
		{
			if (edges.empty())
			{
				error = _T("Remove command 전에 edge list가 없습니다.");
				m_faces.clear();
				return FALSE;
			}
			unsigned int n = (unsigned int)edges.size();
			unsigned int prevIdx = (currentEdge + n - 1) % n;
			HpsEdge prevEdge = edges[prevIdx];
			HpsEdge currEdge = edges[currentEdge];
			if (prevEdge.start == currEdge.end && n > 2)
			{
				unsigned int high = currentEdge > prevIdx ? currentEdge : prevIdx;
				unsigned int low = currentEdge > prevIdx ? prevIdx : currentEdge;
				edges.erase(edges.begin() + high);
				edges.erase(edges.begin() + low);
				if (!edges.empty())
				{
					unsigned int newPrevIdx = (low + (unsigned int)edges.size() - 1) % (unsigned int)edges.size();
					unsigned int newCurrIdx = low % (unsigned int)edges.size();
					edges[newPrevIdx].end = edges[newCurrIdx].start;
					currentEdge = newCurrIdx;
				}
				else
				{
					currentEdge = 0;
				}
			}
			else
			{
				edges[prevIdx].end = currEdge.end;
				edges.erase(edges.begin() + currentEdge);
				currentEdge = edges.empty() ? 0 : currentEdge % edges.size();
			}
		}
		else if (opcode == 10)
		{
			if (globalVertex >= vertexCount)
			{
				error = _T("IncreaseVertexListPointer가 vertex_count 범위를 초과했습니다.");
				m_faces.clear();
				return FALSE;
			}
			++globalVertex;
		}
		else
		{
			error.Format(_T("알 수 없는 face command opcode: %u"), opcode);
			m_faces.clear();
			return FALSE;
		}
	}

	if (m_faces.size() != facetCount)
	{
		error.Format(_T("facet_count 불일치: expected=%u actual=%u commands=%u"),
			facetCount, (unsigned int)m_faces.size(), commands);
		m_faces.clear();
		return FALSE;
	}

	return TRUE;
}

BOOL CMeshData::ReadUInt16(const std::vector<unsigned char>& bytes, size_t& pos, unsigned int& value) const
{
	if (pos + 2 > bytes.size())
		return FALSE;
	value = (unsigned int)bytes[pos] | ((unsigned int)bytes[pos + 1] << 8);
	pos += 2;
	return TRUE;
}

BOOL CMeshData::ReadUInt32(const std::vector<unsigned char>& bytes, size_t& pos, unsigned int& value) const
{
	if (pos + 4 > bytes.size())
		return FALSE;
	value = (unsigned int)bytes[pos] |
		((unsigned int)bytes[pos + 1] << 8) |
		((unsigned int)bytes[pos + 2] << 16) |
		((unsigned int)bytes[pos + 3] << 24);
	pos += 4;
	return TRUE;
}
