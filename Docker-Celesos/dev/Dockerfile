FROM celesos/builder
ARG branch=master
ARG symbol=SYS

RUN git clone -b $branch http://test:test@172.12.0.11/celesos/src.git --recursive \
    && cd celes && echo "$branch:$(git rev-parse HEAD)" > /etc/celesos-version \
    && cmake -H. -B"/opt/celesos" -GNinja -DCMAKE_BUILD_TYPE=Release -DWASM_ROOT=/opt/wasm -DCMAKE_CXX_COMPILER=clang++ \
       -DCMAKE_C_COMPILER=clang -DCMAKE_INSTALL_PREFIX=/opt/celesos -DBUILD_MONGO_DB_PLUGIN=true -DCORE_SYMBOL_NAME=$symbol \
    && cmake --build /opt/celesos --target install \
    && cp /celes/Docker/config.ini / && ln -s /opt/celesos/contracts /contracts && cp /celes/Docker/nodcelesd.sh /opt/celesos/bin/nodcelesd.sh && ln -s /celes/tutorials /tutorials

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get -y install openssl ca-certificates vim psmisc python3-pip && rm -rf /var/lib/apt/lists/*
RUN pip3 install numpy
ENV CELESOS_ROOT=/opt/celesos
RUN chmod +x /opt/celesos/bin/nodcelesd.sh
ENV LD_LIBRARY_PATH /usr/local/lib
ENV PATH /opt/celesos/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
