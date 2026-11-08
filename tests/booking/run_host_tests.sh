#!/bin/sh
# Хостовый прогон проверок BookingService (#9) на Qt 5 + SQLite.
# Не заменяет сборку в Aurora SDK и запуск в эмуляторе.
# QMAKE — путь к qmake Qt 5 (по умолчанию qmake из PATH).
# Пока миграция из #2 не влита в ветку, схема берётся из origin/feat/2-core-storage.
set -eu

HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
BUILD=${BUILD_DIR:-"$ROOT/build-booking-tests"}
QMAKE=${QMAKE:-qmake}

mkdir -p "$BUILD"
if [ -z "${BOOKING_SCHEMA_SQL:-}" ] && [ ! -f "$ROOT/migrations/001_initial.sql" ]; then
    git -C "$ROOT" show origin/feat/2-core-storage:migrations/001_initial.sql \
        > "$BUILD/001_initial.sql"
    BOOKING_SCHEMA_SQL="$BUILD/001_initial.sql"
    export BOOKING_SCHEMA_SQL
    echo "Схема: origin/feat/2-core-storage ($(git -C "$ROOT" rev-parse --short origin/feat/2-core-storage))"
fi

cd "$BUILD"
"$QMAKE" "$HERE/booking-tests.pro"
make -s
./tst_bookingservice "$@"
