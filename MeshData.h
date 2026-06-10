#pragma once

#include <vector>

struct HpsVertex
{
	float x;
	float y;
	float z;
};

struct HpsFace
{
	unsigned int i1;
	unsigned int i2;
	unsigned int i3;
};

struct HpsNormal
{
	float x;
	float y;
	float z;
};

struct HpsBoundingBox
{
	float minX;
	float minY;
	float minZ;
	float maxX;
	float maxY;
	float maxZ;
	BOOL valid;
};

class CMeshData
{
public:
	enum VertexFormat
	{
		VertexFormatUnknown = 0,
		VertexFormatFloat32,
		VertexFormatFloat64,
		VertexFormatCompressed
	};

	enum FacetFormat
	{
		FacetFormatUnknown = 0,
		FacetFormatUInt32,
		FacetFormatUInt16,
		FacetFormatCompressed,
		FacetFormatCcCommands16,
		FacetFormatCcCommands32
	};

	CMeshData();

	void Clear();
	BOOL DetectVertexFormat(const std::vector<unsigned char>& bytes, unsigned int vertexCount, std::vector<CString>& logs);
	BOOL DetectFacetFormat(const std::vector<unsigned char>& bytes, unsigned int facetCount, unsigned int vertexCount, std::vector<CString>& logs);
	BOOL ComputeNormal(const HpsFace& face, HpsNormal& normal) const;
	unsigned int CountWritableTriangles(unsigned int* skippedDegenerate) const;

	const std::vector<HpsVertex>& GetVertices() const { return m_vertices; }
	const std::vector<HpsFace>& GetFaces() const { return m_faces; }
	const HpsBoundingBox& GetBoundingBox() const { return m_bounds; }
	VertexFormat GetVertexFormat() const { return m_vertexFormat; }
	FacetFormat GetFacetFormat() const { return m_facetFormat; }

private:
	BOOL UpdateBoundingBox(std::vector<CString>& logs);
	BOOL IsFiniteFloat(float v) const;
	BOOL IsReasonableRange(float span) const;
	BOOL DecodeCcFacetCommands(const std::vector<unsigned char>& bytes, unsigned int facetCount, unsigned int vertexCount, BOOL use32BitPayload, CString& error);
	BOOL ReadUInt16(const std::vector<unsigned char>& bytes, size_t& pos, unsigned int& value) const;
	BOOL ReadUInt32(const std::vector<unsigned char>& bytes, size_t& pos, unsigned int& value) const;

	std::vector<HpsVertex> m_vertices;
	std::vector<HpsFace> m_faces;
	HpsBoundingBox m_bounds;
	VertexFormat m_vertexFormat;
	FacetFormat m_facetFormat;
};
