#pragma once

#include "parser/Invoice.h"

#include <filesystem>
#include <string>

namespace invoice {

bool ParsePdfInvoice(const std::filesystem::path& path, Invoice& invoice, std::string& error);

}
