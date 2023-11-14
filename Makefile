
BOOM_ROOT := $(abspath $(shell pwd)/..)

IMAGE := chipyard-slim
CONTAINER := building-chipyard
SCRIPT := $(BOOM_ROOT)/build/build.sh

MOUNT += -v $(BOOM_ROOT)/src/main/scala:/root/chipyard/generators/boom/src/main/scala
MOUNT += -v $(BOOM_ROOT)/build:/root/chipyard/sims/verilator/build
MOUNT += -v $(BOOM_ROOT)/hwd-prefetch-study/sifive-cache/design:/root/chipyard/generators/sifive-cache/design
MOUNT += -v $(BOOM_ROOT)/hwd-prefetch-study/BankedL2Params.scala:/root/chipyard/generators/rocket-chip/src/main/scala/subsystem/BankedL2Params.scala

CONFIG ?= MediumBoomConfig
MAKE_VAR := VERILATOR_THREADS=8 JAVA_HEAP_SIZE=32G FIRRTL_LOGLEVEL=warn

CMD += cd /root/chipyard/sims/verilator &&
CMD += make clean &&
CMD += make CONFIG=$(CONFIG) $(MAKE_VAR) -j $(shell nproc) &&
CMD += cp simulator-* build/

all: simulator-chipyard-$(CONFIG)

simulator-chipyard-$(CONFIG):
	mkdir -p $(shell dirname $(SCRIPT))
	echo "#!/bin/bash" > $(SCRIPT)
	chmod a+x $(SCRIPT)
ifeq ($(shell podman ps -aq -f name=$(CONTAINER)),)
	podman run -it --name $(CONTAINER) $(MOUNT) $(IMAGE) /root/chipyard/sims/verilator/build/build.sh
endif
	echo "$(CMD)" >> $(SCRIPT)
	podman start -ai $(CONTAINER)

clean:
	podman rm -i $(CONTAINER)
