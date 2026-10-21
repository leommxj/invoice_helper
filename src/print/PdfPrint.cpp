#include "print/PdfPrint.h"

#include <mupdf/pdf.h>

#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#endif

#include <cstdio>
#include <string>

namespace invoice {
namespace {

}

bool MergePdfFiles(const std::vector<std::filesystem::path>& inputs,
                   const std::filesystem::path& output,
                   std::string& error) {
    error.clear();
    if (inputs.empty()) {
        error = "没有可合并的 PDF 文件";
        return false;
    }

    fz_context* ctx = fz_new_context(nullptr, nullptr, FZ_STORE_UNLIMITED);
    if (ctx == nullptr) {
        error = "无法创建 PDF 解析上下文";
        return false;
    }

    std::string outputPath = output.u8string();
    char message[512] = {};

    fz_try(ctx) {
        fz_register_document_handlers(ctx);
        pdf_document* destination = pdf_create_document(ctx);

        for (const auto& input : inputs) {
#if defined(_WIN32)
            fz_stream* stream = fz_open_file_w(ctx, input.wstring().c_str());
#else
            fz_stream* stream = fz_open_file(ctx, input.string().c_str());
#endif
            pdf_document* source = pdf_open_document_with_stream(ctx, stream);
            const int pages = pdf_count_pages(ctx, source);
            for (int page = 0; page < pages; ++page) {
                pdf_graft_page(ctx, destination, -1, source, page);
            }
            pdf_drop_document(ctx, source);
        }

        fz_output* out = fz_new_output_with_path(ctx, outputPath.c_str(), 0);
        pdf_write_document(ctx, destination, out, &pdf_default_write_options);
        fz_close_output(ctx, out);
        fz_drop_output(ctx, out);
        pdf_drop_document(ctx, destination);
    }
    fz_catch(ctx) {
        std::snprintf(message, sizeof(message), "%s", fz_caught_message(ctx));
        error = message;
        fz_drop_context(ctx);
        return false;
    }

    fz_drop_context(ctx);
    return true;
}

bool PrintPdfWithSystem(const std::filesystem::path& path, std::string& error) {
    error.clear();

#if defined(_WIN32)
    const std::wstring file = path.wstring();
    SHELLEXECUTEINFOW info{};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_NOCLOSEPROCESS;
    info.lpVerb = L"print";
    info.lpFile = file.c_str();
    info.nShow = SW_HIDE;

    if (!ShellExecuteExW(&info)) {
        error = "无法调用系统打印（错误码 " + std::to_string(GetLastError()) + "）";
        return false;
    }
    if (info.hProcess != nullptr) {
        CloseHandle(info.hProcess);
    }
    return true;
#else
    (void)path;
    error = "当前平台尚未实现 PDF 直接打印";
    return false;
#endif
}

}
