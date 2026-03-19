# -------- Build Stage --------
FROM khordadi/truejit:base AS builder
LABEL authors="khordadi"

# Copy only what's needed to build
COPY ./lib /tmp/truejit/lib
COPY ./tools /tmp/truejit/tools
COPY ./CMakeLists.txt /tmp/truejit/CMakeLists.txt

# Build and install
WORKDIR /tmp/truejit
RUN mkdir build && cd build && \
    cmake -GNinja -DCMAKE_BUILD_TYPE=Release .. && \
    sudo cmake --build . --target install -- -j$(nproc --all) \
    && cd .. && sudo rm -rf build \
    && sudo rm -rf lib tools CMakeLists.txt
# -------- Final Stage --------
FROM khordadi/truejit:base
LABEL authors="khordadi"

ENV DISABLE_UPDATE_PROMPT=true
ENV DISABLE_AUTO_UPDATE=true

# gRPC ports
EXPOSE 50051
EXPOSE 50052

# Copy non-build-time resources
COPY ./benchmarks /tmp/truejit/benchmarks
COPY ./keys /tmp/truejit/keys
COPY ./models /tmp/truejit/models

# --- Evaluation scripts ---
# 1. bounded-cache
COPY ./evaluation/utility.py /tmp/truejit/scripts/utility.py
COPY ./evaluation/bounded-cache/orc.py /tmp/truejit/scripts/orc.py
# worker.sh
COPY ./scripts /tmp/truejit/scripts


WORKDIR /tmp/truejit/scripts

# Copy installed artifacts from builder
COPY --from=builder /usr/local /usr/local

#ENTRYPOINT ["/bin/zsh"]