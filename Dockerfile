FROM debian:bullseye-slim AS builder
ENV DEBIAN_FRONTEND=noninteractive
RUN apt update && apt install -y \
        git cmake ninja-build build-essential \
        libqt5websockets5-dev libqt5serialbus5-dev libqt5serialport5-dev \
        libqt5sql5-mysql libqt5sql5-odbc libqt5sql5-psql libqt5sql5-sqlite \
        libssl-dev \
    && rm -rf /var/lib/apt/lists/*
COPY . /src
WORKDIR /src
RUN cmake \
    -D CMAKE_BUILD_TYPE=RelWithDebInfo \
    -D RADAPTER_GUI=OFF \
    -D RADAPTER_STATIC=ON \
    -G Ninja -B /build
RUN cmake --build /build -j$(nproc)

FROM debian:bullseye-slim AS runner
COPY --from=builder /lib /lib
COPY --from=builder /usr/lib /usr/lib
COPY --from=builder /build/bin/radapter /radapter
ENTRYPOINT [ "/radapter" ]