from __future__ import annotations

import unittest


class _Raises:

    def __init__(self, expected):
        self.expected = expected
        self.value = None

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc, _tb):
        if exc_type is None:
            raise AssertionError(f"did not raise {self.expected.__name__}")

        if not issubclass(exc_type, self.expected):
            return False

        self.value = exc
        return True


class _Mark:

    @staticmethod
    def parametrize(argnames, argvalues):
        names = [name.strip() for name in argnames.split(",")]

        def decorate(func):
            parametrizations = list(getattr(func, "_mim_parametrizations", []))
            parametrizations.append((names, list(argvalues)))
            func._mim_parametrizations = parametrizations
            return func

        return decorate

    @staticmethod
    def skip(*, reason):
        def decorate(func):
            func._mim_skip_reason = reason
            return func

        return decorate

    def __getattr__(self, _name):
        def decorate(func):
            return func

        return decorate


mark = _Mark()


def raises(expected):
    return _Raises(expected)


def skip(reason):
    raise unittest.SkipTest(reason)
