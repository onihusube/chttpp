FROM gcc:latest
RUN apt-get update \
    && apt-get install -qqy --no-install-recommends \
    python3-pip \
    cmake \
    && apt-get -y clean \
    && rm -rf /var/lib/apt/lists/*
RUN pip3 install meson ninja
WORKDIR /