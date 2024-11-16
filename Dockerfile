FROM silkeh/clang:18 as ltest

RUN apt update && apt install -y git ninja-build valgrind pip clangd
RUN pip install --break-system-packages click

# cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Debug
# ./verifying/verify.py run -g nonlinear_queue --tasks 4 --rounds 100000 --strategy rr --switches 1