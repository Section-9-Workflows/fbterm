FROM docker.io/library/debian@sha256:139eacf457e4a322903c5ac9d5ac33da2b12bd1340e830124694a467a415a0d9 AS build
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential debhelper autoconf automake pkg-config python3 \
    libfreetype6-dev libfontconfig1-dev libgpm-dev libdrm-dev ncurses-bin \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /source
COPY . /source/
ARG SOURCE_REVISION
ENV DEB_BUILD_OPTIONS=nocheck
RUN --network=none setarch i686 dpkg-buildpackage -b -us -uc -d
RUN mkdir /artifacts && cp /fbterm_*.deb /artifacts/ \
    && cp /fbterm_*.buildinfo /fbterm_*.changes /artifacts/ \
    && printf '%s\n' "$SOURCE_REVISION" > /artifacts/source-revision.txt \
    && dpkg-query -W > /artifacts/build-packages.tsv \
    && cp debian/fbterm/opt/fbterm/share/runtime-packages.json /artifacts/ \
    && tar -czf /artifacts/source.tar.gz --exclude=./debian/fbterm --exclude=./.git -C /source . \
    && cd /artifacts && sha256sum * > SHA256SUMS
FROM scratch AS artifacts
ARG SOURCE_REVISION
LABEL org.opencontainers.image.revision=$SOURCE_REVISION
COPY --from=build /artifacts/ /
