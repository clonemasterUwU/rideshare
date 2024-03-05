FROM gcc:13.2-bookworm
LABEL authors="clonemasteruwu"

RUN apt-get update && \
    apt-get install -y cmake libgdal-dev && \
    rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY . /app
#RUN mkdir build && \
#    cd build && \
#    cmake .. && \
#    cmake --build . --target all