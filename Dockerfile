FROM silkeh/clang:18 AS ltest

RUN apt update && apt install -y git ninja-build valgrind
RUN sh -c "$(wget -O- https://github.com/deluan/zsh-in-docker/releases/download/v1.2.1/zsh-in-docker.sh)" -- \
       -p git
CMD [ "zsh" ]