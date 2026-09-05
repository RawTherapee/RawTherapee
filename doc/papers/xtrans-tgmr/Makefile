REPO_ROOT := $(abspath ../../..)
PAPER_DIR := doc/papers/xtrans-tgmr
IMAGE := rawtherapee-pandoc-extra:3.10.2
UID_GID := $(shell id -u):$(shell id -g)
SOURCE_DATE_EPOCH := 1787616000

.PHONY: all image verify pdf html clean

all: verify pdf html

image:
	docker build --platform linux/amd64 --pull=false \
		-t $(IMAGE) -f $(REPO_ROOT)/$(PAPER_DIR)/Dockerfile $(REPO_ROOT)/$(PAPER_DIR)

verify:
	cd $(REPO_ROOT) && python3 $(PAPER_DIR)/scripts/verify_claims.py

pdf: image verify
	mkdir -p $(REPO_ROOT)/$(PAPER_DIR)/output/pdf
	docker run --rm --platform linux/amd64 --user $(UID_GID) \
		-e HOME=/tmp -e SOURCE_DATE_EPOCH=$(SOURCE_DATE_EPOCH) \
		-v $(REPO_ROOT):/data -w /data $(IMAGE) \
		--defaults $(PAPER_DIR)/defaults-pdf.yaml \
		-o $(PAPER_DIR)/output/pdf/xtrans-tgmr-paper.pdf $(PAPER_DIR)/paper.md

html: image verify
	mkdir -p $(REPO_ROOT)/$(PAPER_DIR)/output/html
	docker run --rm --platform linux/amd64 --user $(UID_GID) \
		-e HOME=/tmp -e SOURCE_DATE_EPOCH=$(SOURCE_DATE_EPOCH) \
		-v $(REPO_ROOT):/data -w /data $(IMAGE) \
		--defaults $(PAPER_DIR)/defaults-html.yaml \
		-o $(PAPER_DIR)/output/html/xtrans-tgmr-paper.html $(PAPER_DIR)/paper.md

clean:
	rm -f $(REPO_ROOT)/$(PAPER_DIR)/output/pdf/xtrans-tgmr-paper.pdf \
		$(REPO_ROOT)/$(PAPER_DIR)/output/html/xtrans-tgmr-paper.html
