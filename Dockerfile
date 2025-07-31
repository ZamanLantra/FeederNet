FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt update && apt install -y \
    build-essential \
    cmake \
    g++ \
    libboost-all-dev \
    wget \
    unzip \
    gdb \
    vim \
    libpqxx-dev \
    net-tools iproute2 iputils-ping

WORKDIR /app
COPY . .

# docker build -t feedernet-dev .