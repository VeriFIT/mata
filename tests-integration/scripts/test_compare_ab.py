"""Tests of compare_ab.py; run with `python -m unittest discover --start-directory tests-integration/scripts`."""

import pathlib
import subprocess
import sys
import tempfile
import unittest

import yaml

SCRIPT = pathlib.Path(__file__).with_name("compare_ab.py")


class CompareAbTest(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.dir = pathlib.Path(tmp.name)

    def run_script(self, *args):
        return subprocess.run([sys.executable, SCRIPT, *map(str, args)], capture_output=True, text=True, check=False)

    def write_rounds(self, columns, rounds):
        """Writes one pyco_proc CSV per round; a round maps an instance to its cells in `columns` order."""
        paths = []
        for number, cells_by_instance in enumerate(rounds):
            path = self.dir / f"ab-{number}.csv"
            lines = [";".join(["name", *columns])]
            lines += [";".join([instance, *map(str, cells)]) for instance, cells in cells_by_instance.items()]
            path.write_text("\n".join(lines) + "\n")
            paths.append(path)
        return paths

    def compare(self, columns, rounds, *options):
        return self.run_script("compare", "--timeout", 10, *options, *self.write_rounds(columns, rounds))

    @staticmethod
    def table(stdout):
        """Returns {metric: [compared, excluded, failed base, failed head, geomean, worst ratio, worst instance]}."""
        rows = [line[2:-2].split(" | ") for line in stdout.splitlines() if line.startswith("| ")]
        # Skip the header and the separator line.
        return {row[0]: row[1:] for row in rows[2:]}

    def test_failed_run_costs_twice_the_timeout(self):
        columns = ["base-op-runtime", "head-op-runtime"]
        rounds = [{"a": [1, 1], "b": [1, 1], "c": [8, 8], "d": [0.5, 0.5], "e": [0.01, 0.01]}]
        rounds[0]["c"][1], rounds[0]["d"][1] = "TO", "ERR"
        result = self.compare(columns, rounds, "--threshold", 1.1)
        # c: 20/8 = 2.5, d: 20/0.5 = 40; the geometric mean over a, b, c, d is 10**0.5.
        self.assertEqual(self.table(result.stdout)["op-runtime"], ["4", "1", "0", "2", "3.162", "40.000", "d"])
        self.assertEqual(result.returncode, 1)
        self.assertIn("REGRESSION: 'op-runtime'", result.stdout)

    def test_medians_ignore_an_outlier_round(self):
        columns = ["base-op-runtime", "head-op-runtime"]
        rounds = [{"a": [1, 1]}, {"a": [1, 1]}, {"a": [1, 5]}]
        result = self.compare(columns, rounds, "--threshold", 1.1)
        self.assertEqual(self.table(result.stdout)["op-runtime"][4], "1.000")
        self.assertEqual(result.returncode, 0)

    def test_runtimes_below_the_cutoff_are_excluded_or_floored(self):
        columns = ["base-op-runtime", "head-op-runtime"]
        rounds = [{"fast": [0.01, 0.04], "slower": [0.02, 0.1]}]
        result = self.compare(columns, rounds)
        # fast: both below 0.05 s, excluded; slower: 0.1 / max(0.02, 0.05).
        self.assertEqual(self.table(result.stdout)["op-runtime"][:5], ["1", "1", "0", "0", "2.000"])
        self.assertEqual(result.returncode, 0)

    def test_only_fast_runtimes_pass_with_their_exclusions(self):
        columns = ["base-op-runtime", "head-op-runtime", "base-op-result", "head-op-result"]
        rounds = [{"a": [0.01, 0.02, "true", "true"]}]
        result = self.compare(columns, rounds, "--threshold", 1.1)
        self.assertEqual(self.table(result.stdout), {"op-runtime": ["0", "1", "0", "0", "-", "-", ""]})
        self.assertEqual(result.returncode, 0)

    def test_failed_runs_leave_out_their_memory(self):
        columns = ["base-op-runtime", "base-op-maxrss", "head-op-runtime", "head-op-maxrss"]
        rounds = [{"a": [1, 1000, 1, 1100], "b": [1, 1000, "MO", "MO"]}]
        result = self.compare(columns, rounds)
        table = self.table(result.stdout)
        self.assertEqual(table["op-maxrss"][:5], ["1", "0", "0", "1", "1.100"])
        self.assertEqual(table["op-runtime"][:5], ["2", "0", "0", "1", "4.472"])

    def test_threshold_sets_the_exit_status(self):
        columns = ["base-op-runtime", "head-op-runtime"]
        rounds = [{"a": [1, 1.2]}]
        self.assertEqual(self.compare(columns, rounds, "--threshold", 1.3).returncode, 0)
        self.assertEqual(self.compare(columns, rounds, "--threshold", 1.1).returncode, 1)
        self.assertEqual(self.compare(columns, rounds).returncode, 0)

    def test_rejects_rounds_without_a_common_metric(self):
        result = self.compare(["base-old-runtime", "head-new-runtime"], [{"a": [1, 1]}], "--threshold", 1.1)
        self.assertEqual(result.returncode, 2)
        self.assertIn("no metric measured on both sides", result.stderr)

    def test_notes_a_metric_only_one_build_prints(self):
        columns = ["base-op-runtime", "head-op-runtime", "head-op-new"]
        result = self.compare(columns, [{"a": [1, 1, 3]}], "--threshold", 1.1)
        self.assertEqual(list(self.table(result.stdout)), ["op-runtime"])
        self.assertIn("Not compared, measured by one build only: op-new (head only).", result.stdout)
        self.assertEqual(result.returncode, 0)

    def test_rejects_a_missing_run(self):
        columns = ["base-op-runtime", "head-op-runtime"]
        result = self.compare(columns, [{"a": [1, 1], "b": [1, "MISSING"]}], "--threshold", 1.1)
        self.assertEqual(result.returncode, 2)
        self.assertIn("op-runtime of b (base only)", result.stderr)

    def test_rejects_a_csv_without_base_and_head_columns(self):
        result = self.compare(["op-runtime"], [{"a": [1]}], "--threshold", 1.1)
        self.assertEqual(result.returncode, 2)
        self.assertIn("no base-/head- columns", result.stderr)

    def test_rejects_rounds_without_measurements(self):
        result = self.compare(["base-op-result", "head-op-result"], [{"a": ["true", "true"]}], "--threshold", 1.1)
        self.assertEqual(result.returncode, 2)
        self.assertIn("no metric measured on both sides", result.stderr)

    def write_jobs(self, name, jobs):
        path = self.dir / name
        path.write_text(yaml.safe_dump(jobs))
        return path

    def test_jobs_interleave_the_common_methods(self):
        base = self.write_jobs("base.yaml", {"defaults": {"timeout": 1}, "op": {"cmd": "base/op"}, "gone": {}})
        head = self.write_jobs("head.yaml", {"op": {"cmd": "head/op"}, "new": {"cmd": "head/new"}})
        result = self.run_script("jobs", "--head-first", base, head)
        self.assertEqual(result.returncode, 0)
        merged = yaml.safe_load(result.stdout)
        self.assertEqual(list(merged), ["head-op", "base-op"])
        self.assertEqual(merged["base-op"], {"timeout": 1, "cmd": "base/op"})
        self.assertIn("base-only methods gone", result.stderr)
        self.assertIn("head-only methods new", result.stderr)

    def test_jobs_reject_files_without_common_methods(self):
        base = self.write_jobs("base.yaml", {"old": {"cmd": "base/old"}})
        head = self.write_jobs("head.yaml", {"new": {"cmd": "head/new"}})
        result = self.run_script("jobs", base, head)
        self.assertEqual(result.returncode, 2)
        self.assertIn("share no method", result.stderr)


if __name__ == "__main__":
    unittest.main()
