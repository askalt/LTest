time ./build/verifying/targets/atomic_register --tasks 1000 --rounds 2000 --strategy random
 --threads 250 --verbose false --switches 100
# real    0m17.835s
# user    0m17.410s
# sys     0m0.421s
time ./build-old/verifying/targets/atomic_register --tasks 1000 --rounds 2000 --strategy random
 --threads 250 --verbose false --switches 100
# real    0m42.766s
# user    0m24.256s
# sys     0m18.501s