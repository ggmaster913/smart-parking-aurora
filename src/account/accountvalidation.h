// SPDX-License-Identifier: MIT
#ifndef ACCOUNTVALIDATION_H
#define ACCOUNTVALIDATION_H

#include <QString>

// Нормализация и проверка демо-ввода профиля по docs/CONTRACTS.md.
// Это проверка формата, а не подтверждение владения контактом.
namespace AccountValidation {

// Демо-код показывается на экране входа; SMS/письмо не отправляется.
QString demoCode();

// Email: trim, lower case, ровно один '@', непустые части, без пробелов, до 254 символов.
// Телефон: необязательный '+', убрать пробелы/скобки/дефисы, затем 10–15 цифр.
// Строка с '@' проверяется как email, иначе как телефон.
bool normalizeContact(const QString &input, QString *normalized);

bool isDemoCodeValid(const QString &code);

// Госномер: trim, upper case, 4–15 символов, без регионального формата.
bool normalizePlate(const QString &input, QString *normalized);

// Марка: trim, 1–60 символов.
bool normalizeBrand(const QString &input, QString *normalized);

// Имя профиля: trim, 1–100 символов.
bool normalizeDisplayName(const QString &input, QString *normalized);

} // namespace AccountValidation

#endif // ACCOUNTVALIDATION_H
