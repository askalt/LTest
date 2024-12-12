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
* Blocking:
It's required to install boost and folly locally, and provide ./boost and ./folly symbolic links at the path of the project, then we can lincheck lock_guard from folly:
```sh
cmake --build build --target verifying/blocking/mutexed_register && LD_PRELOAD=build/syscall_intercept/libpreload.so ./build/verifying/blocking/mutexed_register --syscall_trap
```