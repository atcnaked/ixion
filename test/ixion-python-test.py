#!/usr/bin/env python

import sys
import os.path
import unittest
import itertools

dirname = os.path.dirname(os.path.abspath(__file__))
sys.path.append(dirname + "/../src/python/.libs")
import ixion

class Test(unittest.TestCase):

    def setUp(self):
        self.doc = ixion.Document()

    def test_append_sheets(self):
        tests = (
            "Normal",      # normal name
            "First Sheet", # white space
            "Laura's",     # single quote
            '"Quoted"'     # double quote
        )

        sheets = []
        for test in tests:
            sh = self.doc.append_sheet(test)
            sheets.append(sh)

        for test, sheet in itertools.izip(tests, sheets):
            self.assertEqual(test, sheet.name)

        try:
            sheets[0].name = "Try to change sheet name"
            self.assertTrue(False, "sheet name attribute should not be writable.")
        except TypeError:
            pass # TypeError is expected when attempting to overwrite sheet name attribute.
        except:
            self.assertTrue(False, "Wrong exception has been raised")

    def test_numeric_cell_input(self):
        sh1 = self.doc.append_sheet("Data")

        # Empty cell should yield a value of 0.0.
        check_val = sh1.get_numeric_value(0, 0)
        self.assertEqual(0.0, check_val)

        tests = (
            # row, column, value
            (3, 1, 11.2),
            (4, 1, 12.0),
            (6, 2, -12.0),
            (6, 3, 0.0)
        )

        for test in tests:
            sh1.set_numeric_cell(test[0], test[1], test[2]) # row, column, value
            check_val = sh1.get_numeric_value(column=test[1], row=test[0]) # swap row and column
            self.assertEqual(test[2], check_val)

    def test_string_cell_input(self):
        sh1 = self.doc.append_sheet("Data")

        # Empty cell should yield an empty string.
        check_val = sh1.get_string_value(0, 0)
        self.assertEqual("", check_val)

        tests = (
            # row, column, value
            (0, 0, "normal string"),  # normal string
            (1, 0, "A1+B1"),          # string that looks like a formula expression
            (2, 0, "'single quote'"), # single quote
            (3, 0, "80's music"),     # single quote
            (4, 0, '"The" Music in the 80\'s'), # single and double quotes mixed
        )

        for test in tests:
            sh1.set_string_cell(test[0], test[1], test[2]) # row, column, value
            check_val = sh1.get_string_value(column=test[1], row=test[0]) # swap row and column
            self.assertEqual(test[2], check_val)

    def test_formula_cell_input(self):
        sh1 = self.doc.append_sheet("Data")
        sh1.set_formula_cell(0, 0, "12*3")
        try:
            val = sh1.get_numeric_value(0, 0)
            self.assertTrue(False, "TypeError should have been raised")
        except TypeError:
            # TypeError is expected when trying to fetch numeric value from
            # formula cell before it is calculated.
            pass

        self.doc.calculate()
        val = sh1.get_numeric_value(0, 0)
        self.assertEqual(12*3, val)

    def test_formula_cell_recalc(self):
        sh1 = self.doc.append_sheet("Data")
        sh1.set_numeric_cell(0, 0, 1.0)
        sh1.set_numeric_cell(1, 0, 2.0)
        sh1.set_numeric_cell(2, 0, 4.0)
        sh1.set_formula_cell(3, 0, "SUM(A1:A3)")

        # initial calculation
        self.doc.calculate()
        val = sh1.get_numeric_value(3, 0)
        self.assertEqual(7.0, val)

        # recalculation
        sh1.set_numeric_cell(1, 0, 8.0)
        self.doc.calculate()
        val = sh1.get_numeric_value(3, 0)
        self.assertEqual(13.0, val)

        # add another formula cell and recalc.
        sh1.set_formula_cell(0, 1, "A1+15")
        sh1.set_numeric_cell(0, 0, 0.0)
        self.doc.calculate()
        val = sh1.get_numeric_value(0, 1)
        self.assertEqual(15.0, val)
        val = sh1.get_numeric_value(3, 0)
        self.assertEqual(12.0, val)

    def test_formula_cell_recalc2(self):
        sh1 = self.doc.append_sheet("Data")
        sh1.set_numeric_cell(4, 1, 12.0) # B5
        sh1.set_formula_cell(5, 1, "B5*2")
        sh1.set_formula_cell(6, 1, "B6+10")

        self.doc.calculate()
        val = sh1.get_numeric_value(4, 1)
        self.assertEqual(12.0, val)
        val = sh1.get_numeric_value(5, 1)
        self.assertEqual(24.0, val)
        val = sh1.get_numeric_value(6, 1)
        self.assertEqual(34.0, val)

        # Delete B5 and check.
        sh1.erase_cell(4, 1)
        self.doc.calculate()
        val = sh1.get_numeric_value(4, 1)
        self.assertEqual(0.0, val)
        val = sh1.get_numeric_value(5, 1)
        self.assertEqual(0.0, val)
        val = sh1.get_numeric_value(6, 1)
        self.assertEqual(10.0, val)


if __name__ == '__main__':
    unittest.main()