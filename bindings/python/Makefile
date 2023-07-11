build-ext:
	python setup.py build_ext -fi -j8

build-dist:
	python setup.py sdist

install:
	python setup.py build_ext -fi
	python setup.py install --force

test:
	python3 -m pytest --durations=5 --cov=./ --cov-report term-missing:skip-covered tests/

init:
	pip3 install -r requirements.txt

build-install: build-ext install

pypi-release:
	python3 setup.py sdist


.PHONY: build-ext build-dist install init test pypi-release
