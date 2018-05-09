# Copyright (C) 2018  Christian Berger
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

# Part to build example.
FROM debian:buster-slim as builder
RUN apt-get -y update && apt-get -y upgrade 
RUN apt-get -y install cmake g++ libopencv-dev libopencv-core-dev libopencv-highgui-dev make

ADD . /opt/sources
WORKDIR /opt/sources
RUN mkdir build && \
    cd build && \
    cmake -D CMAKE_BUILD_TYPE=Release -D CMAKE_INSTALL_PREFIX=/tmp .. && \
    make && make install

# Part to deploy example.
FROM debian:buster-slim
RUN apt-get -y update && apt-get -y upgrade 
RUN apt-get -y install libopencv-dev

WORKDIR /usr/bin
COPY --from=builder /tmp/bin/example .
ENV NO_AT_BRIDGE=1
ENTRYPOINT ["/usr/bin/example"]

