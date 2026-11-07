#!/bin/sh
# Хостовый прогон проверок подбора (#20): эталон Python, QtTest правил и сервиса.
# Не заменяет сборку в Aurora SDK. QMAKE — путь к qmake Qt 5 (по умолчанию из PATH).
set -eu

HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
BUILD=${BUILD_DIR:-"$ROOT/build-experience-tests"}
QMAKE=${QMAKE:-qmake}

python3 "$HERE/test_rules.py"

for name in experience-tests experience-service-tests; do
    mkdir -p "$BUILD/$name"
    (cd "$BUILD/$name" && "$QMAKE" "$HERE/$name.pro" && make -s)
done
"$BUILD/experience-tests/tst_recommendationrules"
"$BUILD/experience-service-tests/tst_recommendationservice"
