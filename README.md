### Ltest

* Build

It is not recommended to try install all required dependencies locally in your system. Build docker image and run container:
```sh
./scripts/rund.sh
```

* Run for release:
```sh
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release
```

* Run unit tests:
```sh
cmake --build build --target lin_check_test
```

* Run verify:
```sh
cmake --build build --target verifying/targets/nonlinear_queue && ./build/verifying/targets/nonlinear_queue --tasks 10 --rounds 240 --strategy pct
```