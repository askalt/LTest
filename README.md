### Ltest

* Install dependencies (for debian/ubuntu):
```sh
wget -qO- https://apt.llvm.org/llvm.sh | bash -s -- 18
apt install -y git ninja-build valgrind pip clangd
pip install --break-system-packages click
```

* Run for release:
```sh
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release
```

* To run unit tests:
```sh
cmake --build build --target lin_check_test
```

* To run verify:
```sh
./verifying/verify.py run -g nonlinear_queue --tasks 10 --rounds 240 --strategy pct
```