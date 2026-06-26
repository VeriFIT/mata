# Increase the jobs to more to speed up the compilation time.
jobs=6

build-ext:
	python setup.py build_ext -i -j$(jobs)

build-ext-force:
	python setup.py build_ext -fi -j$(jobs)

build-dist:
	python setup.py sdist

install:
	pip install --force-reinstall -e './[all,dev]'

test:
	python -m pytest --durations=5 --cov=./ --cov-report term-missing:skip-covered tests/

init:
	pip install -e './[all,dev]'

build-install: build-ext install

pypi-release:
	python setup.py sdist

clean:
	find . -type d \( -name "__pycache__" -o -name "*.egg-info" \) -exec rm -rf {} +
	find . -type f \( -name "*.cpp" -o -name "*.dot" -o -name "*.so" -o -name "*.pdf" \) -exec rm -f {} +
	rm -rf \
		.coverage \
		dist/ \
		MANIFEST \
		virtenv

.PHONY: build-ext build-dist install init test pypi-release clean
