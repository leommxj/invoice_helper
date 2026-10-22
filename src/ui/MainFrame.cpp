#include "ui/MainFrame.h"

#include "parser/FileDetector.h"
#include "parser/InvoiceParser.h"
#include "print/PdfPrint.h"
#include "ui/InvoiceDropTarget.h"

#include <wx/arrstr.h>
#include <wx/button.h>
#include <wx/clntdata.h>
#include <wx/dir.h>
#include <wx/datetime.h>
#include <wx/dirdlg.h>
#include <wx/msgdlg.h>
#include <wx/stdpaths.h>
#include <wx/filedlg.h>
#include <wx/filefn.h>
#include <wx/filename.h>
#include <wx/menu.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/splitter.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/tglbtn.h>
#include <wx/bmpbndl.h>
#include <wx/colour.h>
#include <wx/dataview.h>
#include <wx/dvrenderers.h>
#include <wx/toplevel.h>
#include <wx/utils.h>

#if defined(__WXMSW__)
#include <wx/msw/wrapwin.h>
#endif

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace {

enum {
    ID_Open = wxID_HIGHEST + 1,
    ID_OpenFolder,
    ID_Print,
    ID_MergeSave,
    ID_MergeOpen,
    ID_Clear,
    ID_Remove,
    ID_StayOnTop,
};

constexpr std::size_t kNoIndex = static_cast<std::size_t>(-1);

wxString Utf8(const char* text) {
    return wxString::FromUTF8(text);
}

wxString Utf8(const std::string& text) {
    return wxString::FromUTF8(text.c_str());
}

void SetWindowTopMost(wxWindow* window, bool enabled) {
#if defined(__WXMSW__)
    ::SetWindowPos(reinterpret_cast<HWND>(window->GetHandle()),
                   enabled ? HWND_TOPMOST : HWND_NOTOPMOST,
                   0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
#else
    long style = window->GetWindowStyleFlag();
    window->SetWindowStyleFlag(enabled ? style | wxSTAY_ON_TOP : style & ~wxSTAY_ON_TOP);
#endif
    if (enabled) {
        window->Raise();
    }
}

std::filesystem::path ToPath(const wxString& path) {
#if defined(_WIN32)
    return std::filesystem::path(path.ToStdWstring());
#else
    return std::filesystem::path(path.ToStdString());
#endif
}

wxString FromPath(const std::filesystem::path& path) {
#if defined(_WIN32)
    return wxString(path.wstring());
#else
    return wxString::FromUTF8(path.string());
#endif
}

wxString GetPdfCacheDir() {
    wxString root = wxStandardPaths::Get().GetUserDataDir();
    if (root.empty()) {
        root = wxGetCwd();
    }
    return root + wxFILE_SEP_PATH + "print-cache";
}

void CleanupOldPdfFiles() {
    const wxString directory = GetPdfCacheDir();
    if (!wxDirExists(directory)) {
        return;
    }
    wxArrayString files;
    wxDir::GetAllFiles(directory, &files, "*.pdf", wxDIR_FILES);
    const wxDateTime cutoff = wxDateTime::Now() - wxTimeSpan::Hours(24);
    for (const auto& file : files) {
        wxFileName name(file);
        const wxDateTime modified = name.GetModificationTime();
        if (modified.IsValid() && modified.IsEarlierThan(cutoff)) {
            wxRemoveFile(file);
        }
    }
}

std::filesystem::path CreateCachedPdfPath(const wxString& prefix) {
    const wxString directory = GetPdfCacheDir();
    wxFileName::Mkdir(directory, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
    static unsigned long sequence = 0;
    const wxString name = wxString::Format(
        "%s-%s-%lu-%lu.pdf",
        prefix,
        wxDateTime::Now().Format("%Y%m%d-%H%M%S"),
        wxGetProcessId(),
        ++sequence);
    return ToPath(directory + wxFILE_SEP_PATH + name);
}

wxString FormatBytes(std::uintmax_t bytes) {
    constexpr std::uintmax_t KiB = 1024;
    constexpr std::uintmax_t MiB = KiB * 1024;
    if (bytes >= MiB) {
        return wxString::Format("%.1f MB", static_cast<double>(bytes) / static_cast<double>(MiB));
    }
    if (bytes >= KiB) {
        return wxString::Format("%.1f KB", static_cast<double>(bytes) / static_cast<double>(KiB));
    }
    return wxString::Format("%llu B", static_cast<unsigned long long>(bytes));
}

bool IsSupportedExtension(const wxString& path) {
    static const wxString extensions[] = {"pdf", "ofd", "xml"};
    const wxString extension = wxFileName(path).GetExt().Lower();
    return std::any_of(std::begin(extensions), std::end(extensions),
                       [&extension](const wxString& item) { return extension == item; });
}

bool IsImageFormat(invoice::FileFormat format) {
    return format == invoice::FileFormat::Png ||
           format == invoice::FileFormat::Jpeg ||
           format == invoice::FileFormat::Tiff ||
           format == invoice::FileFormat::Webp;
}

void AppendField(wxString& output, const wxString& label, const std::string& value) {
    if (!value.empty()) {
        output << label << Utf8(value) << "\n";
    }
}


struct RowData final : public wxClientData {
    enum class Kind {
        File,
        Folder,
    };

    RowData(Kind kind, std::size_t index, bool duplicate, wxString name,
            wxString number, wxString date, wxString total, wxString status)
        : kind(kind),
          index(index),
          duplicate(duplicate),
          name(std::move(name)),
          number(std::move(number)),
          date(std::move(date)),
          total(std::move(total)),
          status(std::move(status)) {}

    Kind kind;
    std::size_t index;
    bool duplicate;
    wxString name;
    wxString number;
    wxString date;
    wxString total;
    wxString status;
};

class InvoiceTreeModel final : public wxDataViewTreeStore {
public:
    void GetValue(wxVariant& variant, const wxDataViewItem& item,
                  unsigned column) const override {
        auto* data = static_cast<RowData*>(GetItemData(item));
        switch (column) {
            case 0: {
                wxDataViewIconText value(data->name);
                variant << value;
                break;
            }
            case 1: variant = data->number; break;
            case 2: variant = data->date; break;
            case 3: variant = data->total; break;
            case 4: variant = data->status; break;
            default: variant = wxString(); break;
        }
    }

    bool GetAttr(const wxDataViewItem& item, unsigned,
                 wxDataViewItemAttr& attr) const override {
        auto* data = static_cast<RowData*>(GetItemData(item));
        if (data->duplicate) {
            attr.SetColour(wxColour(180, 0, 0));
            return true;
        }
        return false;
    }
};

}

wxBEGIN_EVENT_TABLE(MainFrame, wxFrame)
    EVT_MENU(ID_Open, MainFrame::OpenFiles)
    EVT_MENU(ID_OpenFolder, MainFrame::OpenFolder)
    EVT_MENU(ID_Print, MainFrame::PrintSelected)
    EVT_MENU(ID_MergeSave, MainFrame::MergeSelectedAndSave)
    EVT_MENU(ID_MergeOpen, MainFrame::MergeSelectedAndOpen)
    EVT_MENU(ID_Clear, MainFrame::ClearFiles)
    EVT_MENU(ID_Remove, MainFrame::RemoveSelected)
    EVT_MENU(ID_StayOnTop, MainFrame::OnToggleTopMost)
    EVT_BUTTON(ID_Open, MainFrame::OpenFiles)
    EVT_BUTTON(ID_OpenFolder, MainFrame::OpenFolder)
    EVT_BUTTON(ID_Print, MainFrame::PrintSelected)
    EVT_BUTTON(ID_MergeSave, MainFrame::MergeSelectedAndSave)
    EVT_BUTTON(ID_MergeOpen, MainFrame::MergeSelectedAndOpen)
    EVT_TOGGLEBUTTON(ID_StayOnTop, MainFrame::OnToggleTopMost)
    EVT_DATAVIEW_SELECTION_CHANGED(wxID_ANY, MainFrame::OnSelectionChanged)
    EVT_DATAVIEW_ITEM_CONTEXT_MENU(wxID_ANY, MainFrame::OnFileContextMenu)
wxEND_EVENT_TABLE()

MainFrame::MainFrame()
    : wxFrame(nullptr, wxID_ANY, Utf8("发票小助手"), wxDefaultPosition, wxSize(1200, 760)) {
    SetMinSize(wxSize(900, 560));
    BuildMenu();
    BuildLayout();
    SetDropTarget(new InvoiceDropTarget(this));
    CreateStatusBar();
    SetStatusText(Utf8("可将发票文件或文件夹拖入窗口"));
}

void MainFrame::BuildMenu() {
    auto* fileMenu = new wxMenu();
    fileMenu->Append(ID_Open, Utf8("打开文件...\tCtrl+O"));
    fileMenu->Append(ID_OpenFolder, Utf8("打开文件夹..."));
    fileMenu->Append(ID_Print, Utf8("打印选中...	Ctrl+P"));
    fileMenu->Append(ID_MergeSave, Utf8("合成 PDF 并保存..."));
    fileMenu->Append(ID_MergeOpen, Utf8("合成 PDF 并打开"));
    fileMenu->Append(ID_Remove, Utf8("从列表移除	Delete"));
    fileMenu->Append(ID_Clear, Utf8("清空列表"));
    fileMenu->AppendSeparator();
    fileMenu->Append(wxID_EXIT, Utf8("退出\tAlt+F4"));

    auto* viewMenu = new wxMenu();
    viewMenu->AppendCheckItem(ID_StayOnTop, Utf8("窗口置顶"));

    auto* menuBar = new wxMenuBar();
    menuBar->Append(fileMenu, Utf8("文件"));
    menuBar->Append(viewMenu, Utf8("视图"));
    SetMenuBar(menuBar);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { Close(true); }, wxID_EXIT);
}

void MainFrame::BuildLayout() {
    auto* root = new wxBoxSizer(wxVERTICAL);

    auto* toolbar = new wxPanel(this);
    auto* toolbarSizer = new wxBoxSizer(wxHORIZONTAL);
    toolbarSizer->Add(new wxButton(toolbar, ID_Open, Utf8("打开文件")), 0, wxALL, 4);
    toolbarSizer->Add(new wxButton(toolbar, ID_OpenFolder, Utf8("打开文件夹")), 0, wxALL, 4);
    toolbarSizer->Add(new wxButton(toolbar, ID_Print, Utf8("打印选中")), 0, wxALL, 4);
    toolbarSizer->Add(new wxButton(toolbar, ID_MergeSave, Utf8("合成保存")), 0, wxALL, 4);
    toolbarSizer->Add(new wxButton(toolbar, ID_MergeOpen, Utf8("合成打开")), 0, wxALL, 4);
    topmostButton_ = new wxToggleButton(toolbar, ID_StayOnTop, Utf8("置顶"));
    toolbarSizer->Add(topmostButton_, 0, wxALL, 4);
    toolbarSizer->AddStretchSpacer();
    toolbar->SetSizer(toolbarSizer);
    root->Add(toolbar, 0, wxEXPAND);

    auto* mainSplitter = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                              wxSP_LIVE_UPDATE | wxSP_3D);
    mainSplitter->SetMinimumPaneSize(220);
    mainSplitter->SetSashGravity(0.45);

    auto* left = new wxPanel(mainSplitter);
    auto* leftSizer = new wxBoxSizer(wxVERTICAL);
    summary_ = new wxStaticText(left, wxID_ANY, Utf8("尚未导入文件"));
    leftSizer->Add(summary_, 0, wxALL | wxEXPAND, 8);

    fileTree_ = new wxDataViewCtrl(left, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                   wxDV_MULTIPLE | wxDV_ROW_LINES | wxBORDER_SUNKEN);
    fileTree_->SetMinSize(wxSize(300, 200));
    auto* treeModel = new InvoiceTreeModel();
    fileTree_->AssociateModel(treeModel);
    treeModel->DecRef();
    fileTree_->AppendIconTextColumn(Utf8("文件/文件夹"), 0, wxDATAVIEW_CELL_INERT, 230);
    fileTree_->AppendTextColumn(Utf8("发票号码"), 1, wxDATAVIEW_CELL_INERT, 170);
    fileTree_->AppendTextColumn(Utf8("开票日期"), 2, wxDATAVIEW_CELL_INERT, 90);
    fileTree_->AppendTextColumn(Utf8("价税合计"), 3, wxDATAVIEW_CELL_INERT, 100,
                                wxALIGN_RIGHT);
    fileTree_->AppendTextColumn(Utf8("状态"), 4, wxDATAVIEW_CELL_INERT, 80);
    leftSizer->Add(fileTree_, 1, wxALL | wxEXPAND, 8);
    left->SetSizer(leftSizer);

    auto* right = new wxPanel(mainSplitter);
    auto* rightSizer = new wxBoxSizer(wxVERTICAL);
    auto* detailSplitter = new wxSplitterWindow(right, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                                wxSP_LIVE_UPDATE | wxSP_3D);
    detailSplitter->SetMinimumPaneSize(100);
    detailSplitter->SetSashGravity(0.42);

    auto* overviewPanel = new wxPanel(detailSplitter);
    auto* overviewSizer = new wxBoxSizer(wxVERTICAL);
    overviewSizer->Add(new wxStaticText(overviewPanel, wxID_ANY, Utf8("发票信息")), 0, wxALL, 8);
    overview_ = new wxTextCtrl(overviewPanel, wxID_ANY, Utf8("选择发票或文件夹查看信息。"),
                               wxDefaultPosition, wxDefaultSize,
                               wxTE_MULTILINE | wxTE_READONLY | wxBORDER_SUNKEN);
    overviewSizer->Add(overview_, 1, wxALL | wxEXPAND, 8);
    overviewPanel->SetSizer(overviewSizer);

    auto* itemsPanel = new wxPanel(detailSplitter);
    auto* itemsSizer = new wxBoxSizer(wxVERTICAL);
    itemsSizer->Add(new wxStaticText(itemsPanel, wxID_ANY, Utf8("明细和备注")), 0, wxALL, 8);
    items_ = new wxTextCtrl(itemsPanel, wxID_ANY, Utf8("明细和备注会显示在这里。"),
                            wxDefaultPosition, wxDefaultSize,
                            wxTE_MULTILINE | wxTE_READONLY | wxBORDER_SUNKEN);
    itemsSizer->Add(items_, 1, wxALL | wxEXPAND, 8);
    itemsPanel->SetSizer(itemsSizer);

    detailSplitter->SplitHorizontally(overviewPanel, itemsPanel, 300);
    rightSizer->Add(detailSplitter, 1, wxEXPAND);

    auto* statsBox = new wxStaticBoxSizer(wxVERTICAL, right, Utf8("统计"));
    stats_ = new wxTextCtrl(right, wxID_ANY, wxEmptyString, wxDefaultPosition,
                            wxSize(-1, 112), wxTE_MULTILINE | wxTE_READONLY | wxBORDER_NONE);
    stats_->SetMinSize(wxSize(-1, 112));
    statsBox->Add(stats_, 1, wxEXPAND | wxALL, 6);
    rightSizer->Add(statsBox, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

    right->SetSizer(rightSizer);

    mainSplitter->SplitVertically(left, right, 650);
    root->Add(mainSplitter, 1, wxEXPAND);
    SetSizer(root);
}

void MainFrame::OpenFiles(wxCommandEvent&) {
    wxFileDialog dialog(this, Utf8("选择发票文件"), wxEmptyString, wxEmptyString,
                        Utf8("发票文件 (*.pdf;*.ofd;*.xml)|"
                             "*.pdf;*.ofd;*.xml|"
                             "所有文件 (*.*)|*.*"),
                        wxFD_OPEN | wxFD_FILE_MUST_EXIST | wxFD_MULTIPLE);
    if (dialog.ShowModal() == wxID_OK) {
        wxArrayString paths;
        dialog.GetPaths(paths);
        AddFiles(paths);
    }
}

void MainFrame::OpenFolder(wxCommandEvent&) {
    wxDirDialog dialog(this, Utf8("选择发票文件夹"), wxEmptyString,
                       wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
    if (dialog.ShowModal() == wxID_OK) {
        AddFolder(dialog.GetPath());
        RefreshTree();
    }
}

void MainFrame::PrintSelected(wxCommandEvent&) {
    const auto inputs = SelectedPdfPaths();
    if (inputs.empty()) {
        wxMessageBox(Utf8("请先选择要打印的 PDF 发票。"), Utf8("打印"),
                     wxOK | wxICON_INFORMATION, this);
        return;
    }

    std::filesystem::path printPath;
    if (inputs.size() == 1) {
        printPath = inputs.front();
    } else {
        CleanupOldPdfFiles();
        printPath = CreateCachedPdfPath("invoice-print");
        std::string error;
        if (!invoice::MergePdfFiles(inputs, printPath, error)) {
            wxMessageBox(Utf8("合并 PDF 失败：") + Utf8(error), Utf8("打印"),
                         wxOK | wxICON_ERROR, this);
            return;
        }
    }

    std::string error;
    if (!invoice::PrintPdfWithSystem(printPath, error)) {
        wxMessageBox(Utf8(error), Utf8("打印"), wxOK | wxICON_ERROR, this);
        return;
    }
    SetStatusText(Utf8("已调用系统打印"));
}

void MainFrame::MergeSelectedAndSave(wxCommandEvent&) {
    const auto inputs = SelectedPdfPaths();
    if (inputs.empty()) {
        wxMessageBox(Utf8("请先选择要合并的 PDF 发票。"), Utf8("合成 PDF"),
                     wxOK | wxICON_INFORMATION, this);
        return;
    }

    wxString defaultDir = wxStandardPaths::Get().GetDocumentsDir();
    if (defaultDir.empty()) {
        defaultDir = wxGetCwd();
    }
    const wxString defaultName = wxString::Format(
        Utf8("合并发票-%s.pdf"), wxDateTime::Now().Format("%Y%m%d-%H%M%S"));

    wxFileDialog dialog(this, Utf8("保存合并 PDF"), defaultDir, defaultName,
                        Utf8("PDF 文件 (*.pdf)|*.pdf"),
                        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dialog.ShowModal() != wxID_OK) {
        return;
    }

    wxString output = dialog.GetPath();
    if (!output.Lower().EndsWith(".pdf")) {
        output += ".pdf";
    }

    std::error_code ec;
    for (const auto& input : inputs) {
        if (std::filesystem::exists(ToPath(output)) &&
            std::filesystem::equivalent(ToPath(output), input, ec) && !ec) {
            wxMessageBox(Utf8("保存路径不能与选中的发票相同，请换一个文件名。"),
                         Utf8("合成 PDF"), wxOK | wxICON_WARNING, this);
            return;
        }
        ec.clear();
    }

    std::string error;
    if (!invoice::MergePdfFiles(inputs, ToPath(output), error)) {
        wxMessageBox(Utf8("合并 PDF 失败：") + Utf8(error), Utf8("合成 PDF"),
                     wxOK | wxICON_ERROR, this);
        return;
    }
    SetStatusText(Utf8("已保存合并 PDF：") + output);
}

void MainFrame::MergeSelectedAndOpen(wxCommandEvent&) {
    const auto inputs = SelectedPdfPaths();
    if (inputs.empty()) {
        wxMessageBox(Utf8("请先选择要合并的 PDF 发票。"), Utf8("合成 PDF"),
                     wxOK | wxICON_INFORMATION, this);
        return;
    }

    if (inputs.size() == 1) {
        if (!wxLaunchDefaultApplication(FromPath(inputs.front()))) {
            wxMessageBox(Utf8("无法调用系统 PDF 阅读器。"), Utf8("合成 PDF"),
                         wxOK | wxICON_ERROR, this);
        }
        return;
    }

    CleanupOldPdfFiles();
    const std::filesystem::path output = CreateCachedPdfPath("invoice-merged");
    std::string error;
    if (!invoice::MergePdfFiles(inputs, output, error)) {
        wxMessageBox(Utf8("合并 PDF 失败：") + Utf8(error), Utf8("合成 PDF"),
                     wxOK | wxICON_ERROR, this);
        return;
    }
    if (!wxLaunchDefaultApplication(FromPath(output))) {
        wxMessageBox(Utf8("已生成合并 PDF，但无法调用系统 PDF 阅读器：") +
                         wxString::Format("\n%s", FromPath(output)),
                     Utf8("合成 PDF"), wxOK | wxICON_ERROR, this);
    }
}

void MainFrame::ClearFiles(wxCommandEvent&) {
    auto* model = static_cast<InvoiceTreeModel*>(fileTree_->GetModel());
    model->DeleteAllItems();
    model->Cleared();
    files_.clear();
    folders_.clear();
    UpdateSelectionStats();
    ShowEmptyDetails();
}

void MainFrame::AddFiles(const wxArrayString& paths) {
    int skipped = 0;
    for (const auto& path : paths) {
        if (wxDirExists(path)) {
            AddFolder(path);
        } else if (wxFileExists(path)) {
            const auto format = invoice::DetectFormat(path);
            if (format == invoice::FileFormat::Unknown || IsImageFormat(format)) {
                ++skipped;
                continue;
            }
            AddFile(path, kNoIndex);
        }
    }
    RefreshTree();
    if (skipped > 0) {
        SetStatusText(wxString::Format(Utf8("已跳过 %d 个不支持的文件"), skipped));
    }
}

void MainFrame::AddFolder(const wxString& path) {
    if (!wxDirExists(path)) {
        return;
    }

    const wxString folderPath = wxFileName::DirName(path).GetFullPath();
    wxArrayString allFiles;
    wxDir::GetAllFiles(folderPath, &allFiles, wxEmptyString, wxDIR_FILES | wxDIR_DIRS);

    std::vector<wxString> invoiceFiles;
    for (const auto& file : allFiles) {
        if (IsSupportedExtension(file)) {
            invoiceFiles.push_back(file);
        }
    }
    if (invoiceFiles.empty()) {
        SetStatusText(Utf8("文件夹中没有支持的发票文件"));
        return;
    }

    std::size_t folderIndex = FindFolder(folderPath);
    if (folderIndex == kNoIndex) {
        wxString name = wxFileName(folderPath).GetName();
        if (name.empty()) {
            name = folderPath;
        }
        folders_.push_back(FolderEntry{name, folderPath, {}, false});
        folderIndex = folders_.size() - 1;
    }

    for (const auto& file : invoiceFiles) {
        AddFile(file, folderIndex);
    }
}

bool MainFrame::AddFile(const wxString& path, std::size_t folderIndex) {
    wxFileName file(path);
    if (!file.FileExists()) {
        return false;
    }

    const wxString fullPath = file.GetFullPath();
    if (FindFile(fullPath) != kNoIndex) {
        return false;
    }

    invoice::ImportedFile imported;
    imported.path = fullPath;
    imported.name = file.GetFullName();
    imported.size = static_cast<std::uintmax_t>(file.GetSize().GetValue());
    imported.format = invoice::DetectFormat(fullPath);

    if (imported.format == invoice::FileFormat::Pdf ||
        imported.format == invoice::FileFormat::Ofd ||
        imported.format == invoice::FileFormat::Xml) {
        std::string error;
        bool parsed = false;
        if (imported.format == invoice::FileFormat::Pdf) {
            parsed = invoice::ParsePdfInvoice(ToPath(fullPath), imported.invoice, error);
        } else if (imported.format == invoice::FileFormat::Ofd) {
            parsed = invoice::ParseOfdInvoice(ToPath(fullPath), imported.invoice, error);
        } else {
            parsed = invoice::ParseXmlInvoice(ToPath(fullPath), imported.invoice, error);
        }

        if (parsed) {
            if (imported.invoice.title.empty() &&
                !imported.invoice.hasInvoiceNumberLabel) {
                imported.status = invoice::ImportStatus::NotInvoice;
            } else if (!imported.invoice.numberFromLabel ||
                       !imported.invoice.hasTotalWithTax) {
                imported.status = invoice::ImportStatus::ParseFailed;
                imported.parseError = "缺少有效发票号码或价税合计";
            } else {
                imported.status = invoice::ImportStatus::Parsed;
                for (const auto& existing : files_) {
                    if (existing.status == invoice::ImportStatus::Duplicate) {
                        continue;
                    }
                    if (existing.status == invoice::ImportStatus::Parsed &&
                        existing.invoice.numberFromLabel &&
                        existing.invoice.number == imported.invoice.number) {
                        imported.status = invoice::ImportStatus::Duplicate;
                        imported.duplicateOfName = existing.name;
                        break;
                    }
                }
            }
        } else {
            imported.status = invoice::ImportStatus::ParseFailed;
            imported.parseError = error;
        }
    } else {
        imported.status = invoice::ImportStatus::Unsupported;
    }

    if (folderIndex == kNoIndex) {
        folderIndex = EnsureUngroupedFolder();
    }

    folders_[folderIndex].fileIndices.push_back(files_.size());
    files_.push_back(std::move(imported));
    return true;
}

std::size_t MainFrame::EnsureUngroupedFolder() {
    for (std::size_t i = 0; i < folders_.size(); ++i) {
        if (folders_[i].synthetic) {
            return i;
        }
    }
    folders_.push_back(FolderEntry{Utf8("未分组"), wxEmptyString, {}, true});
    return folders_.size() - 1;
}

std::size_t MainFrame::FindFolder(const wxString& path) const {
    for (std::size_t i = 0; i < folders_.size(); ++i) {
        if (folders_[i].path == path) {
            return i;
        }
    }
    return kNoIndex;
}

std::size_t MainFrame::FindFile(const wxString& path) const {
    for (std::size_t i = 0; i < files_.size(); ++i) {
        if (files_[i].path == path) {
            return i;
        }
    }
    return kNoIndex;
}

void MainFrame::RefreshTree() {
    auto* model = static_cast<InvoiceTreeModel*>(fileTree_->GetModel());
    model->DeleteAllItems();
    model->Cleared();
    const wxDataViewItem root;

    for (std::size_t folderIndex = 0; folderIndex < folders_.size(); ++folderIndex) {
        const auto& folder = folders_[folderIndex];

        const InvoiceStats stats = ComputeStats(folder.fileIndices, folder.name);
        const wxString folderNumber = wxString::Format(Utf8("%llu 文件 / %d 发票"),
            static_cast<unsigned long long>(stats.totalCount), stats.uniqueInvoiceCount);
        const wxString folderStatus = stats.duplicateCount > 0
            ? wxString::Format(Utf8("%d 重复"), stats.duplicateCount)
            : wxString();
        const wxString folderAmount = stats.withTotalCount > 0
            ? Utf8(invoice::FormatMoney(stats.totalCents))
            : wxString();

        auto* folderData = new RowData(RowData::Kind::Folder, folderIndex, false,
                                       folder.name, folderNumber, wxString(),
                                       folderAmount, folderStatus);
        wxDataViewItem folderItem = model->AppendContainer(
            root, folder.name, wxBitmapBundle(), wxBitmapBundle(), folderData);
        model->ItemAdded(root, folderItem);

        for (std::size_t fileIndex : folder.fileIndices) {
            const auto& file = files_[fileIndex];
            const bool duplicate = file.status == invoice::ImportStatus::Duplicate;
            auto* fileData = new RowData(
                RowData::Kind::File, fileIndex, duplicate, file.name,
                file.invoice.parsed ? Utf8(file.invoice.number) : wxString(),
                file.invoice.parsed ? Utf8(file.invoice.issueDate) : wxString(),
                file.invoice.parsed ? Utf8(file.invoice.totalWithTax) : wxString(),
                invoice::StatusName(file.status));
            wxDataViewItem fileItem =
                model->AppendItem(folderItem, file.name, wxBitmapBundle(), fileData);
            model->ItemAdded(folderItem, fileItem);
        }

        if (auto* container = model->FindContainerNode(folderItem)) {
            container->SetExpanded(true);
        }
        fileTree_->Expand(folderItem);
    }

    UpdateSelectionStats();
    fileTree_->Refresh();
}

std::vector<std::size_t> MainFrame::SelectedFileIndices() const {
    wxDataViewItemArray selections;
    fileTree_->GetSelections(selections);
    auto* model = static_cast<InvoiceTreeModel*>(fileTree_->GetModel());

    std::set<std::size_t> selected;
    for (const auto& item : selections) {
        auto* data = static_cast<RowData*>(model->GetItemData(item));
        if (data->kind == RowData::Kind::File) {
            selected.insert(data->index);
        } else {
            for (std::size_t fileIndex : folders_[data->index].fileIndices) {
                selected.insert(fileIndex);
            }
        }
    }
    return std::vector<std::size_t>(selected.begin(), selected.end());
}

std::vector<std::filesystem::path> MainFrame::SelectedPdfPaths() const {
    std::vector<std::filesystem::path> paths;
    for (std::size_t index : SelectedFileIndices()) {
        const auto& file = files_[index];
        if (file.format == invoice::FileFormat::Pdf) {
            paths.push_back(ToPath(file.path));
        }
    }
    return paths;
}

void MainFrame::UpdateSelectionStats() {
    wxDataViewItemArray selections;
    fileTree_->GetSelections(selections);
    const bool hasSelection = !selections.empty();

    std::vector<std::size_t> indices;
    if (hasSelection) {
        indices = SelectedFileIndices();
    } else {
        indices.resize(files_.size());
        for (std::size_t i = 0; i < files_.size(); ++i) {
            indices[i] = i;
        }
    }

    const wxString scope = hasSelection
        ? wxString::Format(Utf8("选中 %llu 个文件"),
                           static_cast<unsigned long long>(indices.size()))
        : wxString::Format(Utf8("全部 %llu 个文件"),
                           static_cast<unsigned long long>(indices.size()));
    const InvoiceStats stats = ComputeStats(indices, scope);

    wxString summary;
    summary << (hasSelection ? Utf8("已选 ") : Utf8("共 "))
            << stats.totalCount << Utf8(" 个文件，发票 ")
            << stats.uniqueInvoiceCount << Utf8(" 张");
    if (stats.duplicateCount > 0) {
        summary << Utf8("，其中重复 ") << stats.duplicateCount << Utf8(" 张");
    }
    if (stats.withTotalCount > 0) {
        summary << Utf8("，价税合计 ") << Utf8(invoice::FormatMoney(stats.totalCents));
    }
    summary_->SetLabel(summary);

    wxString content;
    content << Utf8("范围：") << stats.scope << "\n"
            << Utf8("文件：") << stats.totalCount
            << Utf8("    唯一发票：") << stats.uniqueInvoiceCount
            << Utf8("    重复：") << stats.duplicateCount << "\n"
            << Utf8("非发票：") << stats.notInvoiceCount
            << Utf8("    解析失败：") << stats.parseFailedCount << "\n"
            << Utf8("金额（去重）：") << Utf8(invoice::FormatMoney(stats.amountCents))
            << Utf8("    税额（去重）：") << Utf8(invoice::FormatMoney(stats.taxCents)) << "\n"
            << Utf8("价税合计（去重）：") << Utf8(invoice::FormatMoney(stats.totalCents));
    stats_->SetValue(content);
}

void MainFrame::OnSelectionChanged(wxDataViewEvent&) {
    UpdateSelectionStats();
    ShowSelectionDetails();
}

void MainFrame::OnFileContextMenu(wxDataViewEvent& event) {
    const wxDataViewItem item = event.GetItem();
    if (!item.IsOk()) {
        return;
    }

    wxDataViewItemArray selections;
    fileTree_->GetSelections(selections);
    bool selected = false;
    for (const auto& selection : selections) {
        if (selection == item) {
            selected = true;
            break;
        }
    }
    if (!selected) {
        fileTree_->UnselectAll();
        fileTree_->Select(item);
    }

    wxMenu menu;
    menu.Append(ID_Remove, Utf8("从列表移除"));
    menu.AppendSeparator();
    menu.Append(ID_Clear, Utf8("清空列表"));
    PopupMenu(&menu);
}

void MainFrame::RemoveSelected(wxCommandEvent&) {
    wxDataViewItemArray selections;
    fileTree_->GetSelections(selections);
    if (selections.empty()) {
        return;
    }

    auto* model = static_cast<InvoiceTreeModel*>(fileTree_->GetModel());
    std::vector<bool> removeFile(files_.size(), false);
    std::vector<bool> removeFolder(folders_.size(), false);
    int removedFiles = 0;
    int removedFolders = 0;

    for (const auto& item : selections) {
        auto* data = static_cast<RowData*>(model->GetItemData(item));
        if (data == nullptr) {
            continue;
        }
        if (data->kind == RowData::Kind::File) {
            if (data->index < removeFile.size() && !removeFile[data->index]) {
                removeFile[data->index] = true;
                ++removedFiles;
            }
        } else if (data->index < removeFolder.size()) {
            removeFolder[data->index] = true;
            ++removedFolders;
            for (std::size_t fileIndex : folders_[data->index].fileIndices) {
                if (fileIndex < removeFile.size() && !removeFile[fileIndex]) {
                    removeFile[fileIndex] = true;
                    ++removedFiles;
                }
            }
        }
    }

    if (removedFiles == 0 && removedFolders == 0) {
        return;
    }

    std::vector<std::size_t> mapping(files_.size(), kNoIndex);
    std::vector<invoice::ImportedFile> keptFiles;
    keptFiles.reserve(files_.size());
    for (std::size_t i = 0; i < files_.size(); ++i) {
        if (removeFile[i]) {
            continue;
        }
        mapping[i] = keptFiles.size();
        keptFiles.push_back(std::move(files_[i]));
    }

    std::vector<FolderEntry> keptFolders;
    for (std::size_t i = 0; i < folders_.size(); ++i) {
        if (removeFolder[i]) {
            continue;
        }
        FolderEntry folder = std::move(folders_[i]);
        std::vector<std::size_t> indices;
        for (std::size_t oldIndex : folder.fileIndices) {
            if (oldIndex < mapping.size() && mapping[oldIndex] != kNoIndex) {
                indices.push_back(mapping[oldIndex]);
            }
        }
        folder.fileIndices = std::move(indices);
        if (!folder.fileIndices.empty()) {
            keptFolders.push_back(std::move(folder));
        }
    }

    files_ = std::move(keptFiles);
    folders_ = std::move(keptFolders);

    RecomputeDuplicates();
    RefreshTree();
    ShowEmptyDetails();
    if (removedFiles > 0) {
        SetStatusText(wxString::Format(Utf8("已从列表移除 %d 个文件"), removedFiles));
    } else {
        SetStatusText(Utf8("已从列表移除空文件夹"));
    }
}

void MainFrame::ShowSelectionDetails() {
    wxDataViewItemArray selections;
    fileTree_->GetSelections(selections);
    if (selections.empty()) {
        ShowEmptyDetails();
        return;
    }

    auto* model = static_cast<InvoiceTreeModel*>(fileTree_->GetModel());
    auto* data = static_cast<RowData*>(model->GetItemData(selections[0]));
    if (data->kind == RowData::Kind::File) {
        ShowInvoiceDetails(files_[data->index]);
    } else {
        ShowFolderDetails(folders_[data->index]);
    }
}

void MainFrame::ShowEmptyDetails() {
    overview_->SetValue(Utf8("选择发票或文件夹查看信息。"));
    items_->SetValue(Utf8("明细和备注会显示在这里。"));
}

void MainFrame::ShowInvoiceDetails(const invoice::ImportedFile& file) {
    wxString overview;
    overview << Utf8("文件名：") << file.name << "\n"
             << Utf8("路径：") << file.path << "\n"
             << Utf8("格式：") << invoice::FormatName(file.format) << "\n"
             << Utf8("大小：") << FormatBytes(file.size) << "\n";
    const wxString status = invoice::StatusName(file.status);
    if (!status.empty()) {
        overview << Utf8("状态：") << status << "\n";
    }
    if (file.status == invoice::ImportStatus::Duplicate) {
        overview << Utf8("重复于：") << file.duplicateOfName << "\n";
    }

    wxString items;
    if (file.invoice.parsed) {
        const auto& parsed = file.invoice;
        overview << "\n";
        AppendField(overview, Utf8("票种："), parsed.title);
        AppendField(overview, Utf8("标签："), parsed.tag);
        AppendField(overview, Utf8("发票号码："), parsed.number);
        AppendField(overview, Utf8("开票日期："), parsed.issueDate);
        AppendField(overview, Utf8("购买方："), parsed.buyer.name);
        AppendField(overview, Utf8("购买方税号："), parsed.buyer.taxId);
        AppendField(overview, Utf8("销售方："), parsed.seller.name);
        AppendField(overview, Utf8("销售方税号："), parsed.seller.taxId);
        AppendField(overview, Utf8("金额："), parsed.totalAmount);
        AppendField(overview, Utf8("税额："), parsed.totalTax);
        AppendField(overview, Utf8("价税合计："), parsed.totalWithTax);
        AppendField(overview, Utf8("价税合计（大写）："), parsed.amountInWords);
        AppendField(overview, Utf8("开票人："), parsed.issuer);

        if (!parsed.items.empty()) {
            items << Utf8("明细") << "\n";
            for (std::size_t i = 0; i < parsed.items.size(); ++i) {
                const auto& item = parsed.items[i];
                items << wxString::Format("%llu. ",
                                          static_cast<unsigned long long>(i + 1))
                      << Utf8(item.name);
                if (!item.specification.empty()) {
                    items << Utf8("  规格 ") << Utf8(item.specification);
                }
                if (!item.unit.empty()) {
                    items << Utf8("  单位 ") << Utf8(item.unit);
                }
                if (!item.quantity.empty()) {
                    items << Utf8("  数量 ") << Utf8(item.quantity);
                }
                if (!item.unitPrice.empty()) {
                    items << Utf8("  单价 ") << Utf8(item.unitPrice);
                }
                if (!item.amount.empty()) {
                    items << Utf8("  金额 ") << Utf8(item.amount);
                }
                if (!item.taxRate.empty()) {
                    items << Utf8("  税率 ") << Utf8(item.taxRate);
                }
                if (!item.taxAmount.empty()) {
                    items << Utf8("  税额 ") << Utf8(item.taxAmount);
                }
                items << "\n";
            }
        }
        if (!parsed.remarks.empty()) {
            if (!items.empty()) {
                items << "\n";
            }
            items << Utf8("备注") << "\n" << Utf8(parsed.remarks);
        }
    } else if (!file.parseError.empty()) {
        overview << "\n" << Utf8("解析失败：") << Utf8(file.parseError);
    }

    if (items.empty()) {
        items = Utf8("无明细数据。");
    }
    overview_->SetValue(overview);
    items_->SetValue(items);
}

void MainFrame::ShowFolderDetails(const FolderEntry& folder) {
    const InvoiceStats stats = ComputeStats(folder.fileIndices, folder.name);
    wxString overview;
    overview << Utf8("文件夹：") << folder.name << "\n";
    if (!folder.path.empty()) {
        overview << Utf8("路径：") << folder.path << "\n";
    }

    wxString items;
    for (std::size_t fileIndex : folder.fileIndices) {
        const auto& file = files_[fileIndex];

        items << file.name;
        if (file.status == invoice::ImportStatus::Duplicate) {
            items << Utf8("  [重复]");
        } else if (file.status == invoice::ImportStatus::NotInvoice) {
            items << Utf8("  [非发票]");
        } else if (file.status == invoice::ImportStatus::ParseFailed) {
            items << Utf8("  [解析失败]");
        }
        if (file.invoice.parsed) {
            if (!file.invoice.number.empty()) {
                items << Utf8("  ") << Utf8(file.invoice.number);
            }
            if (!file.invoice.totalWithTax.empty()) {
                items << Utf8("  ") << Utf8(file.invoice.totalWithTax);
            }
        }
        items << "\n";
    }

    overview << Utf8("文件：") << stats.totalCount
             << Utf8("    唯一发票：") << stats.uniqueInvoiceCount;
    if (stats.duplicateCount > 0) {
        overview << Utf8("    重复：") << stats.duplicateCount;
    }
    overview << "\n";
    if (stats.notInvoiceCount > 0) {
        overview << Utf8("非发票：") << stats.notInvoiceCount << "\n";
    }
    if (stats.parseFailedCount > 0) {
        overview << Utf8("解析失败：") << stats.parseFailedCount << "\n";
    }
    if (stats.withTotalCount > 0) {
        overview << Utf8("价税合计（去重）：")
                 << Utf8(invoice::FormatMoney(stats.totalCents)) << "\n";
    }
    if (items.empty()) {
        items = Utf8("文件夹为空。");
    }
    overview_->SetValue(overview);
    items_->SetValue(items);
}

InvoiceStats MainFrame::ComputeStats(const std::vector<std::size_t>& indices,
                                     const wxString& scope) const {
    InvoiceStats stats;
    stats.scope = scope;
    std::set<std::string> seenInvoiceNumbers;

    for (std::size_t index : indices) {
        const auto& file = files_[index];
        ++stats.totalCount;
        if (file.status == invoice::ImportStatus::Duplicate) {
            ++stats.duplicateCount;
        } else if (file.status == invoice::ImportStatus::NotInvoice) {
            ++stats.notInvoiceCount;
        } else if (file.status == invoice::ImportStatus::ParseFailed) {
            ++stats.parseFailedCount;
        }

        if (!file.invoice.parsed || file.invoice.number.empty()) {
            continue;
        }
        if (!seenInvoiceNumbers.insert(file.invoice.number).second) {
            continue;
        }

        ++stats.uniqueInvoiceCount;
        std::int64_t cents = 0;
        if (invoice::ParseMoneyCents(file.invoice.totalAmount, cents)) {
            stats.amountCents += cents;
        }
        if (invoice::ParseMoneyCents(file.invoice.totalTax, cents)) {
            stats.taxCents += cents;
        }
        if (file.invoice.hasTotalWithTax) {
            ++stats.withTotalCount;
            stats.totalCents += file.invoice.totalWithTaxCents;
        }
    }
    return stats;
}

void MainFrame::RecomputeDuplicates() {
    std::map<std::string, wxString> firstByName;
    for (auto& file : files_) {
        if (file.status == invoice::ImportStatus::Duplicate) {
            file.status = invoice::ImportStatus::Parsed;
            file.duplicateOfName.clear();
        }
        if (!file.invoice.parsed || file.invoice.number.empty()) {
            continue;
        }

        const auto it = firstByName.find(file.invoice.number);
        if (it != firstByName.end()) {
            file.status = invoice::ImportStatus::Duplicate;
            file.duplicateOfName = it->second;
        } else {
            file.status = invoice::ImportStatus::Parsed;
            file.duplicateOfName.clear();
            firstByName.emplace(file.invoice.number, file.name);
        }
    }
}

void MainFrame::OnToggleTopMost(wxCommandEvent& event) {
    SetTopMost(event.IsChecked());
}

void MainFrame::SetTopMost(bool enabled) {
    SetWindowTopMost(this, enabled);
    topmostButton_->SetValue(enabled);
    GetMenuBar()->Check(ID_StayOnTop, enabled);
}
