#!/usr/bin/env python3
# Эталон правил подбора (#20) на Python по тексту ТЗ и общим случаям rule_cases.json.
# Проверяет согласованность случаев и формул; C++ проверяет tst_recommendationrules.cpp
# на тех же случаях. Запуск: python3 tests/experience/test_rules.py
import json
import os
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))


def nearest(cs):
    known = [c for c in cs if c[2] >= 0]
    return min(known, key=lambda c: (c[2], c[1], c[0]))[0] if known else None


def cheapest(cs):
    if not cs:
        return None
    return min(cs, key=lambda c: (c[1], 0 if c[2] >= 0 else 1, c[2] if c[2] >= 0 else 0, c[0]))[0]


def balanced(cs):
    known = [c for c in cs if c[2] >= 0]
    if not known:
        return cheapest(cs), True
    p0, p1 = min(c[1] for c in known), max(c[1] for c in known)
    d0, d1 = min(c[2] for c in known), max(c[2] for c in known)

    def score(c):
        ps = (c[1] - p0) / (p1 - p0) if p1 > p0 else 0.0
        ds = (c[2] - d0) / (d1 - d0) if d1 > d0 else 0.0
        return 0.5 * ps + 0.5 * ds

    return min(known, key=lambda c: (score(c), c[0]))[0], False


class Rules(unittest.TestCase):
    def test_cases(self):
        with open(os.path.join(HERE, "rule_cases.json"), encoding="utf-8") as f:
            cases = json.load(f)["cases"]
        self.assertGreaterEqual(len(cases), 10)
        for case in cases:
            with self.subTest(case["name"]):
                cs = [tuple(c) for c in case["candidates"]]
                self.assertEqual(nearest(cs), case["nearest"])
                self.assertEqual(cheapest(cs), case["cheapest"])
                b, price_only = balanced(cs)
                self.assertEqual(b, case["balanced"])
                self.assertEqual(price_only, case["priceOnly"])


if __name__ == "__main__":
    sys.exit(0 if unittest.main(exit=False, verbosity=2).result.wasSuccessful() else 1)
