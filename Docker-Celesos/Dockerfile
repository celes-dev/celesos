FROM eosio/builder as builder
ARG branch=master
ARG symbol=SYS
ARG http_proxy=""
ARG https_proxy=""
ARG no_proxy=""
ARG git_http_proxy=""
ARG git_https_proxy=""
ARG git_crendential=""

RUN git config --global http.proxy "${git_http_proxy}" \
    && git config --global https.proxy "${git_https_proxy}" \
    && git clone -b $branch http://${git_crendential}172.12.0.11/celesos/src.git --recursive ./eos \
    && git config --global --unset http.proxy \
    && git config --global --unset https.proxy \
    && cd eos && echo "$branch:$(git rev-parse HEAD)" > /etc/eosio-version \
    && cmake -H. -B"/tmp/build" -GNinja -DCMAKE_BUILD_TYPE=Release -DWASM_ROOT=/opt/wasm -DCMAKE_CXX_COMPILER=clang++ \
       -DCMAKE_C_COMPILER=clang -DCMAKE_INSTALL_PREFIX=/tmp/build  -DSecp256k1_ROOT_DIR=/usr/local -DBUILD_MONGO_DB_PLUGIN=true -DCORE_SYMBOL_NAME=$symbol \
    && cmake --build /tmp/build --target install && rm /tmp/build/bin/eosiocpp


FROM ubuntu:18.04

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get -y install openssl ca-certificates && rm -rf /var/lib/apt/lists/*
COPY --from=builder /usr/local/lib/* /usr/local/lib/
COPY --from=builder /tmp/build/bin /opt/eosio/bin
COPY --from=builder /tmp/build/contracts /contracts
COPY --from=builder /eos/Docker/config.ini /
COPY --from=builder /etc/eosio-version /etc
COPY --from=builder /eos/Docker/nodeosd.sh /opt/eosio/bin/nodeosd.sh
ENV EOSIO_ROOT=/opt/eosio
RUN chmod +x /opt/eosio/bin/nodeosd.sh
ENV LD_LIBRARY_PATH /usr/local/lib
VOLUME /opt/eosio/bin/data-dir
ENV PATH /opt/eosio/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
