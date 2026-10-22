#include "parser/OfdText.h"

#include <mupdf/fitz.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace invoice {
namespace {

struct TextCode {
    std::string text;
    double x = 0.0;
    double y = 0.0;
    double size = 10.0;
    double width = 1.0;
};

std::string LowerAscii(std::string value) {
    for (char& c : value) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return value;
}

std::string LocalTag(const char* tag) {
    if (tag == nullptr) {
        return {};
    }
    std::string name(tag);
    const std::size_t colon = name.rfind(':');
    if (colon != std::string::npos) {
        name = name.substr(colon + 1);
    }
    return LowerAscii(name);
}

void AppendXmlText(fz_xml* node, std::string& text) {
    if (node == nullptr) {
        return;
    }
    if (fz_xml_tag(node) == nullptr) {
        const char* value = fz_xml_text(node);
        if (value != nullptr) {
            text += value;
        }
        return;
    }
    for (fz_xml* child = fz_xml_down(node); child != nullptr; child = fz_xml_next(child)) {
        AppendXmlText(child, text);
    }
}

double AttributeNumber(fz_xml* node, const char* name, double fallback) {
    const char* value = fz_xml_att(node, name);
    if (value == nullptr) {
        return fallback;
    }
    char* end = nullptr;
    const double number = std::strtod(value, &end);
    return end != value ? number : fallback;
}

double TextWidth(fz_xml* node, const std::string& text, double size) {
    const char* delta = fz_xml_att(node, "DeltaX");
    if (delta != nullptr && *delta != '\0') {
        double width = 0.0;
        const char* cursor = delta;
        while (*cursor != '\0') {
            char* end = nullptr;
            const double value = std::strtod(cursor, &end);
            if (end == cursor) {
                break;
            }
            width += value;
            cursor = end;
            while (*cursor == ' ') {
                ++cursor;
            }
        }
        if (width > 0.0) {
            return width;
        }
    }
    return std::max(1.0, size * 0.5 * static_cast<double>(text.size()));
}

void CollectTextCodes(fz_xml* node, double parentSize, std::vector<TextCode>& codes) {
    for (; node != nullptr; node = fz_xml_next(node)) {
        const char* tag = fz_xml_tag(node);
        if (tag != nullptr && LocalTag(tag) == "textcode") {
            std::string text;
            AppendXmlText(node, text);
            if (!text.empty()) {
                const double size = AttributeNumber(node, "Size", parentSize);
                codes.push_back(TextCode{
                    text,
                    AttributeNumber(node, "X", 0.0),
                    AttributeNumber(node, "Y", 0.0),
                    size,
                    TextWidth(node, text, size)});
            }
        } else {
            CollectTextCodes(fz_xml_down(node), parentSize, codes);
        }
    }
}

void CollectTextObjects(fz_xml* node, std::vector<std::vector<TextCode>>& objects) {
    for (; node != nullptr; node = fz_xml_next(node)) {
        const char* tag = fz_xml_tag(node);
        if (tag != nullptr && LocalTag(tag) == "textobject") {
            std::vector<TextCode> codes;
            const double size = AttributeNumber(node, "Size", 10.0);
            CollectTextCodes(fz_xml_down(node), size, codes);
            if (!codes.empty()) {
                objects.push_back(std::move(codes));
            }
        } else {
            CollectTextObjects(fz_xml_down(node), objects);
        }
    }
}

void AppendLine(TextPage& page, std::vector<TextCode> line) {
    if (line.empty()) {
        return;
    }
    std::sort(line.begin(), line.end(), [](const TextCode& left, const TextCode& right) {
        return left.x < right.x;
    });

    TextLine output;
    double x0 = line.front().x;
    double x1 = line.front().x;
    double y0 = -line.front().y;
    double y1 = -line.front().y;
    double size = line.front().size;
    for (const TextCode& code : line) {
        output.text += code.text;
        x0 = std::min(x0, code.x);
        x1 = std::max(x1, code.x + code.width);
        y0 = std::min(y0, -code.y);
        y1 = std::max(y1, -code.y);
        size = std::max(size, code.size);
    }
    output.x0 = x0;
    output.x1 = x1;
    output.y0 = y0 - size;
    output.y1 = y1;
    page.lines.push_back(std::move(output));
}

TextPage BuildPage(const std::vector<std::vector<TextCode>>& objects, int pageNumber) {
    TextPage page;
    page.number = pageNumber;

    for (const auto& object : objects) {
        std::vector<TextCode> codes = object;
        std::sort(codes.begin(), codes.end(), [](const TextCode& left, const TextCode& right) {
            if (left.y != right.y) {
                return left.y > right.y;
            }
            return left.x < right.x;
        });

        std::vector<TextCode> line;
        double lineY = 0.0;
        for (const TextCode& code : codes) {
            if (line.empty()) {
                lineY = code.y;
            } else if (std::abs(code.y - lineY) > 2.5) {
                AppendLine(page, std::move(line));
                line.clear();
                lineY = code.y;
            }
            line.push_back(code);
        }
        AppendLine(page, std::move(line));
    }

    std::sort(page.lines.begin(), page.lines.end(), [](const TextLine& left, const TextLine& right) {
        if (std::abs((left.y0 + left.y1) * 0.5 - (right.y0 + right.y1) * 0.5) > 1.0) {
            return (left.y0 + left.y1) * 0.5 < (right.y0 + right.y1) * 0.5;
        }
        return left.x0 < right.x0;
    });

    return page;
}

}

bool ExtractOfdText(const std::filesystem::path& path,
                    std::vector<TextPage>& pages,
                    std::string& error) {
    pages.clear();
    error.clear();

    fz_context* ctx = fz_new_context(nullptr, nullptr, FZ_STORE_UNLIMITED);
    if (ctx == nullptr) {
        error = "无法创建 OFD 解析上下文";
        return false;
    }

    fz_stream* stream = nullptr;
    fz_archive* archive = nullptr;
    char message[512] = {};

    fz_try(ctx) {
#if defined(_WIN32)
        stream = fz_open_file_w(ctx, path.wstring().c_str());
#else
        stream = fz_open_file(ctx, path.string().c_str());
#endif
        archive = fz_open_archive_with_stream(ctx, stream);

        std::vector<std::string> entries;
        const int count = fz_count_archive_entries(ctx, archive);
        for (int i = 0; i < count; ++i) {
            const char* name = fz_list_archive_entry(ctx, archive, i);
            if (name == nullptr) {
                continue;
            }
            const std::string entry(name);
            if (entry.size() >= 4 && LowerAscii(entry.substr(entry.size() - 4)) == ".xml") {
                entries.push_back(entry);
            }
        }
        std::sort(entries.begin(), entries.end());

        for (const std::string& entry : entries) {
            fz_xml* xml = fz_parse_xml_archive_entry(ctx, archive, entry.c_str(), 0);
            if (xml == nullptr) {
                continue;
            }

            std::vector<std::vector<TextCode>> objects;
            CollectTextObjects(fz_xml_root(xml), objects);
            fz_drop_xml(ctx, xml);

            if (!objects.empty()) {
                pages.push_back(BuildPage(objects, static_cast<int>(pages.size())));
            }
        }
    }
    fz_catch(ctx) {
        std::snprintf(message, sizeof(message), "%s", fz_caught_message(ctx));
        error = message;
    }

    if (archive != nullptr) {
        fz_drop_archive(ctx, archive);
    } else if (stream != nullptr) {
        fz_drop_stream(ctx, stream);
    }
    fz_drop_context(ctx);

    if (!error.empty()) {
        pages.clear();
        return false;
    }
    if (pages.empty()) {
        error = "OFD 中没有可提取的文本";
        return false;
    }
    return true;
}

}
