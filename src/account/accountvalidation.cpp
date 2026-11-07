// SPDX-License-Identifier: MIT
#include "accountvalidation.h"

namespace {

const int kMaxEmailLength = 254;
const int kMinPhoneDigits = 10;
const int kMaxPhoneDigits = 15;
const int kMinPlateLength = 4;
const int kMaxPlateLength = 15;
const int kMaxBrandLength = 60;
const int kMaxDisplayNameLength = 100;

bool isAsciiDigit(QChar c)
{
    return c >= QLatin1Char('0') && c <= QLatin1Char('9');
}

bool hasControlCharacters(const QString &value)
{
    for (int i = 0; i < value.size(); ++i) {
        if (value.at(i).category() == QChar::Other_Control)
            return true;
    }
    return false;
}

bool normalizeEmail(const QString &input, QString *normalized)
{
    const QString email = input.trimmed().toLower();
    if (email.isEmpty() || email.size() > kMaxEmailLength)
        return false;
    for (int i = 0; i < email.size(); ++i) {
        if (email.at(i).isSpace() || email.at(i).category() == QChar::Other_Control)
            return false;
    }
    if (email.count(QLatin1Char('@')) != 1)
        return false;
    const int at = email.indexOf(QLatin1Char('@'));
    if (at <= 0 || at >= email.size() - 1)
        return false;
    *normalized = email;
    return true;
}

bool normalizePhone(const QString &input, QString *normalized)
{
    QString compact;
    const QString trimmed = input.trimmed();
    for (int i = 0; i < trimmed.size(); ++i) {
        const QChar c = trimmed.at(i);
        if (c.isSpace() || c == QLatin1Char('(') || c == QLatin1Char(')')
                || c == QLatin1Char('-'))
            continue;
        compact.append(c);
    }

    QString digits = compact;
    const bool hasPlus = compact.startsWith(QLatin1Char('+'));
    if (hasPlus)
        digits = compact.mid(1);

    if (digits.size() < kMinPhoneDigits || digits.size() > kMaxPhoneDigits)
        return false;
    for (int i = 0; i < digits.size(); ++i) {
        if (!isAsciiDigit(digits.at(i)))
            return false;
    }
    *normalized = hasPlus ? QLatin1Char('+') + digits : digits;
    return true;
}

bool normalizeTrimmed(const QString &input, int minLength, int maxLength, bool upper,
                      QString *normalized)
{
    QString value = input.trimmed();
    if (upper)
        value = value.toUpper();
    if (value.size() < minLength || value.size() > maxLength || hasControlCharacters(value))
        return false;
    *normalized = value;
    return true;
}

} // namespace

namespace AccountValidation {

QString demoCode()
{
    return QStringLiteral("123456");
}

bool normalizeContact(const QString &input, QString *normalized)
{
    if (!normalized)
        return false;
    if (input.contains(QLatin1Char('@')))
        return normalizeEmail(input, normalized);
    return normalizePhone(input, normalized);
}

bool isDemoCodeValid(const QString &code)
{
    return code.trimmed() == demoCode();
}

bool normalizePlate(const QString &input, QString *normalized)
{
    return normalized
            && normalizeTrimmed(input, kMinPlateLength, kMaxPlateLength, true, normalized);
}

bool normalizeBrand(const QString &input, QString *normalized)
{
    return normalized && normalizeTrimmed(input, 1, kMaxBrandLength, false, normalized);
}

bool normalizeDisplayName(const QString &input, QString *normalized)
{
    return normalized && normalizeTrimmed(input, 1, kMaxDisplayNameLength, false, normalized);
}

} // namespace AccountValidation
