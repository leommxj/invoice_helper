#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace invoice {

struct TextLine {
    std::string text;
    double x0 = 0.0;
    double y0 = 0.0;
    double x1 = 0.0;
    double y1 = 0.0;
};

struct TextPage {
    int number = 0;
    double width = 0.0;
    double height = 0.0;
    std::vector<TextLine> lines;
};

bool ExtractPdfText(const std::filesystem::path& path,
                    std::vector<TextPage>& pages,
                    std::string& error);

}
