"""Проверки реальных ограничений SQLite. Не заменяют проверку Qt и SDK."""

import sqlite3
import tempfile
import unittest
from contextlib import closing
from pathlib import Path


MIGRATION = Path(__file__).resolve().parents[2] / "migrations" / "001_initial.sql"


class SchemaTests(unittest.TestCase):
    def setUp(self):
        self.assertTrue(MIGRATION.is_file(), "Начальная миграция ещё не реализована")
        self.db = sqlite3.connect(":memory:")
        self.addCleanup(self.db.close)
        self.db.execute("PRAGMA foreign_keys = ON")
        self.db.executescript(MIGRATION.read_text(encoding="utf-8"))
        self.db.executemany(
            "INSERT INTO users(id, contact, display_name) VALUES(?,?,?)",
            [("u1", "one@example.test", "Первый"), ("u2", "two@example.test", "Второй")],
        )
        self.db.executemany(
            "INSERT INTO vehicles(id, user_id, plate, brand) VALUES(?,?,?,?)",
            [("v1", "u1", "А123ВС77", "Марка 1"), ("v2", "u2", "В456АС77", "Марка 2")],
        )
        self.db.execute(
            "INSERT INTO zones VALUES(?,?,?,?,?,?,?)",
            ("z1", "Демо", "Демо-адрес", 55.75, 37.61, 18000, "Круглосуточно"),
        )
        self.db.executemany(
            "INSERT INTO spaces VALUES(?,?,?,?)",
            [("s1", "z1", "01", 0), ("s2", "z1", "02", 0)],
        )
        self.db.commit()

    def session(self, session_id="b1", user="u1", vehicle="v1", space="s1", **overrides):
        values = dict(start=1000, end=4600, rate=18000, amount=18000, status="active")
        values.update(overrides)
        self.db.execute(
            "INSERT INTO sessions(id,user_id,vehicle_id,space_id,starts_at_utc,"
            "expires_at_utc,rate_kopecks_per_hour,amount_kopecks,status,plate_snapshot)"
            " VALUES(?,?,?,?,?,?,?,?,?,?)",
            (session_id, user, vehicle, space, values["start"], values["end"],
             values["rate"], values["amount"], values["status"], "А123ВС77"),
        )

    def payment(self, operation_id="op1", user="u1", session="b1", kind="book", amount=18000):
        self.db.execute(
            "INSERT INTO payment_operations VALUES(?,?,?,?,?,?,?,?)",
            (operation_id, user, session, kind, '{"minutes":60}',
             '{"ok":true,"data":{"amountKopecks":18000}}', amount, 1000),
        )

    def test_new_database_has_required_tables_and_version(self):
        tables = {row[0] for row in self.db.execute("SELECT name FROM sqlite_master WHERE type='table'")}
        self.assertTrue({"users", "vehicles", "settings", "zones", "spaces", "sessions", "payment_operations"} <= tables)
        self.assertEqual(self.db.execute("PRAGMA user_version").fetchone()[0], 1)
        self.assertEqual(self.db.execute("PRAGMA foreign_key_check").fetchall(), [])

    def test_profile_survives_reopening_database(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "parking.sqlite"
            with closing(sqlite3.connect(path)) as first:
                first.executescript(MIGRATION.read_text(encoding="utf-8"))
                first.execute("INSERT INTO users VALUES(?,?,?)", ("persist", "p@example.test", "Профиль"))
                first.commit()
            with closing(sqlite3.connect(path)) as reopened:
                self.assertEqual(reopened.execute("SELECT display_name FROM users WHERE id='persist'").fetchone()[0], "Профиль")

    def test_contact_is_unique(self):
        with self.assertRaises(sqlite3.IntegrityError):
            self.db.execute("INSERT INTO users VALUES(?,?,?)", ("u3", "one@example.test", "Повтор"))

    def test_vehicle_number_is_unique_within_user(self):
        with self.assertRaises(sqlite3.IntegrityError):
            self.db.execute("INSERT INTO vehicles VALUES(?,?,?,?)", ("v3", "u1", "А123ВС77", "Повтор"))

    def test_same_vehicle_number_can_belong_to_different_demo_profiles(self):
        self.db.execute("INSERT INTO vehicles VALUES(?,?,?,?)", ("v3", "u2", "А123ВС77", "Демо"))
        self.assertEqual(self.db.execute("SELECT count(*) FROM vehicles WHERE user_id='u2'").fetchone()[0], 2)

    def test_vehicle_requires_existing_user(self):
        with self.assertRaises(sqlite3.IntegrityError):
            self.db.execute("INSERT INTO vehicles VALUES(?,?,?,?)", ("v3", "missing", "А999ВС77", "Демо"))

    def test_second_active_session_for_same_user_is_rejected(self):
        self.session()
        with self.assertRaises(sqlite3.IntegrityError):
            self.session("b2", space="s2")

    def test_second_active_session_for_same_space_is_rejected(self):
        self.session()
        with self.assertRaises(sqlite3.IntegrityError):
            self.session("b2", user="u2", vehicle="v2")

    def test_completed_session_releases_user_and_space(self):
        self.session()
        self.db.execute("UPDATE sessions SET status='completed' WHERE id='b1'")
        self.session("b2")
        self.assertEqual(self.db.execute("SELECT count(*) FROM sessions").fetchone()[0], 2)

    def test_vehicle_cannot_be_used_by_different_user(self):
        with self.assertRaises(sqlite3.IntegrityError):
            self.session(user="u2", vehicle="v1")

    def test_negative_or_fractional_amounts_are_rejected(self):
        for amount in (-1, 1.5):
            with self.subTest(amount=amount), self.assertRaises(sqlite3.IntegrityError):
                self.session(amount=amount)

    def test_expiry_must_follow_start(self):
        for end in (999, 1000):
            with self.subTest(end=end), self.assertRaises(sqlite3.IntegrityError):
                self.session(end=end)

    def test_unknown_session_status_is_rejected(self):
        with self.assertRaises(sqlite3.IntegrityError):
            self.session(status="unpaid")

    def test_operation_id_cannot_charge_twice(self):
        self.session()
        self.payment()
        with self.assertRaises(sqlite3.IntegrityError):
            self.payment()

    def test_payment_cannot_refer_to_another_users_session(self):
        self.session()
        with self.assertRaises(sqlite3.IntegrityError):
            self.payment(user="u2")

    def test_only_one_initial_payment_per_session(self):
        self.session()
        self.payment()
        with self.assertRaises(sqlite3.IntegrityError):
            self.payment("op2")

    def test_multiple_distinct_extensions_are_allowed(self):
        self.session()
        self.payment()
        self.payment("op2", kind="extend", amount=4500)
        self.payment("op3", kind="extend", amount=4500)
        self.assertEqual(self.db.execute("SELECT sum(amount_kopecks) FROM payment_operations").fetchone()[0], 27000)

    def test_invalid_coordinates_are_rejected(self):
        with self.assertRaises(sqlite3.IntegrityError):
            self.db.execute("UPDATE zones SET latitude=91 WHERE id='z1'")

    def test_zero_tariff_is_allowed(self):
        self.db.execute("UPDATE zones SET rate_kopecks_per_hour=0 WHERE id='z1'")
        self.session(rate=0, amount=0)
        self.payment(amount=0)

    def test_user_data_is_not_deleted_while_session_exists(self):
        self.session()
        with self.assertRaises(sqlite3.IntegrityError):
            self.db.execute("DELETE FROM users WHERE id='u1'")


if __name__ == "__main__":
    unittest.main()
