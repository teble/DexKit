# Native smali checks

Run the checks without JNI or the existing fail-fast DEX loader:

```sh
cmake -S Core -B /tmp/dexkit-smali-tests -G Ninja \
  -DDEXKIT_BUILD_SMALI_TESTS=ON -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_FLAGS='-fno-rtti -fno-exceptions'
cmake --build /tmp/dexkit-smali-tests --target dexkit_smali_tests
ctest --test-dir /tmp/dexkit-smali-tests --output-on-failure
```

The fixtures exercise the new bounded reader directly, including malformed data
the existing bridge loader might reject before a smali request is possible.
For host sanitizer validation, add `-fsanitize=address,undefined
-fno-omit-frame-pointer` to CMake C++ and executable linker flags in a separate
build directory. These tests are excluded from normal library builds.
