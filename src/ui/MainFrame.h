#pragma once

#include "model/InvoiceDocument.h"

#include <wx/frame.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

class wxStaticText;
class wxTextCtrl;
class wxToggleButton;
class wxDataViewCtrl;
class wxDataViewEvent;

struct InvoiceStats {
    wxString scope;
    int totalCount = 0;
    int duplicateCount = 0;
    int unparsedCount = 0;
    int withTotalCount = 0;
    std::int64_t amountCents = 0;
    std::int64_t taxCents = 0;
    std::int64_t totalCents = 0;
};

class MainFrame final : public wxFrame {
public:
    MainFrame();

    void AddFiles(const wxArrayString& paths);

private:
    struct FolderEntry {
        wxString name;
        wxString path;
        std::vector<std::size_t> fileIndices;
        bool synthetic = false;
    };

    void BuildMenu();
    void BuildLayout();
    void OpenFiles(wxCommandEvent& event);
    void OpenFolder(wxCommandEvent& event);
    void ClearFiles(wxCommandEvent& event);
    void PrintSelected(wxCommandEvent& event);
    void MergeSelectedAndSave(wxCommandEvent& event);
    void MergeSelectedAndOpen(wxCommandEvent& event);
    void OnToggleTopMost(wxCommandEvent& event);
    void OnSelectionChanged(wxDataViewEvent& event);

    void AddFolder(const wxString& path);
    bool AddFile(const wxString& path, std::size_t folderIndex);
    std::size_t EnsureUngroupedFolder();
    std::size_t FindFolder(const wxString& path) const;
    std::size_t FindFile(const wxString& path) const;

    void RefreshTree();
    std::vector<std::size_t> SelectedFileIndices() const;
    std::vector<std::filesystem::path> SelectedPdfPaths() const;
    void UpdateSelectionStats();
    void ShowSelectionDetails();
    void ShowEmptyDetails();
    void ShowInvoiceDetails(const invoice::ImportedFile& file);
    void ShowFolderDetails(const FolderEntry& folder);
    InvoiceStats ComputeStats(const std::vector<std::size_t>& indices,
                              const wxString& scope) const;
    void SetTopMost(bool enabled);

    wxDataViewCtrl* fileTree_ = nullptr;
    wxStaticText* summary_ = nullptr;
    wxTextCtrl* overview_ = nullptr;
    wxTextCtrl* items_ = nullptr;
    wxTextCtrl* stats_ = nullptr;
    wxToggleButton* topmostButton_ = nullptr;
    std::vector<invoice::ImportedFile> files_;
    std::vector<FolderEntry> folders_;

    wxDECLARE_EVENT_TABLE();
};
