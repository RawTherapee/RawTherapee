# syntax=docker/dockerfile:1.7

# The pinned pandoc/extra Ubuntu image provides Eisvogel, TeX Live, fonts, and
# filters, but contains Pandoc 3.10.0. Replace only the Pandoc executable with
# the authenticated upstream 3.10.2 release.
FROM pandoc/extra:3.10.0.0-ubuntu@sha256:01b2c9cf36d1056650af724934c5b4663276bbb0c7ab72579a345db189c8b885

ARG PANDOC_VERSION=3.10.2
ARG PANDOC_SHA256=c7edd535941c48be6a362081a748272837de81ae11777202d9c341d3d8261c9a

ADD --checksum=sha256:c7edd535941c48be6a362081a748272837de81ae11777202d9c341d3d8261c9a \
    https://github.com/jgm/pandoc/releases/download/3.10.2/pandoc-3.10.2-linux-amd64.tar.gz \
    /tmp/pandoc.tar.gz

RUN printf '%s  %s\n' "${PANDOC_SHA256}" /tmp/pandoc.tar.gz | sha256sum --check - \
    && tar -xzf /tmp/pandoc.tar.gz -C /tmp \
    && install -m 0755 "/tmp/pandoc-${PANDOC_VERSION}/bin/pandoc" /usr/local/bin/pandoc \
    && rm -rf /tmp/pandoc.tar.gz "/tmp/pandoc-${PANDOC_VERSION}" \
    && test "$(pandoc --version | sed -n '1s/^pandoc //p')" = "${PANDOC_VERSION}"
