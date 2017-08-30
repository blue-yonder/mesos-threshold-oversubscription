FROM debian:jessie

RUN apt-key adv --keyserver keyserver.ubuntu.com --recv E56151BF && \
    echo "deb http://repos.mesosphere.com/debian jessie main" > /etc/apt/sources.list.d/mesosphere.list && \
    apt-get update && \
    apt-get install -y \
        cmake \
        g++ \
        g++-4.9 \
        libprotobuf-dev \
        libboost-dev \
        libgoogle-glog-dev \
        libcurl4-nss-dev \
        libgtest-dev \
        mesos

ADD . /src
RUN mkdir /build && \
    cd /build && \
    cmake /src && \
    make && \
    make test && \
    make install
