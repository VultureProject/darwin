# FAUP v1.5
FROM ubuntu:18.04
WORKDIR /root
RUN apt-get update && apt-get install -y\
 git\
 cmake\
 g++\
 pkg-config
RUN git clone git://github.com/stricaud/faup --single-branch --branch v1.5 &&\
 cd faup/build &&\
 cmake .. &&\
 make -j2 install DESTDIR=../install
