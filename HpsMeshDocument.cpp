#include "stdafx.h"
#include "HpsMeshDocument.h"
#include "Base64Decoder.h"
#include "FileUtil.h"
#include <sstream>
#include <stdlib.h>

CHpsMeshDocument::CHpsMeshDocument()
	: m_isDicom(FALSE)
	, m_isHpsMesh(FALSE)
	, m_vertexCount(0)
	, m_facetCount(0)
	, m_facetColor(0)
	, m_verticesExpectedBytes(0)
	, m_facetsExpectedBytes(0)
	, m_textureExpectedBytes(0)
{
}

BOOL CHpsMeshDocument::LoadFromFile(const CString& filePath, std::vector<CString>& logs)
{
	m_filePath = CFileUtil::ToWidePath(filePath);
	m_baseName = CFileUtil::GetFileNameNoExt(m_filePath);
	m_fileBytes.clear();
	m_text.clear();
	m_isDicom = FALSE;
	m_isHpsMesh = FALSE;
	m_vertexCount = 0;
	m_facetCount = 0;
	m_facetColor = 0;
	m_verticesExpectedBytes = 0;
	m_facetsExpectedBytes = 0;
	m_textureExpectedBytes = 0;
	m_decodedVertices.clear();
	m_decodedFacets.clear();
	m_texture = HpsTextureData();
	m_mesh.Clear();

	CString error;
	if (!CFileUtil::ReadAllBytes(m_filePath, m_fileBytes, error))
	{
		logs.push_back(error);
		return FALSE;
	}

	CString line;
	line.Format(_T("파일 크기: %u bytes"), (unsigned int)m_fileBytes.size());
	logs.push_back(line);

	if (m_fileBytes.size() >= 132 &&
		m_fileBytes[128] == 'D' && m_fileBytes[129] == 'I' &&
		m_fileBytes[130] == 'C' && m_fileBytes[131] == 'M')
	{
		m_isDicom = TRUE;
		logs.push_back(_T("DICM 헤더 감지: 일반 CT DICOM은 지원하지 않습니다."));
	}

	if (!m_fileBytes.empty())
		m_text.assign((const char*)&m_fileBytes[0], (const char*)&m_fileBytes[0] + m_fileBytes.size());

	if (m_text.find("<HPS") != std::string::npos || m_text.find("<Packed_geometry") != std::string::npos)
	{
		m_isHpsMesh = TRUE;
		logs.push_back(_T("HPS Mesh XML 구조 감지"));
	}
	else if (m_isDicom)
	{
		logs.push_back(_T("이 도구는 CT DICOM이 아닌 3Shape HPS Mesh DCM만 지원합니다."));
	}
	else
	{
		logs.push_back(_T("HPS Mesh XML 태그를 찾지 못했습니다."));
	}

	return TRUE;
}

BOOL CHpsMeshDocument::Parse(std::vector<CString>& logs, BOOL decodeBinary)
{
	if (!m_isHpsMesh)
	{
		logs.push_back(_T("파싱 실패: HPS Mesh 파일이 아닙니다."));
		return FALSE;
	}

	HpsTagData verticesTag;
	HpsTagData facetsTag;
	HpsTagData textureTag;
	HpsTagData uvTag;
	HpsTagData schemaTag;

	BOOL ok = TRUE;
	CString schemaText;
	if (ExtractTag(m_text, "Schema", schemaTag))
	{
		schemaText = CString(schemaTag.body.c_str());
		schemaText.Trim();
		CString line;
		line.Format(_T("Schema = %s"), schemaText.GetString());
		logs.push_back(line);
	}

	if (!decodeBinary)
	{
		HpsTagData verticesHeader;
		HpsTagData facetsHeader;
		HpsTagData textureHeader;
		HpsTagData uvHeader;

		BOOL ok = TRUE;
		if (!ExtractTagHeader(m_text, "Vertices", verticesHeader))
		{
			logs.push_back(_T("Vertices 태그가 없습니다."));
			ok = FALSE;
		}
		if (!ExtractTagHeader(m_text, "Facets", facetsHeader))
		{
			logs.push_back(_T("Facets 태그가 없습니다."));
			ok = FALSE;
		}
		ExtractTagHeader(m_text, "TextureImage", textureHeader);
		ExtractTagHeader(m_text, "PerVertexTextureCoord", uvHeader);

		if (!ok)
			return FALSE;

		m_vertexCount = ParseUIntAttribute(verticesHeader.attributes, "vertex_count");
		m_facetCount = ParseUIntAttribute(facetsHeader.attributes, "facet_count");
		m_verticesExpectedBytes = ParseUIntAttribute(verticesHeader.attributes, "base64_encoded_bytes");
		m_facetsExpectedBytes = ParseUIntAttribute(facetsHeader.attributes, "base64_encoded_bytes");
		m_facetColor = ParseUIntAttribute(facetsHeader.attributes, "color");

		CString line;
		line.Format(_T("암호화 여부 = %s"), schemaText.CompareNoCase(_T("CE")) == 0 ? _T("예 (CE schema)") : _T("아니오"));
		logs.push_back(line);
		if (schemaText.CompareNoCase(_T("CE")) == 0)
			logs.push_back(_T("CE schema는 Vertices 복호화에 HPS_ENCRYPTION_KEY가 필요합니다."));

		line.Format(_T("vertex_count = %u"), m_vertexCount);
		logs.push_back(line);
		line.Format(_T("facet_count = %u"), m_facetCount);
		logs.push_back(line);
		line.Format(_T("Vertices encoded bytes = %u"), m_verticesExpectedBytes);
		logs.push_back(line);
		line.Format(_T("Facets encoded bytes = %u"), m_facetsExpectedBytes);
		logs.push_back(line);
		line.Format(_T("Facets color = %u"), m_facetColor);
		logs.push_back(line);

		if (textureHeader.found)
		{
			m_texture.width = ParseUIntAttribute(textureHeader.attributes, "Width");
			m_texture.height = ParseUIntAttribute(textureHeader.attributes, "Height");
			m_texture.bytesPerPixel = ParseUIntAttribute(textureHeader.attributes, "BytesPerPixel");
			m_textureExpectedBytes = ParseUIntAttribute(textureHeader.attributes, "Base64EncodedBytes");
			line.Format(_T("TextureImage Width = %u"), m_texture.width);
			logs.push_back(line);
			line.Format(_T("TextureImage Height = %u"), m_texture.height);
			logs.push_back(line);
			line.Format(_T("BytesPerPixel = %u"), m_texture.bytesPerPixel);
			logs.push_back(line);
			line.Format(_T("TextureImage encoded bytes = %u"), m_textureExpectedBytes);
			logs.push_back(line);
		}
		else
		{
			logs.push_back(_T("TextureImage 태그가 없습니다."));
		}

		if (uvHeader.found)
		{
			unsigned int uvBytes = ParseUIntAttribute(uvHeader.attributes, "Base64EncodedBytes");
			line.Format(_T("PerVertexTextureCoord 태그 감지: encoded bytes = %u"), uvBytes);
			logs.push_back(line);
		}
		else
		{
			logs.push_back(_T("PerVertexTextureCoord 태그가 없습니다."));
		}

		logs.push_back(_T("경량 분석 완료: Base64 디코딩 및 STL/이미지 데이터 해석은 수행하지 않았습니다."));
		return TRUE;
	}

	if (!ExtractTag(m_text, "Vertices", verticesTag))
	{
		logs.push_back(_T("Vertices 태그가 없습니다."));
		ok = FALSE;
	}
	if (!ExtractTag(m_text, "Facets", facetsTag))
	{
		logs.push_back(_T("Facets 태그가 없습니다."));
		ok = FALSE;
	}
	if (!ExtractTag(m_text, "TextureImage", textureTag))
	{
		logs.push_back(_T("TextureImage 태그가 없습니다."));
	}
	if (ExtractTag(m_text, "PerVertexTextureCoord", uvTag))
	{
		CString line;
		line.Format(_T("PerVertexTextureCoord 태그 감지: base64 text %u chars"), (unsigned int)uvTag.body.size());
		logs.push_back(line);
	}
	else
	{
		logs.push_back(_T("PerVertexTextureCoord 태그가 없습니다."));
	}

	if (!ok)
		return FALSE;

	m_vertexCount = ParseUIntAttribute(verticesTag.attributes, "vertex_count");
	m_facetCount = ParseUIntAttribute(facetsTag.attributes, "facet_count");
	m_verticesExpectedBytes = ParseUIntAttribute(verticesTag.attributes, "base64_encoded_bytes");
	m_facetsExpectedBytes = ParseUIntAttribute(facetsTag.attributes, "base64_encoded_bytes");
	m_facetColor = ParseUIntAttribute(facetsTag.attributes, "color");

	CString line;
	line.Format(_T("vertex_count = %u"), m_vertexCount);
	logs.push_back(line);
	line.Format(_T("facet_count = %u"), m_facetCount);
	logs.push_back(line);
	line.Format(_T("Facets color = %u"), m_facetColor);
	logs.push_back(line);

	CString error;
	if (!CBase64Decoder::Decode(verticesTag.body, m_decodedVertices, error))
	{
		logs.push_back(_T("Vertices Base64 디코딩 실패: ") + error);
		ok = FALSE;
	}
	else
	{
		line.Format(_T("decoded vertices bytes = %u"), (unsigned int)m_decodedVertices.size());
		logs.push_back(line);
		CompareExpectedBytes("Vertices", m_verticesExpectedBytes, m_decodedVertices.size(), logs);
	}

	if (!CBase64Decoder::Decode(facetsTag.body, m_decodedFacets, error))
	{
		logs.push_back(_T("Facets Base64 디코딩 실패: ") + error);
		ok = FALSE;
	}
	else
	{
		line.Format(_T("decoded facets bytes = %u"), (unsigned int)m_decodedFacets.size());
		logs.push_back(line);
		CompareExpectedBytes("Facets", m_facetsExpectedBytes, m_decodedFacets.size(), logs);
	}

	if (textureTag.found)
	{
		m_texture.width = ParseUIntAttribute(textureTag.attributes, "Width");
		m_texture.height = ParseUIntAttribute(textureTag.attributes, "Height");
		m_texture.bytesPerPixel = ParseUIntAttribute(textureTag.attributes, "BytesPerPixel");
		m_textureExpectedBytes = ParseUIntAttribute(textureTag.attributes, "Base64EncodedBytes");
		m_texture.textureName = CString(GetAttribute(textureTag.attributes, "TextureName").c_str());

		line.Format(_T("TextureImage Width = %u"), m_texture.width);
		logs.push_back(line);
		line.Format(_T("TextureImage Height = %u"), m_texture.height);
		logs.push_back(line);
		line.Format(_T("BytesPerPixel = %u"), m_texture.bytesPerPixel);
		logs.push_back(line);

		if (!CBase64Decoder::Decode(textureTag.body, m_texture.bytes, error))
		{
			logs.push_back(_T("TextureImage Base64 디코딩 실패: ") + error);
		}
		else
		{
			line.Format(_T("decoded texture bytes = %u"), (unsigned int)m_texture.bytes.size());
			logs.push_back(line);
			CompareExpectedBytes("TextureImage", m_textureExpectedBytes, m_texture.bytes.size(), logs);
		}
	}

	if (!m_decodedVertices.empty())
	{
		if (!m_mesh.DetectVertexFormat(m_decodedVertices, m_vertexCount, logs))
		{
			if (schemaText.CompareNoCase(_T("CE")) == 0)
				logs.push_back(_T("CE schema의 Vertices는 Blowfish 암호화 데이터입니다. STL 생성을 위해 HPS_ENCRYPTION_KEY 복호화 키가 필요합니다."));
			logs.push_back(_T("Vertices 압축 포맷으로 보이며 추가 해석 필요"));
		}
	}

	if (!m_decodedFacets.empty())
	{
		if (!m_mesh.DetectFacetFormat(m_decodedFacets, m_facetCount, m_vertexCount, logs))
			logs.push_back(_T("Facets 압축 포맷으로 보이며 추가 해석 필요"));
	}

	return ok;
}

BOOL CHpsMeshDocument::SaveDebugFiles(const CString& outputFolder, std::vector<CString>& logs) const
{
	CStringW folder = CFileUtil::ToWidePath(outputFolder);
	CString error;
	BOOL ok = TRUE;

	CStringW vertexPath = CFileUtil::CombinePath(folder, L"decoded_vertices.bin");
	if (!m_decodedVertices.empty() && !CFileUtil::WriteAllBytes(vertexPath, m_decodedVertices, error))
	{
		logs.push_back(_T("decoded_vertices.bin 저장 실패: ") + error);
		ok = FALSE;
	}

	CStringW facetPath = CFileUtil::CombinePath(folder, L"decoded_facets.bin");
	if (!m_decodedFacets.empty() && !CFileUtil::WriteAllBytes(facetPath, m_decodedFacets, error))
	{
		logs.push_back(_T("decoded_facets.bin 저장 실패: ") + error);
		ok = FALSE;
	}

	CStringW texturePath = CFileUtil::CombinePath(folder, L"decoded_texture.bin");
	if (!m_texture.bytes.empty() && !CFileUtil::WriteAllBytes(texturePath, m_texture.bytes, error))
	{
		logs.push_back(_T("decoded_texture.bin 저장 실패: ") + error);
		ok = FALSE;
	}

	CStringA infoA(MakeParsedInfoText());
	CStringW infoPath = CFileUtil::CombinePath(folder, L"parsed_info.txt");
	if (!CFileUtil::WriteTextFile(infoPath, std::string(infoA), error))
	{
		logs.push_back(_T("parsed_info.txt 저장 실패: ") + error);
		ok = FALSE;
	}
	else
	{
		logs.push_back(_T("디버그 파일 저장 완료: ") + CFileUtil::ToDisplayPath(folder));
	}

	return ok;
}

BOOL CHpsMeshDocument::ExtractTag(const std::string& text, const char* tagName, HpsTagData& tag)
{
	tag = HpsTagData();
	std::string openPrefix = "<";
	openPrefix += tagName;
	std::string closeText = "</";
	closeText += tagName;
	closeText += ">";

	size_t open = std::string::npos;
	size_t searchFrom = 0;
	while (TRUE)
	{
		size_t candidate = text.find(openPrefix, searchFrom);
		if (candidate == std::string::npos)
			return FALSE;

		size_t next = candidate + openPrefix.size();
		if (next < text.size())
		{
			char ch = text[next];
			if (ch == '>' || ch == '/' || ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n')
			{
				open = candidate;
				break;
			}
		}

		searchFrom = candidate + openPrefix.size();
	}

	size_t openEnd = text.find('>', open);
	if (openEnd == std::string::npos)
		return FALSE;

	size_t close = text.find(closeText, openEnd + 1);
	if (close == std::string::npos)
		return FALSE;

	std::string openText = text.substr(open, openEnd - open + 1);
	ParseAttributes(openText, tag.attributes);
	tag.body = text.substr(openEnd + 1, close - openEnd - 1);
	tag.found = TRUE;
	return TRUE;
}

BOOL CHpsMeshDocument::ExtractTagHeader(const std::string& text, const char* tagName, HpsTagData& tag)
{
	tag = HpsTagData();
	std::string openPrefix = "<";
	openPrefix += tagName;

	size_t open = std::string::npos;
	size_t searchFrom = 0;
	while (TRUE)
	{
		size_t candidate = text.find(openPrefix, searchFrom);
		if (candidate == std::string::npos)
			return FALSE;

		size_t next = candidate + openPrefix.size();
		if (next < text.size())
		{
			char ch = text[next];
			if (ch == '>' || ch == '/' || ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n')
			{
				open = candidate;
				break;
			}
		}

		searchFrom = candidate + openPrefix.size();
	}

	size_t openEnd = text.find('>', open);
	if (openEnd == std::string::npos)
		return FALSE;

	std::string openText = text.substr(open, openEnd - open + 1);
	ParseAttributes(openText, tag.attributes);
	tag.found = TRUE;
	return TRUE;
}

void CHpsMeshDocument::ParseAttributes(const std::string& tagOpenText, std::map<std::string, std::string>& attrs)
{
	attrs.clear();
	size_t pos = 0;
	while (pos < tagOpenText.size())
	{
		while (pos < tagOpenText.size() && (tagOpenText[pos] == '<' || tagOpenText[pos] == '/' || tagOpenText[pos] == '>' || tagOpenText[pos] == ' ' || tagOpenText[pos] == '\t' || tagOpenText[pos] == '\r' || tagOpenText[pos] == '\n'))
			++pos;

		size_t keyStart = pos;
		while (pos < tagOpenText.size() && tagOpenText[pos] != '=' && tagOpenText[pos] != '>' && tagOpenText[pos] != ' ' && tagOpenText[pos] != '\t')
			++pos;

		if (pos >= tagOpenText.size() || tagOpenText[pos] != '=')
		{
			++pos;
			continue;
		}

		std::string key = tagOpenText.substr(keyStart, pos - keyStart);
		++pos;
		if (pos >= tagOpenText.size() || tagOpenText[pos] != '"')
			continue;
		++pos;
		size_t valueStart = pos;
		while (pos < tagOpenText.size() && tagOpenText[pos] != '"')
			++pos;
		if (pos >= tagOpenText.size())
			break;
		attrs[key] = tagOpenText.substr(valueStart, pos - valueStart);
		++pos;
	}
}

unsigned int CHpsMeshDocument::ParseUIntAttribute(const std::map<std::string, std::string>& attrs, const char* name)
{
	std::map<std::string, std::string>::const_iterator it = attrs.find(name);
	if (it == attrs.end())
		return 0;
	return (unsigned int)strtoul(it->second.c_str(), NULL, 10);
}

std::string CHpsMeshDocument::GetAttribute(const std::map<std::string, std::string>& attrs, const char* name)
{
	std::map<std::string, std::string>::const_iterator it = attrs.find(name);
	if (it == attrs.end())
		return "";
	return it->second;
}

BOOL CHpsMeshDocument::CompareExpectedBytes(const char* label, unsigned int expected, size_t actual, std::vector<CString>& logs)
{
	CString line;
	CString labelText(label);
	if (expected == 0)
	{
		line.Format(_T("%s expected bytes attribute 없음 또는 0"), labelText.GetString());
		logs.push_back(line);
		return TRUE;
	}
	if (expected == actual)
	{
		line.Format(_T("%s decoded bytes 일치: %u"), labelText.GetString(), expected);
		logs.push_back(line);
		return TRUE;
	}
	line.Format(_T("%s decoded bytes 불일치: attr=%u actual=%u"), labelText.GetString(), expected, (unsigned int)actual);
	logs.push_back(line);
	return FALSE;
}

CString CHpsMeshDocument::MakeParsedInfoText() const
{
	CString info;
	info.AppendFormat(_T("file=%s\r\n"), CFileUtil::ToDisplayPath(m_filePath));
	info.AppendFormat(_T("is_dicom=%u\r\n"), m_isDicom);
	info.AppendFormat(_T("is_hps_mesh=%u\r\n"), m_isHpsMesh);
	info.AppendFormat(_T("vertex_count=%u\r\n"), m_vertexCount);
	info.AppendFormat(_T("facet_count=%u\r\n"), m_facetCount);
	info.AppendFormat(_T("facet_color=%u\r\n"), m_facetColor);
	info.AppendFormat(_T("decoded_vertices_bytes=%u\r\n"), (unsigned int)m_decodedVertices.size());
	info.AppendFormat(_T("decoded_facets_bytes=%u\r\n"), (unsigned int)m_decodedFacets.size());
	info.AppendFormat(_T("texture_width=%u\r\n"), m_texture.width);
	info.AppendFormat(_T("texture_height=%u\r\n"), m_texture.height);
	info.AppendFormat(_T("texture_bytes_per_pixel=%u\r\n"), m_texture.bytesPerPixel);
	info.AppendFormat(_T("decoded_texture_bytes=%u\r\n"), (unsigned int)m_texture.bytes.size());
	const HpsBoundingBox& box = m_mesh.GetBoundingBox();
	if (box.valid)
	{
		info.AppendFormat(_T("bbox_min=%f,%f,%f\r\n"), box.minX, box.minY, box.minZ);
		info.AppendFormat(_T("bbox_max=%f,%f,%f\r\n"), box.maxX, box.maxY, box.maxZ);
	}
	return info;
}
