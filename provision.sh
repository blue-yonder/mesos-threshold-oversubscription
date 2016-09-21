#!/bin/bash
set -e
set -x

# Helper script to install all necessary build dependencies

DISTRO=$(lsb_release -is | tr '[:upper:]' '[:lower:]')
CODENAME=$(lsb_release -cs)

echo "deb http://repos.mesosphere.com/${DISTRO} ${CODENAME} main" | 
  tee /etc/apt/sources.list.d/mesosphere.list

apt-key adv --keyserver keyserver.ubuntu.com --recv E56151BF
apt-get -qq update
apt-get -y --force-yes install mesos

apt-get install -y
    cmake                              \
    g++-4.9                            \
    libprotobuf-dev                    \
    libboost-dev                       \
    libgoogle-glog-dev                 \
    libcurl4-nss-dev                   \
    libgtest-dev                       \
    mesos
