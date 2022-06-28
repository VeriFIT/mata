"""Shared fixtures for the testing of functionality of Python binding."""

import os
import shutil
import pytest

__author__ = 'Tomas Fiedor'


@pytest.fixture(scope="function")
def cleandir():
    """Runs the test in the clean new dir, which is purged afterwards"""
    temp_path = tempfile.mkdtemp()
    os.chdir(temp_path)
    yield
    shutil.rmtree(temp_path)
