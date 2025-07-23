FROM ubuntu:24.04

RUN apt update && apt install -y \
    build-essential \
    cmake \
    g++ \
    libboost-all-dev \
    wget \
    unzip \
    gdb \
    vim \
    net-tools iproute2 iputils-ping

WORKDIR /app
COPY . .

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release . -DCMAKE_CXX_FLAGS_RELEASE="-g -DPOOL_MSG_COUNT=1000000 -DDOCKER" && cmake --build build -j4

RUN wget https://data.binance.vision/data/spot/daily/trades/ETHUSDC/ETHUSDC-trades-2025-06-20.zip && unzip ETHUSDC-trades-2025-06-20.zip && rm ETHUSDC-trades-2025-06-20.zip

WORKDIR /app/build/test

EXPOSE 8084/tcp
EXPOSE 30001/udp

CMD ["./RunTradeServer"]

# docker build -t feedernet .
# docker images
# docker run -it <image_id> /bin/bash

# docker ps
# docker exec -it <container_id> /bin/bash