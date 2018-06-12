ARG DEBIAN_VERSION=stretch
FROM debian:${DEBIAN_VERSION}
ARG MESOS_VERSION=1.5.0-2.0.1

ARG DEBIAN_VERSION
RUN apt-get update && \
    apt-get install -y gnupg && \
    apt-key adv --keyserver keyserver.ubuntu.com --recv E56151BF && \
    echo deb "http://repos.mesosphere.com/debian ${DEBIAN_VERSION} main" > /etc/apt/sources.list.d/mesosphere.list && \
    apt-get update && \
    apt-get install -y \
        cmake \
        g++ \
        libcurl4-nss-dev \
        libgtest-dev \
        systemd \
        mesos=${MESOS_VERSION}

ADD . /src
RUN mkdir /build && \
    cd /build && \
    cmake /src && \
    make && \
    make test && \
    make install
