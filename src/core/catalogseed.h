// SPDX-License-Identifier: MIT
#ifndef CATALOGSEED_H
#define CATALOGSEED_H

#include <QSqlDatabase>
#include <QString>

// Однократный импорт демо-каталога data/parking-zones.json в zones/spaces (#28).
// Выполняется в одной транзакции вместе с записью маркера settings.catalog_seed_version.
// Если маркер уже есть, каталог не перечитывается: профиль, сессии и история не меняются.
namespace CatalogSeed {

extern const char kVersionKey[];
extern const char kVersion[];

// true — каталог уже был или успешно импортирован; false — ошибка, текст в errorMessage,
// БД остаётся в прежнем состоянии.
bool ensureSeeded(QSqlDatabase database, const QString &jsonPath, QString *errorMessage);

} // namespace CatalogSeed

#endif // CATALOGSEED_H
