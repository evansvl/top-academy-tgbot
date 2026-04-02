FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Etc/UTC

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    curl \
    libssl-dev \
    libcurl4-openssl-dev \
    zlib1g-dev \
    libboost-system-dev \
    libboost-thread-dev \
    tar \
    unzip \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /tmp
RUN git clone https://github.com/reo7sp/tgbot-cpp.git \
    && cd tgbot-cpp \
    && cmake -B build -S . \
    && cmake --build build --config Release --target install

WORKDIR /build_app
COPY CMakeLists.txt .
COPY src/ ./src/

RUN cmake -B build -S . \
    && cmake --build build --config Release

FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    libcurl4 \
    libssl3 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=builder /build_app/build/TopAcademyTgBot /app/TopAcademyTgBot

EXPOSE 8080

RUN chmod +x /app/TopAcademyTgBot

CMD ["./TopAcademyTgBot"]
