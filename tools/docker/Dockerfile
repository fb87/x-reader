FROM		ubuntu:24.04

ENV		IDF_TOOLS_PATH /usr/share
ENV		IDF_TOOLS_WRPR /usr/bin/idf

RUN		apt update -y
RUN		apt install -y git wget flex bison gperf python3 python3-pip python3-venv cmake \
                               ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0 

RUN		mkdir -p ${IDF_TOOLS_PATH} && cd ${IDF_TOOLS_PATH} && \
                git clone -b v5.3 --recursive --shallow-submodules --depth=1 https://github.com/espressif/esp-idf.git

RUN		cd ${IDF_TOOLS_PATH}/esp-idf && ./install.sh all

RUN		cat <<-EOF > ${IDF_TOOLS_WRPR}
#!/usr/bin/env bash
. ${IDF_TOOLS_PATH}/esp-idf/export.sh > /dev/null
idf.py \$@
EOF
RUN		chmod +x ${IDF_TOOLS_WRPR}

CMD		[ "${IDF_TOOLS_WRPR}" ]
