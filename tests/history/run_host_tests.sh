#!/bin/sh
# Хостовый прогон проверок HistoryService (#22) на Qt 5 + SQLite и проверки SQL на Python.
# Не заменяет сборку в Aurora SDK и запуск в эмуляторе.
# QMAKE — путь к qmake Qt 5 (по умолчанию qmake из PATH).
set -eu

HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
BUILD=${BUILD_DIR:-"$ROOT/build-history-tests"}
QMAKE=${QMAKE:-qmake}

python3 "$HERE/test_history_sql.py"

mkdir -p "$BUILD"
cd "$BUILD"
"$QMAKE" "$HERE/history-tests.pro"
make -s
./tst_historyservice "$@"
