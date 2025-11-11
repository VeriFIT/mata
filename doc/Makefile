# Makefile for Sphinx documentation.

# You can set these variables from the command line.
SPHINXOPTS    =
SPHINXBUILD   = sphinx-build
SOURCEDIR     = src/
CONFIG_DIR    = ./
BUILDDIR      = _build/

# --fresh-env
ALLSPHINXOPTS = --conf-dir "$(CONFIG_DIR)" --doctree-dir "$(BUILDDIR)/doctrees" $(SPHINXOPTS) "$(SOURCEDIR)"


# Put it first so that "make" without argument is like "make help".
help:
	@$(SPHINXBUILD) -M help $(ALLSPHINXOPTS) $(O)

.PHONY: help Makefile html dirhtml

init:
	pip3 install -r requirements.txt

html:
	breathe-apidoc --output-dir ./src/ --force --members ./xml/
	$(SPHINXBUILD) -b html $(ALLSPHINXOPTS) $(BUILDDIR)/html
	@echo "Finished generating HTML documentation in $(BUILDDIR)/html."

dirhtml:
	$(SPHINXBUILD) -b dirhtml $(ALLSPHINXOPTS) $(BUILDDIR)/dirhtml
	@echo "Finished generating HTML documentation in $(BUILDDIR)/dirhtml."

latex:
	$(SPHINXBUILD) -b latex $(ALLSPHINXOPTS) $(BUILDDIR)/latex
	# Remove characters that seems to be problematic for latexmk...
	sed -i.bak 's///g;s/┃/|/g;s/═/=/g;s/▣/*/g' $(BUILDDIR)/latex/mata.tex
	make -C $(BUILDDIR)/latex all-pdf
	@echo "Finished generating latex pdf documentation in $(BUILDDIR)/latex."

clean:
	rm -rf $(BUILDDIR) ./api/ ./xml/

# Catch-all target: route all unknown targets to Sphinx using the new
# "make mode" option.  $(O) is meant as a shortcut for $(SPHINXOPTS).
%: Makefile
	@$(SPHINXBUILD) -M $@ $(ALLSPHINXOPTS) $(O)
