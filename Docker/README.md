# Run in docker

Simple and fast setup of CELES.OS on Docker is also available.

## Install Dependencies

- [Docker](https://docs.docker.com) Docker 17.05 or higher is required
- [docker-compose](https://docs.docker.com/compose/) version >= 1.10.0

## Docker Requirement

- At least 7GB RAM (Docker -> Preferences -> Advanced -> Memory -> 7GB or above)
- If the build below fails, make sure you've adjusted Docker Memory settings and try again.

## Build celes image

```bash
<<<<<<< HEAD
git clone https://github.com/CELESOS/celes.git --recursive  --depth 1
cd celes/Docker
docker build . -t celesos/celes
=======
git clone https://github.com/EOS-Mainnet/eos.git --recursive  --depth 1
cd eos/Docker
docker build . -t eosio/eos
>>>>>>> eos/tags/v1.4.3
```

The above will build off the most recent commit to the master branch by default. If you would like to target a specific branch/tag, you may use a build argument. For example, if you wished to generate a docker image based off of the v1.4.3 tag, you could do the following:

```bash
<<<<<<< HEAD
docker build -t celesos/celes:v1.4.2 --build-arg branch=v1.4.2 .
=======
docker build -t eosio/eos:v1.4.3 --build-arg branch=v1.4.3 .
>>>>>>> eos/tags/v1.4.3
```

By default, the symbol in celesos.system is set to SYS. You can override this using the symbol argument while building the docker image.

```bash
docker build -t celesos/celes --build-arg symbol=<symbol> .
```

## Start nodceles docker container only

```bash
docker run --name nodceles -p 8888:8888 -p 9876:9876 -t celesos/celes nodcelesd.sh -e --http-alias=nodceles:8888 --http-alias=127.0.0.1:8888 --http-alias=localhost:8888 arg1 arg2
```

By default, all data is persisted in a docker volume. It can be deleted if the data is outdated or corrupted:

```bash
$ docker inspect --format '{{ range .Mounts }}{{ .Name }} {{ end }}' nodceles
fdc265730a4f697346fa8b078c176e315b959e79365fc9cbd11f090ea0cb5cbc
$ docker volume rm fdc265730a4f697346fa8b078c176e315b959e79365fc9cbd11f090ea0cb5cbc
```

Alternately, you can directly mount host directory into the container

```bash
docker run --name nodceles -v /path-to-data-dir:/opt/celesos/bin/data-dir -p 8888:8888 -p 9876:9876 -t celesos/celes nodcelesd.sh -e --http-alias=nodceles:8888 --http-alias=127.0.0.1:8888 --http-alias=localhost:8888 arg1 arg2
```

## Get chain info

```bash
curl http://127.0.0.1:8888/v1/chain/get_info
```

## Start both nodceles and kcelesd containers

```bash
docker volume create --name=nodceles-data-volume
docker volume create --name=kcelesd-data-volume
docker-compose up -d
```

After `docker-compose up -d`, two services named `nodcelesd` and `kcelesd` will be started. nodceles service would expose ports 8888 and 9876 to the host. kcelesd service does not expose any port to the host, it is only accessible to clceles when running clceles is running inside the kcelesd container as described in "Execute clceles commands" section.

### Execute clceles commands

You can run the `clceles` commands via a bash alias.

```bash
alias clceles='docker-compose exec kcelesd /opt/celesos/bin/clceles -u http://nodcelesd:8888 --wallet-url http://localhost:8900'
clceles get info
clceles get account inita
```

Upload sample exchange contract

```bash
clceles set contract exchange contracts/exchange/
```

If you don't need kcelesd afterwards, you can stop the kcelesd service using

```bash
docker-compose stop kcelesd
```

### Develop/Build custom contracts

Due to the fact that the celesos/celes image does not contain the required dependencies for contract development (this is by design, to keep the image size small), you will need to utilize the celesos/celes-dev image. This image contains both the required binaries and dependencies to build contracts using eosiocpp.

You can either use the image available on [Docker Hub](https://hub.docker.com/r/celesos/celes-dev/) or navigate into the dev folder and build the image manually.

```bash
cd dev
docker build -t celesos/celes-dev .
```

### Change default configuration

You can use docker compose override file to change the default configurations. For example, create an alternate config file `config2.ini` and a `docker-compose.override.yml` with the following content.

```yaml
version: "2"

services:
  nodceles:
    volumes:
      - nodceles-data-volume:/opt/celesos/bin/data-dir
      - ./config2.ini:/opt/celesos/bin/data-dir/config.ini
```

Then restart your docker containers as follows:

```bash
docker-compose down
docker-compose up
```

### Clear data-dir

The data volume created by docker-compose can be deleted as follows:

```bash
docker volume rm nodceles-data-volume
docker volume rm kcelesd-data-volume
```

### Docker Hub

Docker Hub image available from [docker hub](https://hub.docker.com/r/celesos/celes/).
Create a new `docker-compose.yaml` file with the content below

```bash
version: "3"

services:
  nodcelesd:
    image: celesos/celes:latest
    command: /opt/celesos/bin/nodcelesd.sh --data-dir /opt/celesos/bin/data-dir -e --http-alias=nodcelesd:8888 --http-alias=127.0.0.1:8888 --http-alias=localhost:8888
    hostname: nodcelesd
    ports:
      - 8888:8888
      - 9876:9876
    expose:
      - "8888"
    volumes:
      - nodceles-data-volume:/opt/celesos/bin/data-dir

  kcelesd:
    image: celesos/celes:latest
    command: /opt/celesos/bin/kcelesd --wallet-dir /opt/celesos/bin/data-dir --http-server-address=127.0.0.1:8900 --http-alias=localhost:8900 --http-alias=kcelesd:8900
    hostname: kcelesd
    links:
      - nodcelesd
    volumes:
      - kcelesd-data-volume:/opt/celesos/bin/data-dir

volumes:
  nodceles-data-volume:
  kcelesd-data-volume:

```

*NOTE:* the default version is the latest, you can change it to what you want

run `docker pull celesos/celes:latest`

run `docker-compose up`

### CELESOS Testnet

We can easily set up a CELESOS local testnet using docker images. Just run the following commands:

Note: if you want to use the mongo db plugin, you have to enable it in your `data-dir/config.ini` first.

```
# create volume
docker volume create --name=nodceles-data-volume
docker volume create --name=kcelesd-data-volume
# pull images and start containers
docker-compose -f docker-compose-celesos-latest.yaml up -d
# get chain info
curl http://127.0.0.1:8888/v1/chain/get_info
# get logs
docker-compose logs -f nodcelesd
# stop containers
docker-compose -f docker-compose-celesos-latest.yaml down
```

The `blocks` data are stored under `--data-dir` by default, and the wallet files are stored under `--wallet-dir` by default, of course you can change these as you want.

### About MongoDB Plugin

Currently, the mongodb plugin is disabled in `config.ini` by default, you have to change it manually in `config.ini` or you can mount a `config.ini` file to `/opt/celesos/bin/data-dir/config.ini` in the docker-compose file.
