#include "parser/PdfText.h"

#include <mupdf/fitz.h>

#include <cstdio>

namespace invoice {
namespace {

void AppendUtf8(std::string& out, int codepoint) {
    if (codepoint < 0) {
        return;
    }
    if (codepoint < 0x80) {
        out.push_back(static_cast<char>(codepoint));
    } else if (codepoint < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
}

}

bool ExtractPdfText(const std::filesystem::path& path,
                    std::vector<TextPage>& pages,
                    std::string& error) {
    pages.clear();
    error.clear();

    fz_context* ctx = fz_new_context(nullptr, nullptr, FZ_STORE_UNLIMITED);
    if (ctx == nullptr) {
        error = "无法创建 PDF 解析上下文";
        return false;
    }

    // These objects are intentionally created before fz_try: MuPDF reports
    // errors with longjmp, which must not skip a C++ destructor.
    std::string lineText;
    char message[512] = {};

    fz_try(ctx) {
        fz_register_document_handlers(ctx);

#if defined(_WIN32)
        fz_stream* stream = fz_open_file_w(ctx, path.wstring().c_str());
#else
        fz_stream* stream = fz_open_file(ctx, path.string().c_str());
#endif
        fz_document* document = fz_open_document_with_stream(ctx, "application/pdf", stream);
        const int pageCount = fz_count_pages(ctx, document);

        for (int pageIndex = 0; pageIndex < pageCount; ++pageIndex) {
            fz_page* page = fz_load_page(ctx, document, pageIndex);
            fz_rect bounds = fz_bound_page(ctx, page);
            fz_stext_page* text = fz_new_stext_page_from_page(ctx, page, nullptr);

            TextPage output;
            output.number = pageIndex;
            output.width = bounds.x1 - bounds.x0;
            output.height = bounds.y1 - bounds.y0;

            for (fz_stext_block* block = text->first_block; block != nullptr; block = block->next) {
                if (block->type != FZ_STEXT_BLOCK_TEXT) {
                    continue;
                }
                for (fz_stext_line* line = block->u.t.first_line; line != nullptr; line = line->next) {
                    lineText.clear();
                    for (fz_stext_char* ch = line->first_char; ch != nullptr; ch = ch->next) {
                        AppendUtf8(lineText, ch->c);
                    }
                    if (lineText.empty()) {
                        continue;
                    }
                    TextLine outputLine;
                    outputLine.text = lineText;
                    outputLine.x0 = line->bbox.x0;
                    outputLine.y0 = line->bbox.y0;
                    outputLine.x1 = line->bbox.x1;
                    outputLine.y1 = line->bbox.y1;
                    output.lines.push_back(std::move(outputLine));
                }
            }

            pages.push_back(std::move(output));
            fz_drop_stext_page(ctx, text);
            fz_drop_page(ctx, page);
        }

        fz_drop_document(ctx, document);
    }
    fz_catch(ctx) {
        std::snprintf(message, sizeof(message), "%s", fz_caught_message(ctx));
        error = message;
    }

    fz_drop_context(ctx);
    if (!error.empty()) {
        pages.clear();
        return false;
    }
    return true;
}

}
