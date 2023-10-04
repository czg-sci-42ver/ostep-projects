#!/bin/bash
curl -O http://www.memcached.org/files/memcached-1.6.17.tar.gz
tar xvzf memcached-1.6.17.tar.gz
cd memcached-1.6.17
./configure
make
