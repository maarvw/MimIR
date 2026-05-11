from __future__ import annotations

import importlib.util
import inspect
import os
import shutil
import sys
import tempfile
import traceback
import unittest
from contextlib import ExitStack
from dataclasses import dataclass
from pathlib import Path


TEST_DIR = Path(__file__).resolve().parent
REPO_ROOT = TEST_DIR.parents[1]
PLUGIN_DIR = REPO_ROOT / "build" / "lib" / "mim"


if str(TEST_DIR) not in sys.path:
    sys.path.insert(0, str(TEST_DIR))


@dataclass
class TestOutcome:
    name: str
    status: str
    detail: str | None = None


class MonkeyPatch:

    def __init__(self):
        self._cwd = Path.cwd()

    def chdir(self, path):
        os.chdir(path)

    def undo(self):
        os.chdir(self._cwd)


class FixtureContext:

    def __init__(self):
        self._exit_stack = ExitStack()
        self._fixtures = {}
        self._monkeypatch = None

    def get(self, name):
        if name in self._fixtures:
            return self._fixtures[name]

        fixture = self._create(name)
        self._fixtures[name] = fixture
        return fixture

    def close(self):
        if self._monkeypatch is not None:
            self._monkeypatch.undo()

        self._exit_stack.close()

    def _create(self, name):
        import mim

        if name == "driver":
            return mim.Driver()

        if name == "world":
            driver = mim.Driver()
            self._fixtures["_world_driver"] = driver
            return driver.world()

        if name == "regex_world":
            if not PLUGIN_DIR.is_dir():
                raise FileNotFoundError(f"expected plugin directory at '{PLUGIN_DIR}'")

            driver = mim.Driver()
            driver.add_search_path(PLUGIN_DIR)
            driver.load_plugins(["compile", "mem", "core", "math", "regex", "opt"])
            self._fixtures["_regex_driver"] = driver
            return driver.world()

        if name == "tmp_path":
            tmpdir = self._exit_stack.enter_context(tempfile.TemporaryDirectory())
            return Path(tmpdir)

        if name == "monkeypatch":
            self._monkeypatch = MonkeyPatch()
            return self._monkeypatch

        raise KeyError(f"unknown fixture '{name}'")


def clean_name(name):
    if name.startswith("test_"):
        return name[5:]

    if name.startswith("Test"):
        return name[4:]

    return name


def import_test_module(path):
    spec = importlib.util.spec_from_file_location(path.stem, path)
    if spec is None or spec.loader is None:
        raise ImportError(f"could not load {path}")

    module = importlib.util.module_from_spec(spec)
    sys.modules[path.stem] = module
    spec.loader.exec_module(module)
    return module


def iter_parametrizations(func):
    cases = [({}, "")]

    for names, argvalues in getattr(func, "_mim_parametrizations", []):
        expanded = []
        for bound, suffix in cases:
            for value in argvalues:
                values = value if isinstance(value, tuple) else (value,)
                if len(values) != len(names):
                    raise ValueError(f"expected {len(names)} values for {names}, got {values!r}")
                params = dict(zip(names, values))
                labels = ", ".join(f"{name}={params[name]!r}" for name in names)
                expanded.append((bound | params, f"{suffix}[{labels}]"))
        cases = expanded

    return cases


def format_exception():
    return traceback.format_exc().rstrip()


def run_function(module_name, func):
    base_name = f"{module_name}.{clean_name(func.__name__)}"

    skip_reason = getattr(func, "_mim_skip_reason", None)
    if skip_reason is not None:
        return [TestOutcome(base_name, "SKIP", skip_reason)]

    outcomes = []
    for bound_args, suffix in iter_parametrizations(func):
        case_name = f"{base_name}{suffix}"
        fixtures = FixtureContext()
        try:
            kwargs = dict(bound_args)
            for parameter in inspect.signature(func).parameters:
                if parameter not in kwargs:
                    kwargs[parameter] = fixtures.get(parameter)

            func(**kwargs)
        except unittest.SkipTest as ex:
            outcomes.append(TestOutcome(case_name, "SKIP", str(ex)))
        except Exception:
            outcomes.append(TestOutcome(case_name, "FAIL", format_exception()))
        else:
            outcomes.append(TestOutcome(case_name, "PASS"))
        finally:
            fixtures.close()

    return outcomes


def run_unittest_method(module_name, cls, method_name):
    name = f"{module_name}.{clean_name(cls.__name__)}.{clean_name(method_name)}"
    test = cls(method_name)
    result = unittest.TestResult()
    test.run(result)

    if result.skipped:
        return TestOutcome(name, "SKIP", result.skipped[0][1])

    if result.failures:
        return TestOutcome(name, "FAIL", result.failures[0][1].rstrip())

    if result.errors:
        return TestOutcome(name, "FAIL", result.errors[0][1].rstrip())

    return TestOutcome(name, "PASS")


def run_module(path):
    module_name = clean_name(path.stem)
    try:
        module = import_test_module(path)
    except Exception:
        return [TestOutcome(module_name, "FAIL", format_exception())]

    outcomes = []
    for _, value in module.__dict__.items():
        if inspect.isfunction(value) and value.__module__ == module.__name__ and value.__name__.startswith("test_"):
            outcomes.extend(run_function(module_name, value))
        elif (
            inspect.isclass(value)
            and value.__module__ == module.__name__
            and issubclass(value, unittest.TestCase)
            and value is not unittest.TestCase
        ):
            for method_name in dir(value):
                if method_name.startswith("test_"):
                    outcomes.append(run_unittest_method(module_name, value, method_name))

    return outcomes


def main():
    test_files = sorted(TEST_DIR.glob("test_*.py"))
    outcomes = []
    for path in test_files:
        outcomes.extend(run_module(path))

    passed = skipped = failed = 0
    for outcome in outcomes:
        print(f"{outcome.status:4} {outcome.name}")
        if outcome.detail and outcome.status != "PASS":
            print(outcome.detail)
            print()

        if outcome.status == "PASS":
            passed += 1
        elif outcome.status == "SKIP":
            skipped += 1
        else:
            failed += 1

    total = len(outcomes)
    print(f"summary: {passed} passed, {skipped} skipped, {failed} failed, {total} total")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
