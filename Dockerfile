FROM ubuntu

RUN apt update && apt install -y \
    build-essential \
    cmake \
    g++ \
    clang \
    libboost-all-dev \
    wget \
    unzip \
    gdb \
    vim

WORKDIR /app
COPY . .

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release . -DCMAKE_CXX_FLAGS_RELEASE="-g -DPOOL_MSG_COUNT=1000000" && cmake --build build -j4

RUN wget https://data.binance.vision/data/spot/daily/trades/ETHUSDC/ETHUSDC-trades-2025-06-20.zip && unzip ETHUSDC-trades-2025-06-20.zip
RUN rm ETHUSDC-trades-2025-06-20.zip

# docker build -t feedernet .
# docker images
# docker run -it <image_id> /bin/bash

# docker ps
# docker exec -it <container_id> /bin/bash