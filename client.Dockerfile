FROM ubuntu:24.04

RUN apt update && apt install -y \
    build-essential \
    cmake \
    g++ \
    libboost-all-dev \
    libpq-dev \
    wget \
    unzip \
    pkg-config \
    vim \
    git \
    libzmq3-dev \
    net-tools iproute2 iputils-ping

WORKDIR /opt
RUN git clone --branch 7.10.1 https://github.com/jtv/libpqxx.git
WORKDIR /opt/libpqxx
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --target install -j4

WORKDIR /app
COPY . .

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release . -DCMAKE_CXX_FLAGS_RELEASE="-g -DPOOL_MSG_COUNT=1500000 -DDOCKER" && cmake --build build -j4

WORKDIR /app/build/test

EXPOSE 8084/tcp
EXPOSE 30001/udp

CMD ["sh", "-c", "./RunTradeReceiver"]
# CMD ["sh", "-c", "./RunTradeReceiver && tail -f /dev/null"]

# docker build -t feedernet .
# docker images
# docker run -it <image_id> /bin/bash

# docker ps
# docker exec -it <container_id> /bin/bash