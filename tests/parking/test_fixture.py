# SPDX-License-Identifier: MIT
"""Проверки фикстуры data/parking-zones.json по docs/CONTRACTS.md.

Запуск из корня репозитория:
    python3 -m unittest discover -s tests/parking -p 'test_*.py'

Проверяется только файл данных. Сборку приложения и работу ParkingService
эти проверки не доказывают.
"""

import json
import math
import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
FIXTURE = ROOT / "data" / "parking-zones.json"

ZONE_FIELDS = {
    "id": str,
    "name": str,
    "address": str,
    "latitude": (int, float),
    "longitude": (int, float),
    "rateKopecksPerHour": int,
    "workingHours": str,
    "spaces": list,
}
SPACE_FIELDS = {"id": str, "label": str, "occupied": bool}


def _no_duplicate_keys(pairs):
    keys = [k for k, _ in pairs]
    duplicates = {k for k in keys if keys.count(k) > 1}
    if duplicates:
        raise ValueError("повторяющиеся ключи: %s" % sorted(duplicates))
    return dict(pairs)


def load_fixture():
    with FIXTURE.open(encoding="utf-8") as handle:
        return json.load(handle, object_pairs_hook=_no_duplicate_keys)


def _is_type(value, expected):
    # bool — подкласс int в Python, поэтому исключаем его явно.
    if expected is not bool and isinstance(value, bool):
        return False
    return isinstance(value, expected)


class ParkingFixtureTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data = load_fixture()
        cls.zones = cls.data["zones"]

    def test_header(self):
        self.assertEqual(self.data["schemaVersion"], 1)
        self.assertIs(self.data["isDemo"], True)
        self.assertIsInstance(self.zones, list)

    def test_at_least_five_zones(self):
        self.assertGreaterEqual(len(self.zones), 5)

    def test_zone_fields_and_types(self):
        for zone in self.zones:
            for field, expected in ZONE_FIELDS.items():
                with self.subTest(zone=zone.get("id"), field=field):
                    self.assertIn(field, zone)
                    self.assertTrue(_is_type(zone[field], expected))
            for field in ("id", "name", "address", "workingHours"):
                self.assertTrue(zone[field].strip(), field)

    def test_coordinates_valid(self):
        for zone in self.zones:
            with self.subTest(zone=zone["id"]):
                lat, lon = zone["latitude"], zone["longitude"]
                self.assertTrue(math.isfinite(lat) and math.isfinite(lon))
                self.assertTrue(-90 <= lat <= 90)
                self.assertTrue(-180 <= lon <= 180)

    def test_rate_integer_non_negative(self):
        for zone in self.zones:
            with self.subTest(zone=zone["id"]):
                self.assertTrue(_is_type(zone["rateKopecksPerHour"], int))
                self.assertGreaterEqual(zone["rateKopecksPerHour"], 0)

    def test_ids_unique(self):
        zone_ids = [z["id"] for z in self.zones]
        self.assertEqual(len(zone_ids), len(set(zone_ids)))
        space_ids = [s["id"] for z in self.zones for s in z["spaces"]]
        self.assertEqual(len(space_ids), len(set(space_ids)))
        self.assertFalse(set(zone_ids) & set(space_ids))

    def test_spaces_non_empty_and_valid(self):
        for zone in self.zones:
            with self.subTest(zone=zone["id"]):
                self.assertTrue(zone["spaces"])
                labels = [s["label"] for s in zone["spaces"]]
                self.assertEqual(len(labels), len(set(labels)))
                for space in zone["spaces"]:
                    for field, expected in SPACE_FIELDS.items():
                        self.assertIn(field, space)
                        self.assertTrue(_is_type(space[field], expected))
                    self.assertTrue(space["id"].strip())
                    self.assertTrue(space["label"].strip())

    def test_has_fully_occupied_zone(self):
        full = [z for z in self.zones if all(s["occupied"] for s in z["spaces"])]
        self.assertTrue(full, "нужна хотя бы одна полностью занятая зона")

    def test_has_free_zone_with_available_space(self):
        free = [z for z in self.zones
                if z["rateKopecksPerHour"] == 0
                and any(not s["occupied"] for s in z["spaces"])]
        self.assertTrue(free, "нужна бесплатная зона со свободным местом")

    def test_working_hours_round_the_clock(self):
        for zone in self.zones:
            self.assertEqual(zone["workingHours"], "Круглосуточно")

    def test_names_mark_demo(self):
        for zone in self.zones:
            self.assertTrue(zone["name"].startswith("Демо-"), zone["id"])


if __name__ == "__main__":
    unittest.main()
