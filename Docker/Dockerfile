FROM 172.12.0.11:5000/celesos/builder:v1.4.2 as builder
ARG branch=master
ARG symbol=CCHN
ARG http_proxy=""
ARG https_proxy=""
ARG no_proxy=""
ARG git_http_proxy=""
ARG git_https_proxy=""
ARG git_crendential=""

RUN git config --global http.proxy "${git_http_proxy}" \
    && git config --global https.proxy "${git_https_proxy}" \
    && git clone -b $branch http://${git_crendential}172.12.0.11/celesos/src.git --recursive ./celes \
    && git config --global --unset http.proxy \
    && git config --global --unset https.proxy \
    && cd celes && echo "$branch:$(git rev-parse HEAD)" > /etc/celesos-version \
    && cmake -H. -B"/tmp/build" -GNinja -DCMAKE_BUILD_TYPE=Release -DWASM_ROOT=/opt/wasm -DCMAKE_CXX_COMPILER=clang++ \
       -DCMAKE_C_COMPILER=clang -DCMAKE_INSTALL_PREFIX=/tmp/build -DBUILD_MONGO_DB_PLUGIN=true -DCORE_SYMBOL_NAME=$symbol \
    && cmake --build /tmp/build --target install && rm /tmp/build/bin/eosiocpp


FROM ubuntu:18.04

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get -y install openssl ca-certificates && rm -rf /var/lib/apt/lists/*
COPY --from=builder /usr/local/lib/* /usr/local/lib/
COPY --from=builder /tmp/build/bin /opt/celesos/bin
COPY --from=builder /tmp/build/contracts /contracts
COPY --from=builder /celes/Docker/config.ini /
COPY --from=builder /etc/celesos-version /etc
COPY --from=builder /celes/Docker/nodcelesd.sh /opt/celesos/bin/nodcelesd.sh
ENV CELESOS_ROOT=/opt/celesos
RUN chmod +x /opt/celesos/bin/nodcelesd.sh
ENV LD_LIBRARY_PATH /usr/local/lib
ENV PATH /opt/celesos/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
