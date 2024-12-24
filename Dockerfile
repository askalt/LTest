FROM silkeh/clang:18 AS ltest
RUN apt update && apt install -y git ninja-build valgrind libboost-context-dev
