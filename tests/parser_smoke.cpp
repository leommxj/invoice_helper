#include "parser/InvoiceParser.h"

#include <filesystem>
#include <iostream>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: invoice_parser_smoke <invoice.pdf>\n";
        return 2;
    }

    invoice::Invoice parsed;
    std::string error;
    if (!invoice::ParsePdfInvoice(std::filesystem::path(argv[1]), parsed, error)) {
        std::cerr << "parse failed: " << error << "\n";
        return 1;
    }

    std::cout << "title: " << parsed.title << "\n"
              << "tag: " << parsed.tag << "\n"
              << "number: " << parsed.number << "\n"
              << "date: " << parsed.issueDate << "\n"
              << "buyer: " << parsed.buyer.name << " / " << parsed.buyer.taxId << "\n"
              << "seller: " << parsed.seller.name << " / " << parsed.seller.taxId << "\n"
              << "amount: " << parsed.totalAmount << "\n"
              << "tax: " << parsed.totalTax << "\n"
              << "total: " << parsed.totalWithTax << "\n"
              << "words: " << parsed.amountInWords << "\n"
              << "issuer: " << parsed.issuer << "\n"
              << "remarks: " << parsed.remarks << "\n";
    for (const auto& item : parsed.items) {
        std::cout << "item: " << item.name << " | qty=" << item.quantity
                  << " price=" << item.unitPrice << " amount=" << item.amount
                  << " rate=" << item.taxRate << " tax=" << item.taxAmount << "\n";
    }
    return 0;
}
