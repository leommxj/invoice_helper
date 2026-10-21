#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace invoice {

bool MergePdfFiles(const std::vector<std::filesystem::path>& inputs,
                   const std::filesystem::path& output,
                   std::string& error);

bool PrintPdfWithSystem(const std::filesystem::path& path, std::string& error);

}
