#include "parser/InvoiceParser.h"

#include "parser/OfdText.h"
#include "parser/PdfText.h"

#include <mupdf/fitz.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <initializer_list>
#include <map>
#include <regex>
#include <string>
#include <vector>

namespace invoice {
namespace {

struct LineRef {
    const TextLine* line = nullptr;
    int page = 0;
};

std::string Trim(const std::string& value) {
    std::size_t begin = 0;
    std::size_t end = value.size();
    while (begin < end) {
        const unsigned char c = static_cast<unsigned char>(value[begin]);
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
            break;
        }
        ++begin;
    }
    while (end > begin) {
        const unsigned char c = static_cast<unsigned char>(value[end - 1]);
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
            break;
        }
        --end;
    }
    return value.substr(begin, end - begin);
}

std::string Compact(const std::string& value) {
    std::string result;
    result.reserve(value.size());
    for (std::size_t i = 0; i < value.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(value[i]);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            continue;
        }
        if (c == 0xC2 && i + 1 < value.size() &&
            static_cast<unsigned char>(value[i + 1]) == 0xA0) {
            ++i;
            continue;
        }
        if (c == 0xE3 && i + 2 < value.size() &&
            static_cast<unsigned char>(value[i + 1]) == 0x80 &&
            static_cast<unsigned char>(value[i + 2]) == 0x80) {
            i += 2;
            continue;
        }
        if (c == 0xEF && i + 2 < value.size() &&
            static_cast<unsigned char>(value[i + 1]) == 0xBC &&
            static_cast<unsigned char>(value[i + 2]) == 0x9A) {
            result.push_back(':');
            i += 2;
            continue;
        }
        result.push_back(static_cast<char>(c));
    }
    return result;
}

bool StartsWith(const std::string& value, const std::string& prefix) {
    return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
}

bool Contains(const std::string& value, const std::string& needle) {
    return value.find(needle) != std::string::npos;
}

bool IsInvoiceTitleLine(const std::string& text) {
    const std::string compact = Compact(text);
    if (StartsWith(compact, "电子发票")) {
        return true;
    }
    if (StartsWith(compact, "增值税") && Contains(compact, "发票")) {
        return true;
    }
    if (StartsWith(compact, "机动车销售统一发票")) {
        return true;
    }
    if (StartsWith(compact, "二手车销售统一发票")) {
        return true;
    }
    return false;
}

double CenterY(const TextLine& line) {
    return (line.y0 + line.y1) * 0.5;
}

bool IsLabelLike(const std::string& text) {
    const std::string compact = Compact(text);
    return !compact.empty() && compact.back() == ':';
}

bool IsShortValue(const std::string& text) {
    return Compact(text).size() <= 3;
}

std::string ValueAfterColon(const std::string& text) {
    std::size_t position = text.rfind("：");
    std::size_t length = 3;
    if (position == std::string::npos) {
        position = text.rfind(':');
        length = 1;
    }
    if (position == std::string::npos) {
        return {};
    }
    return Trim(text.substr(position + length));
}

const LineRef* FindByPrefix(const std::vector<LineRef>& lines, const std::string& prefix) {
    const std::string wanted = Compact(prefix);
    for (const auto& ref : lines) {
        if (StartsWith(Compact(ref.line->text), wanted)) {
            return &ref;
        }
    }
    return nullptr;
}

const LineRef* FindByContains(const std::vector<LineRef>& lines, const std::string& needle) {
    const std::string wanted = Compact(needle);
    for (const auto& ref : lines) {
        if (Contains(Compact(ref.line->text), wanted)) {
            return &ref;
        }
    }
    return nullptr;
}

bool FindValueRight(const std::vector<LineRef>& lines,
                    const LineRef& label,
                    std::string& value) {
    const std::string inlineValue = ValueAfterColon(label.line->text);
    if (!inlineValue.empty() && !IsShortValue(inlineValue)) {
        value = inlineValue;
        return true;
    }

    const double labelY = CenterY(*label.line);
    const TextLine* best = nullptr;
    double bestDistance = 1.0e9;

    for (const auto& ref : lines) {
        if (ref.page != label.page || ref.line == label.line) {
            continue;
        }
        if (std::abs(CenterY(*ref.line) - labelY) > 5.0) {
            continue;
        }
        if (ref.line->x0 <= label.line->x0 + 1.0) {
            continue;
        }
        const double distance = std::abs(ref.line->x0 - label.line->x1);
        if (distance >= bestDistance) {
            continue;
        }
        if (IsLabelLike(ref.line->text) || IsShortValue(ref.line->text)) {
            continue;
        }
        best = ref.line;
        bestDistance = distance;
    }

    if (best != nullptr) {
        value = Trim(best->text);
        return !value.empty();
    }

    const TextLine* next = nullptr;
    double bestVertical = 1.0e9;
    for (const auto& ref : lines) {
        if (ref.page != label.page || ref.line == label.line) {
            continue;
        }
        if (ref.line->y0 < label.line->y1 - 1.0 || ref.line->y0 > label.line->y1 + 20.0) {
            continue;
        }
        if (ref.line->x0 < label.line->x0 - 4.0) {
            continue;
        }
        if (IsLabelLike(ref.line->text) || IsShortValue(ref.line->text)) {
            continue;
        }
        const double distance = ref.line->y0 - label.line->y1;
        if (distance < bestVertical) {
            bestVertical = distance;
            next = ref.line;
        }
    }
    if (next != nullptr) {
        value = Trim(next->text);
        return !value.empty();
    }
    return false;
}

bool FindLabelValue(const std::vector<LineRef>& lines,
                    const std::string& label,
                    std::string& value) {
    const LineRef* ref = FindByPrefix(lines, label);
    return ref != nullptr && FindValueRight(lines, *ref, value);
}

std::string NormalizeDate(const std::string& text) {
    std::smatch match;
    const std::regex chinese(R"((\d{4})\s*年\s*(\d{1,2})\s*月\s*(\d{1,2})\s*日)");
    const std::string compact = Compact(text);
    if (std::regex_search(compact, match, chinese)) {
        char buffer[16] = {};
        std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d",
                      std::stoi(match[1].str()), std::stoi(match[2].str()),
                      std::stoi(match[3].str()));
        return buffer;
    }

    const std::regex numeric(R"((\d{4})[-/.](\d{1,2})[-/.](\d{1,2}))");
    if (std::regex_search(compact, match, numeric)) {
        char buffer[16] = {};
        std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d",
                      std::stoi(match[1].str()), std::stoi(match[2].str()),
                      std::stoi(match[3].str()));
        return buffer;
    }
    return {};
}

std::string NormalizeInvoiceNumber(const std::string& text) {
    const std::string compact = Compact(text);
    std::smatch match;
    const std::regex number(R"((\d{8,20}))");
    if (std::regex_search(compact, match, number)) {
        return match[1].str();
    }
    return {};
}

bool IsNumericLike(const std::string& text) {
    const std::string compact = Compact(text);
    if (compact == "***") {
        return true;
    }
    if (!compact.empty() && compact.back() == '%') {
        const std::string body = compact.substr(0, compact.size() - 1);
        return !body.empty() &&
               std::all_of(body.begin(), body.end(), [](unsigned char c) {
                   return (c >= '0' && c <= '9') || c == '.';
               });
    }
    const std::regex number(R"(^-?[0-9][0-9,]*(\.[0-9]+)?$)");
    return std::regex_match(compact, number);
}

std::string CleanNumberToken(const std::string& text) {
    std::string result;
    for (unsigned char c : text) {
        if ((c >= '0' && c <= '9') || c == '.' || c == '-' || c == '%' || c == '*') {
            result.push_back(static_cast<char>(c));
        }
    }
    return result;
}

bool IsIntegerToken(const std::string& text) {
    return !text.empty() &&
           std::all_of(text.begin(), text.end(), [](unsigned char c) {
               return c >= '0' && c <= '9';
           });
}

bool ContainsChineseAmount(const std::string& text) {
    static const char* const characters[] = {
        "零", "壹", "贰", "叁", "肆", "伍", "陆", "柒", "捌", "玖",
        "拾", "佰", "仟", "万", "亿", "元", "圆", "角", "分", "整"};
    for (const char* character : characters) {
        if (Contains(text, character)) {
            return true;
        }
    }
    return false;
}

bool IsRemarkHeader(const std::string& text) {
    const std::string compact = Compact(text);
    return compact.empty() || compact == "备" || compact == "注" ||
           compact == "备注" || Contains(compact, "下载次数") ||
           StartsWith(compact, "开票人") || StartsWith(compact, "价税合计") ||
           StartsWith(compact, "合计");
}

struct Candidate {
    const LineRef* ref = nullptr;
    std::string value;
};

std::vector<Candidate> CollectLabelValues(const std::vector<LineRef>& lines,
                                          const std::function<bool(const std::string&)>& match) {
    std::vector<Candidate> result;
    for (const auto& ref : lines) {
        if (!match(Compact(ref.line->text))) {
            continue;
        }
        std::string value;
        if (FindValueRight(lines, ref, value)) {
            result.push_back(Candidate{&ref, value});
        }
    }
    std::sort(result.begin(), result.end(), [](const Candidate& left, const Candidate& right) {
        if (left.ref->page != right.ref->page) {
            return left.ref->page < right.ref->page;
        }
        if (std::abs(left.ref->line->x0 - right.ref->line->x0) > 1.0) {
            return left.ref->line->x0 < right.ref->line->x0;
        }
        return left.ref->line->y0 < right.ref->line->y0;
    });
    return result;
}

std::string FormatAmount(const std::string& text) {
    std::int64_t cents = 0;
    if (ParseMoneyCents(text, cents)) {
        return FormatMoney(cents);
    }
    return Trim(text);
}

std::string NormalizeXmlKey(const std::string& key) {
    std::string result;
    const std::size_t colon = key.rfind(':');
    const std::size_t start = colon == std::string::npos ? 0 : colon + 1;
    for (std::size_t i = start; i < key.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(key[i]);
        if (c == '_' || c == '-' || c == '.' || c == '{' || c == '}') {
            continue;
        }
        result.push_back(c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : static_cast<char>(c));
    }
    return result;
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

void CollectXmlFields(fz_xml* node, std::map<std::string, std::string>& fields) {
    for (; node != nullptr; node = fz_xml_next(node)) {
        const char* tag = fz_xml_tag(node);
        if (tag != nullptr) {
            std::string text;
            AppendXmlText(node, text);
            const std::string key = NormalizeXmlKey(tag);
            if (!key.empty() && !Trim(text).empty()) {
                fields.emplace(key, Trim(text));
            }
            CollectXmlFields(fz_xml_down(node), fields);
        }
    }
}

std::string FindXmlField(const std::map<std::string, std::string>& fields,
                         std::initializer_list<const char*> names) {
    for (const char* name : names) {
        const auto it = fields.find(NormalizeXmlKey(name));
        if (it != fields.end()) {
            return it->second;
        }
    }
    return {};
}

}

void ParseInvoiceLines(std::vector<LineRef>& lines, Invoice& invoice);

bool ParsePdfInvoice(const std::filesystem::path& path, Invoice& invoice, std::string& error) {
    invoice = Invoice{};
    error.clear();

    std::vector<TextPage> pages;
    if (!ExtractPdfText(path, pages, error)) {
        return false;
    }

    std::vector<LineRef> lines;
    for (std::size_t pageIndex = 0; pageIndex < pages.size(); ++pageIndex) {
        for (const auto& line : pages[pageIndex].lines) {
            lines.push_back(LineRef{&line, static_cast<int>(pageIndex)});
        }
    }
    if (lines.empty()) {
        error = "PDF 中没有可提取的文本层";
        return false;
    }

    ParseInvoiceLines(lines, invoice);
    return true;
}

void ParseInvoiceLines(std::vector<LineRef>& lines, Invoice& invoice) {
    std::sort(lines.begin(), lines.end(), [](const LineRef& left, const LineRef& right) {
        if (left.page != right.page) {
            return left.page < right.page;
        }
        if (std::abs(CenterY(*left.line) - CenterY(*right.line)) > 1.0) {
            return CenterY(*left.line) < CenterY(*right.line);
        }
        return left.line->x0 < right.line->x0;
    });

    for (const auto& ref : lines) {
        if (!invoice.rawText.empty()) {
            invoice.rawText += "\n";
        }
        invoice.rawText += ref.line->text;
    }

    for (const auto& ref : lines) {
        std::string text = Trim(ref.line->text);
        if (invoice.title.empty() && IsInvoiceTitleLine(text)) {
            const std::size_t numberPosition = text.find("发票号码");
            if (numberPosition != std::string::npos) {
                text = Trim(text.substr(0, numberPosition));
            }
            invoice.title = text;
        }
        if (invoice.tag.empty() &&
            (Contains(text, "差额征税") || Contains(text, "红字") || Contains(text, "通行费"))) {
            invoice.tag = text;
        }
    }

    const LineRef* numberLabel = FindByContains(lines, "发票号码");
    if (numberLabel != nullptr) {
        invoice.hasInvoiceNumberLabel = true;
        std::string numberValue;
        if (FindValueRight(lines, *numberLabel, numberValue)) {
            invoice.number = NormalizeInvoiceNumber(numberValue);
            invoice.numberFromLabel = !invoice.number.empty();
        }
    }

    std::string dateValue;
    if (FindLabelValue(lines, "开票日期", dateValue)) {
        invoice.issueDate = NormalizeDate(dateValue);
    }
    if (invoice.issueDate.empty()) {
        for (const auto& ref : lines) {
            invoice.issueDate = NormalizeDate(ref.line->text);
            if (!invoice.issueDate.empty()) {
                break;
            }
        }
    }

    const auto names = CollectLabelValues(lines, [](const std::string& text) {
        return StartsWith(text, "名称:");
    });
    if (!names.empty()) {
        invoice.buyer.name = names[0].value;
    }
    if (names.size() > 1) {
        invoice.seller.name = names[1].value;
    }
    if (invoice.buyer.name.empty()) {
        const auto buyerNames = CollectLabelValues(lines, [](const std::string& text) {
            return StartsWith(text, "购买方名称:") || StartsWith(text, "购方名称:");
        });
        if (!buyerNames.empty()) {
            invoice.buyer.name = buyerNames.front().value;
        }
    }
    if (invoice.seller.name.empty()) {
        const auto sellerNames = CollectLabelValues(lines, [](const std::string& text) {
            return StartsWith(text, "销售方名称:") || StartsWith(text, "销方名称:");
        });
        if (!sellerNames.empty()) {
            invoice.seller.name = sellerNames.front().value;
        }
    }

    const auto taxIds = CollectLabelValues(lines, [](const std::string& text) {
        return Contains(text, "统一社会信用代码") || Contains(text, "纳税人识别号");
    });
    if (!taxIds.empty()) {
        invoice.buyer.taxId = taxIds[0].value;
    }
    if (taxIds.size() > 1) {
        invoice.seller.taxId = taxIds[1].value;
    }

    const LineRef* header = nullptr;
    for (const auto& ref : lines) {
        const std::string text = Compact(ref.line->text);
        if (Contains(text, "项目名称")) {
            header = &ref;
            break;
        }
    }

    const LineRef* totalLabel = nullptr;
    if (header != nullptr) {
        for (const auto& ref : lines) {
            if (ref.page != header->page) {
                continue;
            }
            const std::string text = Compact(ref.line->text);
            if (text == "合计" || (StartsWith(text, "合计") && !Contains(text, "价税合计"))) {
                totalLabel = &ref;
                break;
            }
        }
    }
    if (header != nullptr && totalLabel != nullptr) {
        const double headerY = CenterY(*header->line);
        const double totalY = CenterY(*totalLabel->line);
        std::vector<const LineRef*> itemLines;
        for (const auto& ref : lines) {
            if (ref.page != header->page) {
                continue;
            }
            const double y = CenterY(*ref.line);
            if (y > headerY + 3.0 && y < totalY - 3.0) {
                itemLines.push_back(&ref);
            }
        }
        std::sort(itemLines.begin(), itemLines.end(),
                  [](const LineRef* left, const LineRef* right) {
                      const double leftY = CenterY(*left->line);
                      const double rightY = CenterY(*right->line);
                      if (std::abs(leftY - rightY) > 1.0) {
                          return leftY < rightY;
                      }
                      return left->line->x0 < right->line->x0;
                  });
        std::vector<std::vector<const LineRef*>> groups;
        double groupY = -1.0e9;
        for (const LineRef* ref : itemLines) {
            const double y = CenterY(*ref->line);
            if (groups.empty() || std::abs(y - groupY) > 5.0) {
                groups.emplace_back();
                groupY = y;
            }
            groups.back().push_back(ref);
        }

        double lastItemY = -1.0e9;
        for (const auto& group : groups) {
            const TextLine* nameLine = nullptr;
            std::vector<const LineRef*> numericRefs;
            for (const LineRef* ref : group) {
                const std::string text = Trim(ref->line->text);
                if (IsNumericLike(text)) {
                    numericRefs.push_back(ref);
                } else if (!text.empty() &&
                           (!nameLine || ref->line->x0 < nameLine->x0)) {
                    nameLine = ref->line;
                }
            }
            if (numericRefs.empty()) {
                if (nameLine != nullptr && !invoice.items.empty() &&
                    std::abs(CenterY(*nameLine) - lastItemY) < 35.0 &&
                    nameLine->x0 < 150.0) {
                    invoice.items.back().name += Trim(nameLine->text);
                }
                continue;
            }

            InvoiceItem item;
            if (nameLine != nullptr) {
                item.name = Trim(nameLine->text);
            }

            std::sort(numericRefs.begin(), numericRefs.end(),
                      [](const LineRef* left, const LineRef* right) {
                          return left->line->x0 < right->line->x0;
                      });

            std::string taxRate;
            std::vector<std::string> numbers;
            for (const LineRef* ref : numericRefs) {
                const std::string token = CleanNumberToken(ref->line->text);
                if (token == "***" || (!token.empty() && token.back() == '%')) {
                    taxRate = token;
                } else if (!token.empty()) {
                    numbers.push_back(token);
                }
            }

            std::vector<const TextLine*> extraText;
            if (nameLine != nullptr && !numericRefs.empty()) {
                const double numericLeft = numericRefs.front()->line->x0;
                for (const LineRef* ref : group) {
                    if (ref->line != nameLine && !IsNumericLike(ref->line->text) &&
                        ref->line->x0 > nameLine->x1 - 1.0 &&
                        ref->line->x0 < numericLeft) {
                        extraText.push_back(ref->line);
                    }
                }
                std::sort(extraText.begin(), extraText.end(),
                          [](const TextLine* left, const TextLine* right) {
                              return left->x0 < right->x0;
                          });
                if (!extraText.empty()) {
                    item.specification = Trim(extraText[0]->text);
                }
                if (extraText.size() > 1) {
                    item.unit = Trim(extraText[1]->text);
                }
            }
            const std::size_t count = numbers.size();
            if (count >= 4) {
                item.quantity = numbers[count - 4];
                item.unitPrice = numbers[count - 3];
                item.amount = numbers[count - 2];
                item.taxAmount = numbers[count - 1];
            } else if (count == 3) {
                if (IsIntegerToken(numbers[0])) {
                    item.quantity = numbers[0];
                    item.amount = numbers[1];
                } else {
                    item.unitPrice = numbers[0];
                    item.amount = numbers[1];
                }
                item.taxAmount = numbers[2];
            } else if (count == 2) {
                item.amount = numbers[0];
                item.taxAmount = numbers[1];
            } else if (count == 1) {
                item.amount = numbers[0];
            }
            item.taxRate = taxRate;
            invoice.items.push_back(std::move(item));
            lastItemY = CenterY(nameLine != nullptr ? *nameLine : *numericRefs.front()->line);
        }
    }
    if (totalLabel == nullptr) {
        for (const auto& ref : lines) {
            const std::string text = Compact(ref.line->text);
            if (text == "合计" || text == "合" || text == "计" ||
                (StartsWith(text, "合计") && !Contains(text, "价税合计"))) {
                totalLabel = &ref;
                break;
            }
        }
    }

    if (totalLabel != nullptr) {
        struct Amount {
            double x = 0.0;
            std::string formatted;
        };
        std::vector<Amount> amounts;
        const double labelY = CenterY(*totalLabel->line);
        for (const auto& ref : lines) {
            if (ref.page != totalLabel->page || std::abs(CenterY(*ref.line) - labelY) > 3.0) {
                continue;
            }
            if (ref.line->x0 <= totalLabel->line->x1 - 2.0) {
                continue;
            }
            std::int64_t cents = 0;
            if (ParseMoneyCents(ref.line->text, cents)) {
                amounts.push_back(Amount{ref.line->x0, FormatMoney(cents)});
            }
        }
        std::sort(amounts.begin(), amounts.end(),
                  [](const Amount& left, const Amount& right) { return left.x < right.x; });
        if (!amounts.empty()) {
            invoice.totalAmount = amounts[0].formatted;
        }
        if (amounts.size() > 1) {
            invoice.totalTax = amounts[1].formatted;
        }
    }

    const LineRef* totalWithTaxLabel = FindByContains(lines, "价税合计");
    const LineRef* smallCaseLabel = FindByContains(lines, "（小写）");
    std::string totalWithTaxValue;
    if (smallCaseLabel != nullptr && FindValueRight(lines, *smallCaseLabel, totalWithTaxValue)) {
        invoice.totalWithTax = FormatAmount(totalWithTaxValue);
    }
    if (invoice.totalWithTax.empty() && totalWithTaxLabel != nullptr) {
        const double labelY = CenterY(*totalWithTaxLabel->line);
        for (const auto& ref : lines) {
            if (ref.page != totalWithTaxLabel->page ||
                std::abs(CenterY(*ref.line) - labelY) > 12.0) {
                continue;
            }
            if (!Contains(ref.line->text, "¥") && !Contains(ref.line->text, "￥")) {
                continue;
            }
            std::int64_t cents = 0;
            if (ParseMoneyCents(ref.line->text, cents)) {
                invoice.totalWithTax = FormatMoney(cents);
                break;
            }
        }
    }
    if (!invoice.totalWithTax.empty()) {
        std::int64_t cents = 0;
        if (ParseMoneyCents(invoice.totalWithTax, cents)) {
            invoice.totalWithTaxCents = cents;
            invoice.hasTotalWithTax = true;
        }
    }

    if (totalWithTaxLabel != nullptr) {
        const double labelY = CenterY(*totalWithTaxLabel->line);
        for (const auto& ref : lines) {
            if (ref.page != totalWithTaxLabel->page ||
                std::abs(CenterY(*ref.line) - labelY) > 14.0) {
                continue;
            }
            if (ref.line == totalWithTaxLabel->line) {
                continue;
            }
            if (ContainsChineseAmount(ref.line->text) &&
                !Contains(Compact(ref.line->text), "价税合计")) {
                invoice.amountInWords = Trim(ref.line->text);
                break;
            }
        }
    }

    std::string issuerValue;
    if (FindLabelValue(lines, "开票人", issuerValue)) {
        invoice.issuer = issuerValue;
    }

    if (totalWithTaxLabel != nullptr || totalLabel != nullptr) {
        const LineRef* top = totalWithTaxLabel != nullptr ? totalWithTaxLabel : totalLabel;
        double topY = top->line->y1;
        const LineRef* issuerLabel = FindByPrefix(lines, "开票人");
        double bottomY = 1.0e9;
        if (issuerLabel != nullptr && issuerLabel->page == top->page) {
            bottomY = issuerLabel->line->y0;
        }
        std::vector<std::string> remarks;
        for (const auto& ref : lines) {
            if (ref.page != top->page) {
                continue;
            }
            if (ref.line->y0 <= topY + 1.0 || ref.line->y1 >= bottomY - 1.0) {
                continue;
            }
            if (IsRemarkHeader(ref.line->text)) {
                continue;
            }
            remarks.push_back(Trim(ref.line->text));
        }
        for (const auto& remark : remarks) {
            if (remark.empty()) {
                continue;
            }
            if (!invoice.remarks.empty()) {
                invoice.remarks += "\n";
            }
            invoice.remarks += remark;
        }
    }

    if (invoice.numberFromLabel && invoice.hasTotalWithTax) {
        invoice.parsed = true;
    }
}

bool ParseOfdInvoice(const std::filesystem::path& path, Invoice& invoice, std::string& error) {
    invoice = Invoice{};
    error.clear();

    std::vector<TextPage> pages;
    if (!ExtractOfdText(path, pages, error)) {
        return false;
    }

    std::vector<LineRef> lines;
    for (std::size_t pageIndex = 0; pageIndex < pages.size(); ++pageIndex) {
        for (const auto& line : pages[pageIndex].lines) {
            lines.push_back(LineRef{&line, static_cast<int>(pageIndex)});
        }
    }
    if (lines.empty()) {
        error = "OFD 中没有可提取的文本";
        return false;
    }

    ParseInvoiceLines(lines, invoice);
    return true;
}

bool ParseXmlInvoice(const std::filesystem::path& path, Invoice& invoice, std::string& error) {
    invoice = Invoice{};
    error.clear();

    fz_context* ctx = fz_new_context(nullptr, nullptr, FZ_STORE_UNLIMITED);
    if (ctx == nullptr) {
        error = "无法创建 XML 解析上下文";
        return false;
    }

    fz_stream* stream = nullptr;
    fz_buffer* buffer = nullptr;
    fz_xml* xml = nullptr;
    std::map<std::string, std::string> fields;
    char message[512] = {};

    fz_try(ctx) {
#if defined(_WIN32)
        stream = fz_open_file_w(ctx, path.wstring().c_str());
#else
        stream = fz_open_file(ctx, path.string().c_str());
#endif
        buffer = fz_read_all(ctx, stream, 0);
        fz_drop_stream(ctx, stream);
        stream = nullptr;

        xml = fz_parse_xml(ctx, buffer, 0);
        fz_drop_buffer(ctx, buffer);
        buffer = nullptr;

        if (xml != nullptr) {
            fz_xml* root = fz_xml_root(xml);
            if (root != nullptr) {
                CollectXmlFields(root, fields);
            }
        }
    }
    fz_catch(ctx) {
        std::snprintf(message, sizeof(message), "%s", fz_caught_message(ctx));
        error = message;
    }

    if (xml != nullptr) {
        fz_drop_xml(ctx, xml);
    }
    if (buffer != nullptr) {
        fz_drop_buffer(ctx, buffer);
    }
    if (stream != nullptr) {
        fz_drop_stream(ctx, stream);
    }
    fz_drop_context(ctx);

    if (!error.empty()) {
        return false;
    }
    if (fields.empty()) {
        return true;
    }

    const std::string title = FindXmlField(fields, {
        "einvoicetype", "invoicetitle", "invoicetype", "title", "发票类型", "票种"});
    if (!title.empty()) {
        const std::size_t electronic = title.find("电子发票");
        invoice.title = electronic != std::string::npos && electronic > 0
            ? title.substr(electronic)
            : title;
    }

    const std::string number = FindXmlField(fields, {
        "eiid", "invoicenumber", "invoiceno", "invoicenum", "invoiceid", "fphm", "发票号码"});
    if (!number.empty()) {
        invoice.hasInvoiceNumberLabel = true;
        invoice.number = NormalizeInvoiceNumber(number);
        invoice.numberFromLabel = !invoice.number.empty();
    }

    const std::string date = FindXmlField(fields, {
        "issuetime", "issuedate", "invoicedate", "kprq", "开票日期", "rq"});
    if (!date.empty()) {
        invoice.issueDate = NormalizeDate(date);
    }

    const std::string totalWithTax = FindXmlField(fields, {
        "totaltaxincludedamount", "totalwithtax", "totalamountwithtax",
        "amountwithtax", "grandtotal", "jshj", "价税合计"});
    const std::string totalAmount = FindXmlField(fields, {
        "totalamwithouttax", "totalamount", "hjje", "合计金额"});
    const std::string totalTax = FindXmlField(fields, {
        "totaltaxam", "totaltax", "taxamount", "se", "税额"});

    std::int64_t amountCents = 0;
    std::int64_t taxCents = 0;
    const bool hasAmount = ParseMoneyCents(totalAmount, amountCents);
    const bool hasTax = ParseMoneyCents(totalTax, taxCents);
    if (!totalAmount.empty()) {
        invoice.totalAmount = FormatAmount(totalAmount);
    }
    if (!totalTax.empty()) {
        invoice.totalTax = FormatAmount(totalTax);
    }

    std::int64_t totalCents = 0;
    if (!totalWithTax.empty() && ParseMoneyCents(totalWithTax, totalCents)) {
        invoice.totalWithTax = FormatMoney(totalCents);
        invoice.totalWithTaxCents = totalCents;
        invoice.hasTotalWithTax = true;
    } else if (hasAmount && hasTax) {
        totalCents = amountCents + taxCents;
        invoice.totalWithTax = FormatMoney(totalCents);
        invoice.totalWithTaxCents = totalCents;
        invoice.hasTotalWithTax = true;
    }

    const std::string buyerName = FindXmlField(fields, {
        "buyername", "purchasername", "gmfmc", "购买方名称", "购方名称"});
    if (!buyerName.empty()) {
        invoice.buyer.name = buyerName;
    }
    const std::string sellerName = FindXmlField(fields, {
        "sellername", "salername", "xsfmc", "销售方名称", "销方名称"});
    if (!sellerName.empty()) {
        invoice.seller.name = sellerName;
    }
    const std::string buyerTaxId = FindXmlField(fields, {
        "buyeridnum", "buyertaxid", "purchasertaxid", "gmfnsrsbh",
        "购买方税号", "购买方纳税人识别号"});
    if (!buyerTaxId.empty()) {
        invoice.buyer.taxId = buyerTaxId;
    }
    const std::string sellerTaxId = FindXmlField(fields, {
        "selleridnum", "sellertaxid", "salestaxid", "xsfnsrsbh",
        "销售方税号", "销售方纳税人识别号"});
    if (!sellerTaxId.empty()) {
        invoice.seller.taxId = sellerTaxId;
    }

    const std::string issuer = FindXmlField(fields, {
        "drawer", "issuer", "kpr", "开票人"});
    if (!issuer.empty()) {
        invoice.issuer = issuer;
    }
    const std::string amountInWords = FindXmlField(fields, {
        "totaltaxincludedamountinchinese", "amountinwords", "totalamountinwords",
        "大写金额", "价税合计大写"});
    if (!amountInWords.empty()) {
        invoice.amountInWords = amountInWords;
    }
    const std::string remarks = FindXmlField(fields, {
        "remarks", "remark", "bz", "备注"});
    if (!remarks.empty()) {
        invoice.remarks = remarks;
    }

    if (invoice.numberFromLabel && invoice.hasTotalWithTax) {
        invoice.parsed = true;
    }
    return true;
}

}
