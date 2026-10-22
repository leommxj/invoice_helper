#pragma once

#include "parser/PdfText.h"

#include <filesystem>
#include <string>
#include <vector>

namespace invoice {

bool ExtractOfdText(const std::filesystem::path& path,
                    std::vector<TextPage>& pages,
                    std::string& error);

}
