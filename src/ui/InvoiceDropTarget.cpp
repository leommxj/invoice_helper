#include "ui/InvoiceDropTarget.h"

#include "ui/MainFrame.h"

InvoiceDropTarget::InvoiceDropTarget(MainFrame* owner) : owner_(owner) {}

bool InvoiceDropTarget::OnDropFiles(wxCoord, wxCoord, const wxArrayString& filenames) {
    owner_->AddFiles(filenames);
    return true;
}
