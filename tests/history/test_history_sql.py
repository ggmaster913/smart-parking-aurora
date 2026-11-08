#!/usr/bin/env python3
# Проверка SQL HistoryService (#22) на настоящей migrations/001_initial.sql.
# Запросы берутся из src/history/historyservice.cpp (метки HISTORY_SQL_BEGIN/END),
# правила отображения повторяют C++ (истёкшая active -> completed, предел суммы).
# Это проверка SQL и правил, а не сборки Qt/Аврора. Запуск: python3 tests/history/test_history_sql.py
import os
import re
import sqlite3
import sys
import unittest

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
MAX_TOTAL = 9007199254740991
NOW = 1762538400  # 2025-11-07 18:00:00 UTC


def read(*parts):
    with open(os.path.join(ROOT, *parts), encoding="utf-8") as f:
        return f.read()


def load_sql():
    text = read("src", "history", "historyservice.cpp")
    result = {}
    for name, block in re.findall(r"// HISTORY_SQL_BEGIN (\w+)\n(.*?)// HISTORY_SQL_END", text, re.S):
        result[name] = "".join(re.findall(r'"((?:[^"\\]|\\.)*)"', block))
    return result


SQL = load_sql()


def open_db():
    db = sqlite3.connect(":memory:")
    db.execute("PRAGMA foreign_keys = ON")
    db.executescript(read("migrations", "001_initial.sql"))
    db.executescript("""
        INSERT INTO users VALUES ('u1','u1@demo.test','Первый');
        INSERT INTO users VALUES ('u2','u2@demo.test','Второй');
        INSERT INTO vehicles VALUES ('v1','u1','А123ВС77','Лада');
        INSERT INTO vehicles VALUES ('v2','u2','В456ОР99','Камаз');
        INSERT INTO zones VALUES ('z-paid','Платная','Адрес 1',55.75,37.61,18000,'Круглосуточно');
        INSERT INTO zones VALUES ('z-free','Бесплатная','Адрес 2',55.76,37.62,0,'Круглосуточно');
        INSERT INTO spaces VALUES ('s1','z-paid','01',0);
        INSERT INTO spaces VALUES ('s2','z-paid','02',0);
        INSERT INTO spaces VALUES ('s3','z-free','01',0);
    """)
    return db


def add_session(db, sid, user, vehicle, space, start, minutes, amount, status="completed", plate="А123ВС77"):
    db.execute("INSERT INTO sessions VALUES (?,?,?,?,?,?,18000,?,?,?)",
               (sid, user, vehicle, space, start, start + minutes * 60, amount, status, plate))


def refresh(db, user, now=NOW):
    """Повторяет HistoryService::refresh: итоги по всем сессиям, список до 100, статус по UTC."""
    if not user:
        return {"ok": False, "errorCode": "NOT_AUTHENTICATED"}
    total, count = 0, 0
    for (amount,) in db.execute(SQL["amounts"], {"user_id": user}):
        if amount < 0 or amount > MAX_TOTAL - total:
            return {"ok": False, "errorCode": "STORAGE_ERROR"}
        total += amount
        count += 1
    entries = []
    for row in db.execute(SQL["entries"], {"user_id": user}):
        sid, zone_id, zone_name, address, plate, starts, expires, amount, status = row
        if status == "active" and expires <= now:
            status = "completed"
        entries.append({"id": sid, "zoneId": zone_id, "zoneName": zone_name, "address": address,
                        "plate": plate, "startsAtUtc": starts, "expiresAtUtc": expires,
                        "amountKopecks": amount, "status": status, "ticketNumber": "DEMO-" + sid})
    return {"ok": True, "entries": entries, "totalKopecks": total, "totalSessions": count}


class HistorySql(unittest.TestCase):
    def setUp(self):
        self.db = open_db()

    def tearDown(self):
        self.db.close()

    def test_sql_found_and_read_only(self):
        self.assertIn("entries", SQL)
        self.assertIn("amounts", SQL)
        for sql in SQL.values():
            self.assertTrue(sql.lstrip().upper().startswith("SELECT"), sql)
            self.assertNotRegex(sql.upper(), r"\b(INSERT|UPDATE|DELETE|REPLACE|DROP|ALTER)\b")
        add_session(self.db, "a", "u1", "v1", "s1", NOW - 7200, 60, 18000)
        before = self.db.total_changes
        refresh(self.db, "u1")
        self.assertEqual(self.db.total_changes, before, "история не должна менять БД")

    def test_empty_history(self):
        r = refresh(self.db, "u1")
        self.assertEqual((r["entries"], r["totalKopecks"], r["totalSessions"]), ([], 0, 0))

    def test_not_authenticated(self):
        self.assertEqual(refresh(self.db, "")["errorCode"], "NOT_AUTHENTICATED")

    def test_users_are_isolated(self):
        add_session(self.db, "a", "u1", "v1", "s1", NOW - 7200, 60, 18000)
        add_session(self.db, "b", "u2", "v2", "s2", NOW - 3600, 30, 9000, plate="В456ОР99")
        r1, r2 = refresh(self.db, "u1"), refresh(self.db, "u2")
        self.assertEqual([e["id"] for e in r1["entries"]], ["a"])
        self.assertEqual([e["id"] for e in r2["entries"]], ["b"])
        self.assertEqual((r1["totalKopecks"], r2["totalKopecks"]), (18000, 9000))
        self.assertEqual(refresh(self.db, "nobody")["entries"], [])

    def test_fields_join_and_ticket(self):
        add_session(self.db, "a", "u1", "v1", "s1", NOW - 7200, 60, 18000)
        e = refresh(self.db, "u1")["entries"][0]
        self.assertEqual((e["zoneId"], e["zoneName"], e["address"], e["plate"], e["ticketNumber"]),
                         ("z-paid", "Платная", "Адрес 1", "А123ВС77", "DEMO-a"))

    def test_order_starts_desc_then_id_asc(self):
        add_session(self.db, "b", "u1", "v1", "s1", NOW - 9000, 15, 4500)
        add_session(self.db, "c", "u1", "v1", "s2", NOW - 7200, 15, 4500)
        add_session(self.db, "a", "u1", "v1", "s3", NOW - 7200, 15, 0)
        self.assertEqual([e["id"] for e in refresh(self.db, "u1")["entries"]], ["a", "c", "b"])

    def test_extension_not_double_counted(self):
        # Бронь 45 мин (13500) + продление 15 мин (4500): сессия хранит накопленные 18000.
        add_session(self.db, "a", "u1", "v1", "s1", NOW - 7200, 60, 18000)
        self.db.execute("INSERT INTO payment_operations VALUES ('op1','u1','a','book','{}','{}',13500,?)", (NOW - 7200,))
        self.db.execute("INSERT INTO payment_operations VALUES ('op2','u1','a','extend','{}','{}',4500,?)", (NOW - 5000,))
        r = refresh(self.db, "u1")
        self.assertEqual((r["totalKopecks"], r["totalSessions"], r["entries"][0]["amountKopecks"]), (18000, 1, 18000))

    def test_free_parking(self):
        add_session(self.db, "f", "u1", "v1", "s3", NOW - 7200, 60, 0)
        r = refresh(self.db, "u1")
        self.assertEqual((r["totalKopecks"], r["totalSessions"], r["entries"][0]["amountKopecks"]), (0, 1, 0))

    def test_expired_active_shown_completed_without_write(self):
        add_session(self.db, "x", "u1", "v1", "s1", NOW - 3600, 30, 9000, status="active")
        add_session(self.db, "y", "u2", "v2", "s2", NOW - 600, 60, 18000, status="active", plate="В456ОР99")
        before = self.db.total_changes
        self.assertEqual(refresh(self.db, "u1")["entries"][0]["status"], "completed")
        self.assertEqual(refresh(self.db, "u2")["entries"][0]["status"], "active")
        self.assertEqual(self.db.execute("SELECT status FROM sessions WHERE id='x'").fetchone()[0], "active")
        self.assertEqual(self.db.total_changes, before)

    def test_more_than_100_totals_cover_all(self):
        for i in range(130):
            add_session(self.db, "s%03d" % i, "u1", "v1", "s1" if i % 2 else "s2", NOW - 100000 + i * 600, 15, 4500)
        r = refresh(self.db, "u1")
        self.assertEqual(len(r["entries"]), 100)
        self.assertEqual(r["totalSessions"], 130)
        self.assertEqual(r["totalKopecks"], 130 * 4500)
        self.assertEqual(r["entries"][0]["id"], "s129")

    def test_money_limit(self):
        add_session(self.db, "a", "u1", "v1", "s1", NOW - 7200, 15, MAX_TOTAL - 1)
        add_session(self.db, "b", "u1", "v1", "s2", NOW - 3600, 15, 1)
        r = refresh(self.db, "u1")
        self.assertEqual(r["totalKopecks"], MAX_TOTAL)
        add_session(self.db, "c", "u1", "v1", "s3", NOW - 1800, 15, 1)
        self.assertEqual(refresh(self.db, "u1"), {"ok": False, "errorCode": "STORAGE_ERROR"})

    def test_reopen_database_restores(self):
        path = os.path.join(os.environ.get("TMPDIR", "/tmp"), "history-test-%d.sqlite" % os.getpid())
        if os.path.exists(path):
            os.remove(path)
        try:
            db = sqlite3.connect(path)
            db.executescript(read("migrations", "001_initial.sql"))
            src = self.db
            for table in ("users", "vehicles", "zones", "spaces"):
                rows = src.execute("SELECT * FROM %s" % table).fetchall()
                if rows:
                    db.executemany("INSERT INTO %s VALUES (%s)" % (table, ",".join("?" * len(rows[0]))), rows)
            add_session(db, "a", "u1", "v1", "s1", NOW - 7200, 60, 18000)
            db.commit()
            db.close()
            db = sqlite3.connect(path)
            r = refresh(db, "u1")
            self.assertEqual((r["totalSessions"], r["totalKopecks"]), (1, 18000))
            db.close()
        finally:
            if os.path.exists(path):
                os.remove(path)


if __name__ == "__main__":
    sys.exit(0 if unittest.main(exit=False, verbosity=2).result.wasSuccessful() else 1)
