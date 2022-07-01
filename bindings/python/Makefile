build-ext:
	python setup.py build_ext -fi

build-dist:
	python setup.py sdist

install:
	python setup.py install --force

test:
	python3 -m pytest --cov=./ --cov-report term-missing:skip-covered tests/

build-install: build-ext install

