#pragma once

#include <wx/dnd.h>

class MainFrame;

class InvoiceDropTarget final : public wxFileDropTarget {
public:
    explicit InvoiceDropTarget(MainFrame* owner);

    bool OnDropFiles(wxCoord x, wxCoord y, const wxArrayString& filenames) override;

private:
    MainFrame* owner_;
};
