#pragma once

#include "model/InvoiceDocument.h"

namespace invoice {

FileFormat DetectFormat(const wxString& path);

}
