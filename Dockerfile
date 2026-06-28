FROM debian:bullseye-slim AS builder
ENV DEBIAN_FRONTEND=noninteractive
RUN apt update && apt install -y \
        git file cmake ninja-build build-essential \
        libqt5websockets5-dev libqt5serialbus5-dev libqt5serialport5-dev \
        libqt5sql5-mysql libqt5sql5-odbc libqt5sql5-psql libqt5sql5-sqlite \
        libssl-dev \
    && rm -rf /var/lib/apt/lists/*
COPY . /src
WORKDIR /src
ENV CPM_SOURCE_CACHE=/cpm
RUN ls
RUN --mount=type=cache,target=/cpm \
    cmake \
    -D CMAKE_BUILD_TYPE=RelWithDebInfo \
    -D RADAPTER_GUI=OFF \
    -D RADAPTER_STATIC=OFF \
    -D RADAPTER_PACKAGES=ON \
    -G Ninja -B /build \
    && cmake --build /build -j$(nproc)
WORKDIR /build
RUN cpack -G DEB

FROM debian:bullseye-slim AS runner
COPY --from=builder /lib /lib
COPY --from=builder /usr/lib /usr/lib

COPY --from=builder /build/bin/libradapter-sdk.so /
COPY --from=builder /build/bin/radapter /
COPY --from=builder /build/radapter-headless_3.0_amd64.deb /
ENTRYPOINT [ "/radapter" ]