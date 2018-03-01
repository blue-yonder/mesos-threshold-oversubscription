FROM debian:jessie
ARG MESOS_VERSION=1.4.1-2.0.1

RUN apt-key adv --keyserver keyserver.ubuntu.com --recv E56151BF && \
    echo "deb http://repos.mesosphere.com/debian jessie main" > /etc/apt/sources.list.d/mesosphere.list && \
    apt-get update && \
    apt-get install -y \
        cmake \
        g++ \
        libcurl4-nss-dev \
        libgtest-dev \
        mesos=${MESOS_VERSION}

ADD . /src
RUN mkdir /build && \
    cd /build && \
    cmake /src && \
    make && \
    make test && \
    make install
