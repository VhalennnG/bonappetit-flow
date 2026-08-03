# Build stage
FROM ubuntu:22.04 AS builder

# Install build dependencies
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y \
    git \
    cmake \
    g++ \
    libjsoncpp-dev \
    uuid-dev \
    zlib1g-dev \
    libssl-dev \
    nlohmann-json3-dev \
    && rm -rf /var/lib/apt/lists/*

# Install Drogon Web Framework from source
RUN git clone https://github.com/drogonframework/drogon.git \
    && cd drogon \
    && git submodule update --init \
    && mkdir build \
    && cd build \
    && cmake .. -DDR_TEMPLATE_PATH=/app \
    && make -j$(nproc) \
    && make install \
    && cd ../.. \
    && rm -rf drogon

# Build our application
WORKDIR /app
COPY . .
RUN rm -rf build \
    && mkdir build \
    && cd build \
    && cmake -DCMAKE_BUILD_TYPE=Release .. \
    && make -j$(nproc)

# Runtime stage
FROM ubuntu:22.04

# Install runtime dependencies (Drogon requires shared libraries)
RUN apt-get update && apt-get install -y \
    libjsoncpp-dev \
    uuid-dev \
    zlib1g-dev \
    libssl-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /app/build/be_server .

# Expose port (Render overrides this with $PORT env variable, default is 8080)
EXPOSE 8080

CMD ["./be_server"]
