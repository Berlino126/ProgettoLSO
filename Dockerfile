FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    build-essential \
    gcc \
    make \
    net-tools \
    iputils-ping \
    vim \
    && rm -rf /var/lib/apt/lists/*

# Crea le directory
RUN mkdir -p /app/server /app/client

# Copia i sorgenti
COPY server/server.c /app/server/
COPY client/client.c /app/client/

# Compila server
WORKDIR /app/server
RUN gcc server.c -o server -lpthread

# Compila client
WORKDIR /app/client
RUN gcc client.c -o client
