# Minimal makefile for Sphinx documentation
#

# You can set these variables from the command line.
SPHINXOPTS    =
SPHINXBUILD   = sphinx-build
SOURCEDIR     = .
BUILDDIR      = _build
ALLSPHINXOPTS = -d $(BUILDDIR)/doctrees $(SPHINXOPTS) .

init:
	pip3 install -r requirements.txt

# Put it first so that "make" without argument is like "make help".
help:
	@$(SPHINXBUILD) -M help "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)

.PHONY: help Makefile html dirhtml

html:
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
	rm -rf $(BUILDDIR)

# Catch-all target: route all unknown targets to Sphinx using the new
# "make mode" option.  $(O) is meant as a shortcut for $(SPHINXOPTS).
%: Makefile
	@$(SPHINXBUILD) -M $@ "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)
