FROM ubuntu:24.04 AS builder
RUN apt-get update && apt-get install -y --no-install-recommends cmake g++ make \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCACHEFLY_BUILD_TESTS=OFF \
    && cmake --build build --parallel

FROM ubuntu:24.04
RUN apt-get update && apt-get install -y --no-install-recommends redis-tools ca-certificates \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --system --uid 10001 --home /data cachefly \
    && mkdir -p /data && chown cachefly:cachefly /data
COPY --from=builder /src/build/src/cachefly /usr/local/bin/cachefly
COPY configs/cachefly.conf /etc/cachefly/cachefly.conf
WORKDIR /data
USER cachefly
EXPOSE 6379 8080
VOLUME ["/data"]
ENTRYPOINT ["/usr/local/bin/cachefly"]
CMD ["--config=/etc/cachefly/cachefly.conf"]
