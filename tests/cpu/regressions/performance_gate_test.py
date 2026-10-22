"""Check the speed gate with stable, noisy and inconsistent measurements."""
import contextlib
import importlib.util
import io
from pathlib import Path
import tempfile
import unittest


spec = importlib.util.spec_from_file_location(
    "workload_compare", Path(__file__).resolve().parents[1] / "run_workload_compare.py")
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)


def measurements(round_values, checksum="12345678"):
    return [[dict(ns_per_call=str(value), kernel_iterations="32", checksum=checksum)
             for value in values] for values in round_values]


class PerformanceGate(unittest.TestCase):
    def compare(self, before, after):
        with tempfile.TemporaryDirectory(dir=Path.cwd()) as directory, contextlib.redirect_stdout(io.StringIO()):
            return runner.summarize(Path(directory), ["kernel"], ["before", "after"], 6,
                                    {("kernel", "before"): before,
                                     ("kernel", "after"): after})[0]

    def test_stable_work(self):
        before = measurements([[100] * 9] * 6)
        after = measurements([[101] * 9] * 6)
        self.assertEqual(self.compare(before, after)["decision"], "no_clear_change")

    def test_repeatable_slowdown(self):
        before = measurements([[100] * 9] * 6)
        after = measurements([[106] * 9] * 6)
        self.assertEqual(self.compare(before, after)["decision"], "regression")

    def test_noisy_run_is_not_a_pass(self):
        before = measurements([[100] * 9] * 6)
        after = measurements([[90, 110] * 4 + [100]] * 6)
        self.assertEqual(self.compare(before, after)["decision"], "noisy")

    def test_inconsistent_slowdown_is_not_a_pass(self):
        before = measurements([[100] * 9] * 6)
        after = measurements([[105] * 9] * 4 + [[101] * 9] * 2)
        self.assertEqual(self.compare(before, after)["decision"], "needs_repeat")

    def test_mismatched_guest_results_cannot_count_as_a_gain(self):
        before = measurements([[100] * 9] * 6)
        after = measurements([[90] * 9] * 6, checksum="bad")
        with self.assertRaisesRegex(ValueError, "Guest work/results differ"):
            self.compare(before, after)


if __name__ == "__main__":
    unittest.main()
