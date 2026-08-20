# Increase the jobs to more to speed up the compilation time.
jobs=6

build-ext:
	python3 setup.py build_ext -i -j$(jobs)

build-ext-force:
	python3 setup.py build_ext -fi -j$(jobs)

build-dist:
	python3 setup.py sdist

install:
	python3 setup.py build_ext -fi
	python3 setup.py install --force

test:
	python3 -m pytest --durations=5 --cov=./ --cov-report term-missing:skip-covered tests/

init:
	pip3 install ./

build-install: build-ext install

pypi-release:
	python3 setup.py sdist

clean:
	find . -type d \( -name "__pycache__" -o -name "*.egg-info" \) -exec rm -rf {} +
	find . -type f \( -name "*.cpp" -o -name "*.dot" -o -name "*.so" -o -name "*.pdf" \) -exec rm -f {} +
	rm -rf \
		.coverage \
		dist/ \
		MANIFEST \
		virtenv

.PHONY: build-ext build-dist install init test pypi-release clean
