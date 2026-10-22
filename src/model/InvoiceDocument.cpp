#include "model/InvoiceDocument.h"

namespace {

wxString Utf8(const char* text) {
    return wxString::FromUTF8(text);
}

}

namespace invoice {

wxString FormatName(FileFormat format) {
    switch (format) {
        case FileFormat::Pdf: return "PDF";
        case FileFormat::Ofd: return "OFD";
        case FileFormat::Xml: return "XML";
        case FileFormat::Png: return "PNG";
        case FileFormat::Jpeg: return "JPEG";
        case FileFormat::Tiff: return "TIFF";
        case FileFormat::Webp: return "WebP";
        case FileFormat::Unknown: return Utf8("未知");
    }
    return Utf8("未知");
}

wxString StatusName(ImportStatus status) {
    switch (status) {
        case ImportStatus::Imported: return Utf8("已导入");
        case ImportStatus::Parsed: return wxString();
        case ImportStatus::NotInvoice: return Utf8("非发票");
        case ImportStatus::Duplicate: return Utf8("重复");
        case ImportStatus::ParseFailed: return Utf8("解析失败");
        case ImportStatus::Unsupported: return Utf8("不支持");
        case ImportStatus::Failed: return Utf8("导入失败");
    }
    return Utf8("未知");
}

}
