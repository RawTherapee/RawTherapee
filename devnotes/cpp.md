# C++

We are currently using C++17 ([cppreference](https://en.cppreference.com/w/cpp/compiler_support/17)).

All core language features are allowed, but only a subset of library features
should be used due to lack of support on older compilers.

## Library Features to Avoid

- Parallel algorithms and execution policies ([P0024R2](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0024r2.html))
- [P0220R1](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0220r1.html)
  - [Polymorphic memory resources](https://en.cppreference.com/w/cpp/header/memory_resource.html)
  - [`std::apply`](https://en.cppreference.com/w/cpp/utility/apply.html)
  - [Searchers](https://en.cppreference.com/w/cpp/functional.html#Searchers)
  - [`std::sample`](https://en.cppreference.com/w/cpp/algorithm/sample.html)
- [Mathematical special functions](https://en.cppreference.com/w/cpp/numeric/special_math.html) ([P0226R1](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0226r1.pdf))
- [`constexpr std::addressof`](https://en.cppreference.com/w/cpp/memory/addressof.html) ([LWG2296](https://cplusplus.github.io/LWG/issue2296))
- [P0067R5](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0067r5.html)
  - [`std::to_chars`](https://en.cppreference.com/w/cpp/utility/to_chars.html)
  - [`std::from_chars`](https://en.cppreference.com/w/cpp/utility/from_chars.html)
- `std::shared_ptr` and `std::weak_ptr` with array support ([P0414R2](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0414r2.html))
- `std::shared_ptr<T[]>` ([P0497R0](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0497r0.html))
- [Hardware interference size](https://en.cppreference.com/w/cpp/thread/hardware_destructive_interference_size.html) ([P0154R1](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0154r1.html))
- [`std::hash<std::filesystem::path>`](https://en.cppreference.com/w/cpp/filesystem/path/hash.html) ([LWG3657](https://cplusplus.github.io/LWG/issue3657))
