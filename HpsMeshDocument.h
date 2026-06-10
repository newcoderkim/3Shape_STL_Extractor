#pragma once

#include <map>
#include <string>
#include <vector>
#include "MeshData.h"

struct HpsTagData
{
	BOOL found;
	std::string body;
	std::map<std::string, std::string> attributes;

	HpsTagData() : found(FALSE) {}
};

struct HpsTextureData
{
	unsigned int width;
	unsigned int height;
	unsigned int bytesPerPixel;
	CString textureName;
	std::vector<unsigned char> bytes;

	HpsTextureData() : width(0), height(0), bytesPerPixel(0) {}
};

class CHpsMeshDocument
{
public:
	CHpsMeshDocument();

	BOOL LoadFromFile(const CString& filePath, std::vector<CString>& logs);
	BOOL Parse(std::vector<CString>& logs);
	BOOL SaveDebugFiles(const CString& outputFolder, std::vector<CString>& logs) const;

	BOOL IsDicom() const { return m_isDicom; }
	BOOL IsHpsMesh() const { return m_isHpsMesh; }
	const CStringW& GetFilePath() const { return m_filePath; }
	const CStringW& GetBaseName() const { return m_baseName; }
	unsigned int GetVertexCount() const { return m_vertexCount; }
	unsigned int GetFacetCount() const { return m_facetCount; }
	unsigned int GetFacetColor() const { return m_facetColor; }
	const std::vector<unsigned char>& GetDecodedVertices() const { return m_decodedVertices; }
	const std::vector<unsigned char>& GetDecodedFacets() const { return m_decodedFacets; }
	const HpsTextureData& GetTexture() const { return m_texture; }
	CMeshData& GetMeshData() { return m_mesh; }
	const CMeshData& GetMeshData() const { return m_mesh; }

	static BOOL ExtractTag(const std::string& text, const char* tagName, HpsTagData& tag);
	static void ParseAttributes(const std::string& tagOpenText, std::map<std::string, std::string>& attrs);

private:
	static unsigned int ParseUIntAttribute(const std::map<std::string, std::string>& attrs, const char* name);
	static std::string GetAttribute(const std::map<std::string, std::string>& attrs, const char* name);
	static BOOL CompareExpectedBytes(const char* label, unsigned int expected, size_t actual, std::vector<CString>& logs);
	CString MakeParsedInfoText() const;

	CStringW m_filePath;
	CStringW m_baseName;
	std::vector<unsigned char> m_fileBytes;
	std::string m_text;
	BOOL m_isDicom;
	BOOL m_isHpsMesh;

	unsigned int m_vertexCount;
	unsigned int m_facetCount;
	unsigned int m_facetColor;
	unsigned int m_verticesExpectedBytes;
	unsigned int m_facetsExpectedBytes;
	unsigned int m_textureExpectedBytes;

	std::vector<unsigned char> m_decodedVertices;
	std::vector<unsigned char> m_decodedFacets;
	HpsTextureData m_texture;
	CMeshData m_mesh;
};
