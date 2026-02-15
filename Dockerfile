FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential cmake python3-pip git pkg-config libssl-dev \
    && rm -rf /var/lib/apt/lists/*

RUN pip3 install conan && conan profile detect --force

WORKDIR /app

COPY conanfile.txt .

RUN conan install . --build=missing --profile default || conan install . --build=missing

COPY . .

RUN cmake -S . -B build_out \
    -DCMAKE_TOOLCHAIN_FILE=build/Release/generators/conan_toolchain.cmake \
    -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build_out -j$(nproc)

FROM ubuntu:22.04
RUN apt-get update && apt-get install -y ca-certificates libssl3 && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY --from=builder /app/build_out/cqg .
COPY --from=builder /app/config.json .
RUN chmod +x ./cqg
ENTRYPOINT ["./cqg"]