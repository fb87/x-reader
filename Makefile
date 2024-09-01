# vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab

LXC_MGR     ?= $(shell which podman)
BLD_CMD     := ${LXC_MGR} run --rm -it -v $PWD:/work -w /work idf

all: build

build:
    ${BLD_CMD} bash -c 'idf build'

flash:
    ${BLD_CMD} bash -c 'idf build'

clean:
    ${BLD_CMD} bash -c 'idf fullclean'

