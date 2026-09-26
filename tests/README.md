# Host Tests

Run the handwritten ZIP-adjacent DEFLATE and XML tests without ESP-IDF:

```sh
cmake -S tests -B build-host-tests
cmake --build build-host-tests
ctest --test-dir build-host-tests --output-on-failure
```

The firmware ZIP reader is exercised through its shared DEFLATE decoder here;
archive-file tests remain hardware/storage integration work.
