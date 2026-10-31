FROM debian:trixie-slim AS build
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build git ca-certificates libpqxx-dev libhiredis-dev \
    nlohmann-json3-dev doctest-dev zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY server/CMakeLists.txt server/CMakeLists.txt
COPY server/cmake server/cmake
# Fetch and compile dependencies before source changes invalidate the build cache.
RUN cmake -S server -B build -G Ninja -DCAW_DEPENDENCIES_ONLY=ON -DCAW_BUILD_SERVER=ON \
    && cmake --build build -j 2

ARG CMAKE_BUILD_TYPE=Release
COPY server server
RUN cmake -S server -B build -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} \
    -DCAW_DEPENDENCIES_ONLY=OFF -DCAW_BUILD_SERVER=ON \
    && cmake --build build -j 2 \
    && ctest --test-dir build --output-on-failure

# Tooling lives only in the builder; CI runs bin/lint against the saved build database.
RUN apt-get update && apt-get install -y --no-install-recommends clang-format clang-tidy \
    && rm -rf /var/lib/apt/lists/*
COPY .clang-format .clang-tidy ./
COPY bin/lint bin/lint

FROM debian:trixie-slim AS runtime
RUN apt-get update && apt-get install -y --no-install-recommends \
    libpqxx-7.10 libhiredis1.1.0 zlib1g ca-certificates curl \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --system --uid 10001 --no-create-home caw
COPY --from=build /app/build/caw-server /usr/local/bin/caw-server
COPY --from=build /app/build/_deps/uwebsockets-src/LICENSE /usr/share/doc/caw/uWebSockets.LICENSE
COPY --from=build /app/build/_deps/uwebsockets-src/uSockets/LICENSE /usr/share/doc/caw/uSockets.LICENSE
COPY --from=build /app/build/_deps/redis_plus_plus-src/LICENSE /usr/share/doc/caw/redis-plus-plus.LICENSE
COPY --from=build /app/server/src/game/LICENSE.upstream /usr/share/doc/caw/game.LICENSE
COPY LICENSE /usr/share/doc/caw/CAW.LICENSE
# Card packs with their manifest, attribution and checksums, loaded at start (PACKS_DIR).
COPY assets/cardpacks /app/packs
USER 10001:10001
EXPOSE 8080
CMD ["caw-server"]
