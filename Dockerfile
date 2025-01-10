FROM silkeh/clang:18 AS ltest

RUN apt update && apt install -y git ninja-build valgrind libgflags-dev

FROM ltest as blocking
RUN apt install -y pkg-config libcapstone-dev && \
    git clone https://github.com/Kirillog/syscall_intercept.git &&  \
    cmake syscall_intercept -G Ninja -B syscall_intercept/build -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang && \
    cmake --build syscall_intercept/build --target install
RUN sh -c "$(wget -O- https://github.com/deluan/zsh-in-docker/releases/download/v1.2.1/zsh-in-docker.sh)" -- \
       -p git
CMD [ "zsh" ]