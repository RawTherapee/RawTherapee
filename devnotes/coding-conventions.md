# Coding Conventions

Try to follow the developer coding conventions. If a file does not follow the
conventions, try to match existing conventions in the file or local scope.
Otherwise, stay locally/internally consistent with your own changes.

## C/C++

### Style

- To break header dependencies use forward declarations as much as possible.
  See [#5197](https://github.com/RawTherapee/RawTherapee/pull/5197#issuecomment-468938190) for some tips.
- The naming isn't homogeneous throughout the code but here is a rough guideline:
  - *Identifiers* (variables, functions, methods, keys, enums, etc.) should be clear and unambiguous.
    - Make them as long as necessary to ensure that your code is understandable to others.
  - *Types* (classes, structs, enums, typedefs...) should be named with `UpperCamelCase`.
  - *Functions* and *methods* should be named with `lowerCamelCase`.
  - *Variables* should be named with `lower_underscores`.
  - *Enum values*, *constants*, and *macros* should be named with `UPPER_UNDERSCORES`.
  - Be consistent, even when not sticking to the rules.

### Formatting

#### Old

Some code is formatted using astyle version 3 or newer.

```bash
astyle --options=rawtherapee.astylerc code.cc
```

#### New

[clang-format](https://clang.llvm.org/docs/ClangFormat.html) 18
is used to automatically format files. The files that should be auto-formatted
can be found in the [.clang-format-ignore](.clang-format-ignore) whitelist.

```bash
# Run formatting checks
/path/to/clang-format-18 \
    --style=file --fallback-style=none \
    --Werror --ferror-limit=5 \
    --dry-run FILES...

# Format code in-place
/path/to/clang-format-18 \
    --style=file --fallback-style=none \
    --Werror --ferror-limit=0 \
    -i FILES...

# Format all using find
find . -type f \( -name "*.h" -o -name "*.hh" -o -name "*.hpp" -o -name "*.c" -o -name "*.cc" -o -name "*.cpp" \) | \
    xargs /path/to/clang-format-18 \
    --style=file --fallback-style=none --Werror --ferror-limit=0 -i

# Format all using [fd](https://github.com/sharkdp/fd)
fd -e h -e hh -e hpp -e c -e cc -e cpp -X \
    /path/to/clang-format-18 \
    --style=file --fallback-style=none --Werror --ferror-limit=0 -i
```

If your version of clang-format is not 18, you will need to use a package
manager with the specific version or
[download the binary](https://github.com/llvm/llvm-project/releases/tag/llvmorg-18.1.8)
from official LLVM releases. Download the appropriate `clang+llvm` tarball and
use the provided standalone `bin/clang-format` executable in it. The 18.1.8
release has prebuilt binaries for Linux, Windows, and MacOS (ARM). If all else
fails, you can build clang 18 from source.

The prebuilt clang-format 18.1.8 executable from the LLVM repo may require you
to install `ncurses5` if you get an error about `libtinfo.so.5`.

### Tips for clang-format

Disable format for a block of code using `// clang-format off/on`

```cpp
// clang-format off
keep_formatting_for_this_code();
// clang-format on
```

Keep each element of an initializer list on different lines by adding a
trailing comma.

```cpp
// If no trailing comma is present, clang-format tends to bin-pack the elements
// when they exceed the line length limit.
std::array bin_packed_array = { 0, 1,
                                2 };

std::array desired_format_array = {
    0,
    1,
    2,  // Trailing comma
};
```

## Python

- Follow PEP 8 [naming conventions](https://peps.python.org/pep-0008/#naming-conventions)
- Format by running [black](https://github.com/psf/black) with default settings

An easy way of running black is to use [uv](https://docs.astral.sh/uv/). It
also lets you create one-off scripts with extra dependencies.

```bash
# Create a one-off script that has dependencies managed by uv
uv init --script YOUR_SCRIPT.py
# Add dependencies to the script
uv add --script YOUR_SCRIPT.py numpy onnx
# Format your code
uv tool run black YOUR_SCRIPT.py
# Run the script and uv will manage/install dependencies automatically
uv run YOUR_SCRIPT.py
```

## Coding Style Tips

Treat the following recommendations as soft/loose guidelines that will improve
the quality of new and refactored code.

### Language Agnostic Tips

#### Function Length

Functions shouldn't be too long. No one likes reading/debugging a 1000+ line
function that does 20 different things. It can also cause issues for variable
scoping, compiler optimizations, and debuggers.

A good length for functions is usually < 100 lines, but they can be longer if
subdividing the function into smaller ones hurts readability.

If your function is long and has separate blocks of tasks, you can extract the
blocks into their own functions and call them instead.

```cpp
// Poor
void run()
{
    // Find candidates
    // ...

    // Process candidates
    // ...

    // Cleanup
    // ...
}

// Better
void find() {}
void process() {}
void cleanup() {}

void run()
{
    find();
    process();
    cleanup();
}
```

#### Guard Clauses/Early Returns

If you have a deeply nested `if` statements, you can reduce nesting by using
"guard clauses". These work especially well in loops and smaller functions.

```cpp
// Poor
void func() {
    if (a) {
        if (b) {
            if (c) {
                // Do stuff
            } else {
                return;
            }
        } else {
            return;
        }
    } else {
        return;
    }
}

// Better
void func() {
    if (!a) return;
    if (!b) return;
    if (!c) return;

    // Do stuff
}
```

```cpp
// Poor
while (true) {
    if (a) {
        if (b) {
            // Do stuff
        } else {
            break;
        }
    } else {
        continue;
    }
}

// Better
while (true) {
    if (!a) continue;
    if (!b) break;

    // Do stuff
}
```

#### Minimize using magic numbers

"Magic numbers" are hard-coded numeric literals with a special unexplained
meaning. Some numbers (e.g., 0, 1, 255, 65535) are ok due to being obvious in
context or easier to understand than a longer variable name. However, when the
value is more "non-standard" or used in multiple places, understanding and
modifying the code becomes more difficult and error-prone.

Prefer defining constant variables and using them instead of sprinkling numeric
literals throughout your code.

```cpp
enum SCALING_METHOD {
    NEAREST_NEIGHBOUR = 0,
    BILINEAR = 1,  // '= 1' is unnecessary as C/C++ automatically increments
};

constexpr int NEAREST_NEIGHBOUR_SCALING = 0;
constexpr int BILINEAR_SCALING = 1;

// You can define constants with structs and data structures too.
constexpr std::array<int, 3> RGB_GREEN = {0, 255, 0};
```

### C++ Tips

Many concepts here are from the STL. See the [official reference](https://cppreference.com).

#### Memory management

Use smart pointers like `std::unique_ptr` and `std::shared_ptr` instead of raw
C pointers (e.g. `Foo*`) when modeling ownership. They reduce memory leaks and
help developers reason about pointer lifetimes.

Prefer using `std::unique_ptr` most of the time as shared ownership (i.e.
`std::shared_ptr`) is more difficult to reason about. You can return
`std::unique_ptr` from functions to tell the caller that they need to manage
the lifetime of the returned pointer. You can also take `std::unique_ptr<T>&&`
as a function parameter to indicate that the function will take ownership of
the pointer.

For code that stores a pointer but does not manage the lifetime (i.e. freeing
the pointer), a simple raw pointer is sufficient. For functions that take raw
pointers (and don't manage lifetime), a smart pointer can be passed by calling
the `get()` member function to extract the raw pointer.

This rule is less applicable to GTK code due to the use of `Gtk::managed()`.

#### RAII

C++'s constructors and destructors can be used to simplify and reduce bugs
related to lifetime management. When an object is constructed, some resource is
opened. When the object is destroyed, the resource is closed.

Existing examples include smart pointers (e.g. `std::unique_ptr`), file
handling (e.g. `std::ofstream`), and concurrency
(`std::lock_guard<std::mutex> lock(mtx)`).

#### Range-based `for` loops

When you want to iterate over a data structure and don't need the index, use
a range-based `for` loop. This is particularly useful for iterating over arrays
and lists.

```cpp
// Good
for (auto it = vec.begin(); it != vec.end(); it++) {
    Type& entry = *it;
    func(entry);
}

// Better
for (const auto& entry : vec) {  // Remove const as needed
    func(entry);
}
```

#### Prefer `std::vector` and `std::array` over raw C arrays

Prefer using `std::vector` for dynamically-sized arrays.

Prefer `std::array<T, N> my_array` over `T my_array[N]` for fixed-size
arrays as it allows C++ utilities like range-based for loops and integration
with `<algorithm>` header functions.

`std::array` can be passed to functions that need the raw C array by calling
the `data()` member function.

In most cases, prefer `std::vector` over `std::list` since it is faster due to
better cache locality.

### `auto` type deduction

`auto` allows the compiler to deduce the type automatically. This may hurt
readability. It should be used sparingly except for `for` loops, iterators,
lambdas (and sometimes their arguments), templates, trailing return types, and
where the deduced type is obvious.

```cpp
auto ptr = std::make_unique<Foo>();
auto gtk_box = Gtk::manage(new Box());

std::unordered_map<int> map;
auto it = map.find(target);

std::vector<int> vec;
auto start = vec.begin();

for (const auto& it : vec) {}

auto lambda = [](auto arg_type_may_depend_on_lambda_callsite_usage) {};
```

#### String formatting

Prefer using the more modern [fmt](https://fmt.dev) library for string
formatting as it is both faster, safer, and more ergonomic. Future C++ versions
(i.e. C++20 and above) incorporate this library in the STL, but the standalone
library remains more up-to-date on features, performance, and security.

```cpp
#include <fmt/format.h>

void func()
{
    std::string filepath = "foo.jpg";
    int width = 1920;
    int height = 1080;
    std::string text = fmt::format("Image '{}' has dimensions {}x{}",
                                   filepath, width, height);
    // Output: Image 'foo.jpg' has dimensions 1920x1080
}
```
