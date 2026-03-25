# Increase the jobs to more to speed up the compilation time.
jobs=6

build-ext:
	python setup.py build_ext -i -j$(jobs)

build-ext-force:
	python setup.py build_ext -fi -j$(jobs)

build-dist:
	python setup.py sdist

install:
	python setup.py build_ext -fi
	python setup.py install --force

test:
	python -m pytest --durations=5 --cov=./ --cov-report term-missing:skip-covered tests/

init:
	pip install ./

build-install: build-ext install

pypi-release:
	python setup.py sdist


.PHONY: build-ext build-dist install init test pypi-release
