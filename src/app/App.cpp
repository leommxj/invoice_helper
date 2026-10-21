#include "app/App.h"

#include "ui/MainFrame.h"

bool App::OnInit() {
    SetAppName("invoice_helper");
    SetAppDisplayName(wxString::FromUTF8("发票小助手"));
    auto* frame = new MainFrame();
    frame->Show(true);
    return true;
}
