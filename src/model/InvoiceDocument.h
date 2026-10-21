#pragma once

#include "parser/Invoice.h"

#include <wx/string.h>

#include <cstdint>
#include <string>

namespace invoice {

enum class FileFormat {
    Unknown,
    Pdf,
    Ofd,
    Xml,
    Png,
    Jpeg,
    Tiff,
    Webp,
};

enum class ImportStatus {
    Imported,
    Parsed,
    Duplicate,
    ParseFailed,
    Unsupported,
    Failed,
};

struct ImportedFile {
    wxString path;
    wxString name;
    FileFormat format = FileFormat::Unknown;
    std::uintmax_t size = 0;
    ImportStatus status = ImportStatus::Imported;
    std::string parseError;
    wxString duplicateOfName;
    Invoice invoice;
};

wxString FormatName(FileFormat format);
wxString StatusName(ImportStatus status);

}
