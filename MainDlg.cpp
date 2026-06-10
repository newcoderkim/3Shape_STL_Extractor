// MainDlg.cpp : implementation of the CMainDlg class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"

#include "aboutdlg.h"
#include "MainDlg.h"
#include "FileUtil.h"
#include "StlWriter.h"
#include "TextureWriter.h"

BOOL CMainDlg::PreTranslateMessage(MSG* pMsg)
{
	return CWindow::IsDialogMessage(pMsg);
}

BOOL CMainDlg::OnIdle()
{
	UIUpdateChildWindows();
	return FALSE;
}

LRESULT CMainDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	m_hasParsedDocument = FALSE;

	// center the dialog on the screen
	CenterWindow();

	// set icons
	HICON hIcon = AtlLoadIconImage(IDR_MAINFRAME, LR_DEFAULTCOLOR, ::GetSystemMetrics(SM_CXICON), ::GetSystemMetrics(SM_CYICON));
	SetIcon(hIcon, TRUE);
	HICON hIconSmall = AtlLoadIconImage(IDR_MAINFRAME, LR_DEFAULTCOLOR, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON));
	SetIcon(hIconSmall, FALSE);

	// register object for message filtering and idle updates
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop != NULL);
	pLoop->AddMessageFilter(this);
	pLoop->AddIdleHandler(this);

	UIAddChildWindowContainer(m_hWnd);
	AppendLog(_T("3Shape HPS Mesh STL Extractor 준비 완료"));

	return TRUE;
}

LRESULT CMainDlg::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	// unregister message filtering and idle updates
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop != NULL);
	pLoop->RemoveMessageFilter(this);
	pLoop->RemoveIdleHandler(this);

	return 0;
}

LRESULT CMainDlg::OnAppAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CAboutDlg dlg;
	dlg.DoModal();
	return 0;
}

LRESULT CMainDlg::OnSelectFile(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CFileDialog dlg(TRUE, _T("dcm"), NULL, OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
		_T("DCM Files (*.dcm;*.DCM)\0*.dcm;*.DCM\0All Files (*.*)\0*.*\0"));
	if (dlg.DoModal() == IDOK)
	{
		m_filePath = dlg.m_szFileName;
		SetDlgItemText(IDC_EDIT_FILE, m_filePath);
		if (m_outputFolder.IsEmpty())
		{
			m_outputFolder = CFileUtil::ToDisplayPath(CFileUtil::GetDirectoryName(CFileUtil::ToWidePath(m_filePath)));
			SetDlgItemText(IDC_EDIT_OUTPUT, m_outputFolder);
		}
		m_hasParsedDocument = FALSE;
		AppendLog(_T("파일 선택: ") + m_filePath);
	}
	return 0;
}

LRESULT CMainDlg::OnSelectOutputFolder(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CString folder;
	GetDlgItemText(IDC_EDIT_OUTPUT, folder);
	if (BrowseFolder(folder))
	{
		m_outputFolder = folder;
		SetDlgItemText(IDC_EDIT_OUTPUT, m_outputFolder);
		AppendLog(_T("출력 폴더 선택: ") + m_outputFolder);
	}
	return 0;
}

LRESULT CMainDlg::OnSelectInputFolder(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CString folder;
	GetDlgItemText(IDC_EDIT_INPUT_FOLDER, folder);
	if (BrowseFolder(folder))
	{
		m_inputFolder = folder;
		SetDlgItemText(IDC_EDIT_INPUT_FOLDER, m_inputFolder);
		AppendLog(_T("입력 폴더 선택: ") + m_inputFolder);
	}
	return 0;
}

LRESULT CMainDlg::OnAnalyze(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	AnalyzeCurrentFile();
	return 0;
}

LRESULT CMainDlg::OnExtractStl(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (!m_hasParsedDocument && !AnalyzeCurrentFile())
		return 0;

	const CMeshData& mesh = m_doc.GetMeshData();
	if (mesh.GetVertices().empty() || mesh.GetFaces().empty())
	{
		AppendLog(_T("STL 생성 실패: vertices/facets 포맷이 정상 해석되지 않았습니다."));
		SaveDebugFiles();
		return 0;
	}

	CStringW outFolder = CFileUtil::ToWidePath(GetOutputFolder());
	CStringW baseName = m_doc.GetBaseName();
	CStringW binaryPath = CFileUtil::CombinePath(outFolder, baseName + L".stl");
	CStringW asciiPath = CFileUtil::CombinePath(outFolder, baseName + L"_ascii.stl");
	CString error;
	unsigned int skippedBinary = 0;
	unsigned int skippedAscii = 0;

	if (CStlWriter::WriteBinaryStl(binaryPath, mesh, skippedBinary, error))
	{
		AppendLog(_T("Binary STL 저장 경로: ") + CFileUtil::ToDisplayPath(binaryPath));
		if (skippedBinary > 0)
		{
			CString line;
			line.Format(_T("degenerate triangle 스킵: %u"), skippedBinary);
			AppendLog(line);
		}
	}
	else
	{
		AppendLog(_T("Binary STL 저장 실패: ") + error);
	}

	if (CStlWriter::WriteAsciiStl(asciiPath, mesh, skippedAscii, error))
	{
		AppendLog(_T("ASCII STL 저장 경로: ") + CFileUtil::ToDisplayPath(asciiPath));
	}
	else
	{
		AppendLog(_T("ASCII STL 저장 실패: ") + error);
	}

	SaveDebugFiles();
	return 0;
}

LRESULT CMainDlg::OnExtractImage(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (!m_hasParsedDocument && !AnalyzeCurrentFile())
		return 0;

	CStringW savedPath;
	CString error;
	if (CTextureWriter::WriteTexture(CFileUtil::ToWidePath(GetOutputFolder()), m_doc.GetBaseName(), m_doc.GetTexture(), savedPath, error))
	{
		AppendLog(_T("이미지 저장 경로: ") + CFileUtil::ToDisplayPath(savedPath));
	}
	else
	{
		AppendLog(_T("이미지 저장 실패: ") + error);
	}

	SaveDebugFiles();
	return 0;
}

LRESULT CMainDlg::OnBatchAnalyze(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	GetDlgItemText(IDC_EDIT_INPUT_FOLDER, m_inputFolder);
	if (m_inputFolder.IsEmpty())
	{
		AppendLog(_T("입력 폴더를 먼저 선택하세요."));
		return 0;
	}

	std::vector<CStringW> files;
	CFileUtil::FindDcmFilesRecursive(CFileUtil::ToWidePath(m_inputFolder), files);
	CString line;
	line.Format(_T("재귀 탐색 DCM 파일 수: %u"), (unsigned int)files.size());
	AppendLog(line);

	for (size_t i = 0; i < files.size(); ++i)
	{
		AppendLog(_T("----------------------------------------"));
		AppendLog(_T("분석 파일: ") + CFileUtil::ToDisplayPath(files[i]));

		CHpsMeshDocument doc;
		std::vector<CString> logs;
		if (doc.LoadFromFile(CFileUtil::ToDisplayPath(files[i]), logs))
			doc.Parse(logs);
		AppendLogs(logs);

		line.Format(_T("요약: vertex_count=%u, facet_count=%u, texture=%ux%u bpp=%u, color=%u"),
			doc.GetVertexCount(), doc.GetFacetCount(),
			doc.GetTexture().width, doc.GetTexture().height, doc.GetTexture().bytesPerPixel,
			doc.GetFacetColor());
		AppendLog(line);
	}

	AppendLog(_T("일괄 분석 종료"));
	return 0;
}

LRESULT CMainDlg::OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CloseDialog(wID);
	return 0;
}

void CMainDlg::AppendLog(const CString& text)
{
	CEdit edit = GetDlgItem(IDC_EDIT_LOG);
	if (!edit.IsWindow())
		return;

	CString line(text);
	line += _T("\r\n");
	int len = edit.GetWindowTextLength();
	edit.SetSel(len, len);
	edit.ReplaceSel(line);
}

void CMainDlg::AppendLogs(const std::vector<CString>& logs)
{
	for (size_t i = 0; i < logs.size(); ++i)
		AppendLog(logs[i]);
}

BOOL CMainDlg::BrowseFolder(CString& folder)
{
	BROWSEINFO bi;
	memset(&bi, 0, sizeof(bi));
	bi.hwndOwner = m_hWnd;
	bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
	bi.lpszTitle = _T("폴더를 선택하세요.");

	LPITEMIDLIST pidl = ::SHBrowseForFolder(&bi);
	if (pidl == NULL)
		return FALSE;

	TCHAR path[MAX_PATH] = { 0 };
	BOOL ok = ::SHGetPathFromIDList(pidl, path);
	IMalloc* pMalloc = NULL;
	if (SUCCEEDED(::SHGetMalloc(&pMalloc)) && pMalloc != NULL)
	{
		pMalloc->Free(pidl);
		pMalloc->Release();
	}

	if (!ok)
		return FALSE;

	folder = path;
	return TRUE;
}

BOOL CMainDlg::AnalyzeCurrentFile()
{
	GetDlgItemText(IDC_EDIT_FILE, m_filePath);
	if (m_filePath.IsEmpty())
	{
		AppendLog(_T("분석할 .dcm 파일을 먼저 선택하세요."));
		return FALSE;
	}

	std::vector<CString> logs;
	BOOL loaded = m_doc.LoadFromFile(m_filePath, logs);
	BOOL parsed = FALSE;
	if (loaded)
		parsed = m_doc.Parse(logs);
	AppendLogs(logs);

	m_hasParsedDocument = loaded && parsed;
	if (loaded)
		SaveDebugFiles();

	return m_hasParsedDocument;
}

CString CMainDlg::GetOutputFolder()
{
	GetDlgItemText(IDC_EDIT_OUTPUT, m_outputFolder);
	if (m_outputFolder.IsEmpty() && !m_filePath.IsEmpty())
	{
		m_outputFolder = CFileUtil::ToDisplayPath(CFileUtil::GetDirectoryName(CFileUtil::ToWidePath(m_filePath)));
		SetDlgItemText(IDC_EDIT_OUTPUT, m_outputFolder);
	}
	return m_outputFolder;
}

void CMainDlg::SaveDebugFiles()
{
	CString folder = GetOutputFolder();
	if (folder.IsEmpty())
		return;

	std::vector<CString> logs;
	m_doc.SaveDebugFiles(folder, logs);
	AppendLogs(logs);
}

void CMainDlg::CloseDialog(int nVal)
{
	DestroyWindow();
	::PostQuitMessage(nVal);
}
