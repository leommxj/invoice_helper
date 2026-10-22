#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace invoice {

struct Party {
    std::string name;
    std::string taxId;
};

struct InvoiceItem {
    std::string name;
    std::string specification;
    std::string unit;
    std::string quantity;
    std::string unitPrice;
    std::string amount;
    std::string taxRate;
    std::string taxAmount;
};

struct Invoice {
    bool parsed = false;
    bool hasInvoiceNumberLabel = false;
    bool numberFromLabel = false;
    std::string title;
    std::string tag;
    std::string number;
    std::string issueDate;
    Party buyer;
    Party seller;
    std::vector<InvoiceItem> items;
    std::string totalAmount;
    std::string totalTax;
    std::string totalWithTax;
    std::string amountInWords;
    std::string issuer;
    std::string remarks;
    std::string rawText;
    std::int64_t totalWithTaxCents = 0;
    bool hasTotalWithTax = false;
};

bool ParseMoneyCents(const std::string& text, std::int64_t& cents);
std::string FormatMoney(std::int64_t cents);

}
