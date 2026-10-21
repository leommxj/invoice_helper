#include "parser/FileDetector.h"

#include <wx/file.h>
#include <wx/filename.h>

#include <array>
#include <cctype>
#include <string>

namespace {

bool StartsWith(const std::string& data, const char* prefix) {
    const std::string expected(prefix);
    return data.size() >= expected.size() && data.compare(0, expected.size(), expected) == 0;
}

bool HasExtension(const wxFileName& file, const wxString& extension) {
    return file.GetExt().Lower() == extension;
}

bool LooksLikeXml(const std::string& data) {
    std::size_t index = 0;
    while (index < data.size() && std::isspace(static_cast<unsigned char>(data[index]))) {
        ++index;
    }
    return index < data.size() && data[index] == '<';
}

}

namespace invoice {

FileFormat DetectFormat(const wxString& path) {
    const wxFileName file(path);
    wxFile input(path);
    if (!input.IsOpened()) {
        return FileFormat::Unknown;
    }

    std::array<char, 8192> buffer{};
    const auto bytesRead = input.Read(buffer.data(), buffer.size());
    if (bytesRead == wxInvalidOffset) {
        return FileFormat::Unknown;
    }
    const std::string data(buffer.data(), static_cast<std::size_t>(bytesRead));

    if (StartsWith(data, "%PDF-")) {
        return FileFormat::Pdf;
    }
    if (data.size() >= 4 && static_cast<unsigned char>(data[0]) == 0x89 &&
        data[1] == 'P' && data[2] == 'N' && data[3] == 'G') {
        return FileFormat::Png;
    }
    if (data.size() >= 3 && static_cast<unsigned char>(data[0]) == 0xff &&
        static_cast<unsigned char>(data[1]) == 0xd8 &&
        static_cast<unsigned char>(data[2]) == 0xff) {
        return FileFormat::Jpeg;
    }
    if (data.size() >= 4 &&
        ((data[0] == 'I' && data[1] == 'I' && data[2] == '*' && data[3] == '\0') ||
         (data[0] == 'M' && data[1] == 'M' && data[2] == '\0' && data[3] == '*'))) {
        return FileFormat::Tiff;
    }
    if (data.size() >= 12 && StartsWith(data, "RIFF") && data.compare(8, 4, "WEBP") == 0) {
        return FileFormat::Webp;
    }
    if (LooksLikeXml(data)) {
        return FileFormat::Xml;
    }

    if (data.size() >= 4 && static_cast<unsigned char>(data[0]) == 'P' &&
        static_cast<unsigned char>(data[1]) == 'K' &&
        (static_cast<unsigned char>(data[2]) == 3 || static_cast<unsigned char>(data[2]) == 5 ||
         static_cast<unsigned char>(data[2]) == 7) &&
        (static_cast<unsigned char>(data[3]) == 4 || static_cast<unsigned char>(data[3]) == 6 ||
         static_cast<unsigned char>(data[3]) == 8)) {
        return HasExtension(file, "ofd") ? FileFormat::Ofd : FileFormat::Unknown;
    }

    return FileFormat::Unknown;
}

}
