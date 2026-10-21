#include "parser/Invoice.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>

namespace invoice {
namespace {

bool IsSpace(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

}

bool ParseMoneyCents(const std::string& text, std::int64_t& cents) {
    std::string digits;
    bool negative = false;
    bool seenDot = false;
    int fractionDigits = 0;

    for (std::size_t i = 0; i < text.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        if (IsSpace(c) || c == ',' || c == '\'' || c == 0xA5) {
            continue;
        }
        if (c == '-' && digits.empty() && !negative) {
            negative = true;
            continue;
        }
        if (c >= '0' && c <= '9') {
            digits.push_back(static_cast<char>(c));
            if (seenDot) {
                ++fractionDigits;
            }
            continue;
        }
        if (c == '.' && !seenDot) {
            seenDot = true;
            continue;
        }
    }

    if (digits.empty()) {
        return false;
    }

    if (fractionDigits > 2) {
        const std::size_t wholeDigits = digits.size() - static_cast<std::size_t>(fractionDigits);
        const int nextDigit = digits[wholeDigits + 2] - '0';
        digits.resize(wholeDigits + 2);
        if (nextDigit >= 5) {
            int carry = 1;
            for (std::size_t i = digits.size(); i > 0 && carry != 0; --i) {
                int value = (digits[i - 1] - '0') + carry;
                digits[i - 1] = static_cast<char>('0' + (value % 10));
                carry = value / 10;
            }
            if (carry != 0) {
                digits.insert(digits.begin(), '1');
            }
        }
        fractionDigits = 2;
    }

    while (digits.size() < 3) {
        digits.insert(digits.begin(), '0');
    }
    if (fractionDigits < 2) {
        digits.append(static_cast<std::size_t>(2 - fractionDigits), '0');
    }

    const std::string whole = digits.substr(0, digits.size() - 2);
    const std::string fraction = digits.substr(digits.size() - 2);
    std::int64_t value = 0;
    for (char c : whole) {
        const int digit = c - '0';
        if (value > (std::numeric_limits<std::int64_t>::max() - digit) / 10) {
            return false;
        }
        value = value * 10 + digit;
    }
    value = value * 100 + std::stoi(fraction);
    cents = negative ? -value : value;
    return true;
}

std::string FormatMoney(std::int64_t cents) {
    const bool negative = cents < 0;
    const std::uint64_t value = static_cast<std::uint64_t>(negative ? -(cents + 1) : cents) + (negative ? 1 : 0);
    const std::uint64_t whole = value / 100;
    const unsigned fraction = static_cast<unsigned>(value % 100);

    std::string digits = std::to_string(whole);
    std::string grouped;
    for (std::size_t i = 0; i < digits.size(); ++i) {
        if (i != 0 && (digits.size() - i) % 3 == 0) {
            grouped.push_back(',');
        }
        grouped.push_back(digits[i]);
    }

    std::string result;
    if (negative) {
        result.push_back('-');
    }
    result += "¥";
    result += grouped;
    result.push_back('.');
    result.push_back(static_cast<char>('0' + fraction / 10));
    result.push_back(static_cast<char>('0' + fraction % 10));
    return result;
}

}
