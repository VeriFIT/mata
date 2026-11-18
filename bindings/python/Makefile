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

# The tests exercise to_dataframe() and to_networkx_graph(), hence the pandas and plotting extras.
init:
	# pip 25.1+ reads the development dependencies from the shared dependency group in pyproject.toml, so the
	#  versions are exactly the ones `uv sync` uses, not a second copy.
	pip3 install --upgrade "pip>=25.1"
	pip3 install --group bindings/python/pyproject.toml:dev "./[pandas,plotting]"

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
