# Profiling

There are many tools available for profiling the runtime and memory usage of
an application. If you can access and know how to use tools like perf, Intel
VTune, Valgrind, etc, feel free to use them for profiling.

The RawTherapee codebase currently has a few integrated profiling methods.

1. A primitive `StopWatch` RAII object (no dependencies)
2. `GNU gprof` (UNIX only)
3. Tracy profiler (requires building/installing the profiler GUI)

In general, prefer using `Release` or `RelWithDebInfo` builds when profiling.

## `StopWatch`

1. Include `rtengine/StopWatch.h`
2. Enable benchmarking
    1. Configure CMake with `-DWITH_BENCHMARK="ON"`, or
    2. Add `#define BENCHMARK` manually before including the `StopWatch` header
3. Use the `BENCHFUN` or `BENCHFUNMICRO` at the start of a function

For more fine-grained control, you can instantiate the `StopWatch` object to
time a specific scope.

Make sure to remove any manually added `#define BENCHMARK` and `StopWatch`
instances from the code base before submitting a PR. This is so profiling code
doesn't affect regular builds.

## `gprof`

[`gprof`](https://ftp.gnu.org/old-gnu/Manuals/gprof-2.9.1/html_mono/gprof.html)
is a performance analysis tool for UNIX applications.

1. Configure CMake with `-DWITH_PROF="ON"`
2. Run `gprof rawtherapee-cli`
3. View results in `gmon.out`

The recommendation is to profile `rawtherapee-cli` and not the GUI application
as user input can mess with `gprof`'s collected data.

## Tracy

[Tracy](https://github.com/wolfpld/tracy/) is a powerful open source tracing-
style profiler. The GitHub's releases includes a link/download to the Tracy
documentation ([tracy.pdf](https://github.com/wolfpld/tracy/releases/latest/download/tracy.pdf)).

There is also a [YouTube video](https://www.youtube.com/watch?v=ghXk3Bk5F2U)
that goes over the basics with an accompanying
[slide deck](https://github.com/CppCon/CppCon2023/blob/main/Presentations/CppCon_2023_-_Tracy_Profiler.pdf).

You should reference the documentation for all the ways to setup and use Tracy.
Basic setup/usage for RawTherapee is provided below.

### Setup

To enable profiling with Tracy, configure CMake with
`-DWITH_TRACY_PROFILER="ON"`. To additionally enable memory profiling,
also include `-DWITH_TRACY_MEMORY_PROFILING=ON`.

To view the profiling data, you also need to acquire the Tracy profiling
server/GUI. The version of the server should match the version of the client
code. RawTherapee is currently using
[**v0.14.0**](https://github.com/wolfpld/tracy/releases/tag/v0.14.0).

There are a few ways to obtain the profiling server:

- On Windows, the Tracy release page contains a prebuilt executable
- Install using your favorite package manager
- Build the profiling server locally

By default, we configure Tracy's server-client connection to be on localhost
only. If you have extra computers, it is advised to run the profiler from a
separate computer from RawTherapee. The profiling data will be transferred over
the network. To enable this flow, additionally configure CMake with
`-DWITH_TRACY_ONLY_LOCALHOST="OFF" -DWITH_TRACY_NO_BROADCAST="OFF"`.

#### Building the Server

Follow the Tracy documentation PDF section 2.3 - Building the server.

```bash
git clone --depth 1 --single-branch --branch v0.14.0 https://github.com/wolfpld/tracy.git
cd tracy
cmake -G Ninja -DCMAKE_BUILD_TYPE="Release" -S profiler -B profiler/build
cmake --build profiler/build
# Run the Tracy profiler server
./profiler/build/tracy-profiler
```

### Code Annotations

We wrap Tracy's profiling macros with our own in `rtengine/profiling.h`. Not
all of Tracy's features have been wrapped. If you need additional
functionality, consult Tracy's documentation and add a wrapper macro.

The most common macro is `RT_PROFILE(NAME, TAG)` which wraps Tracy's
`ZoneNamed` macro. It takes a constexpr string literal for the profiling zone's
name. It also takes a profiling tag (see `Profiling::Tag`). These tags are for
use with `Profiling::ZONE_FILTER` which acts as compile-time toggles to limit
which zones have data collected.

```cpp
void func()
{
    RT_PROFILE("Put whatever zone text", GUI_EDITOR);
}
```

### Profiling

Modify `Profiling::ZONE_FILTER` in `rtengine/profiling.h` to limit which
profiling zones you want to view. Rebuild RawTherapee to apply your changes.

1. Run the Tracy profiler GUI
2. Tell the profiler to search for connections
3. Run RawTherapee
4. Perform some actions
5. Close RawTherapee and stop the profiler from recording more data

Since Tracy starts collecting data immediately on program execution, there may
be a lot of irrelevant data when profiling the GUI due to waiting for user
interaction. Configure CMake with `-DWITH_TRACY_ON_DEMAND="ON"` to only gather
data in RawTherapee when the profiling server is connected. If so, the
profiling flow is changed slightly.

1. Run RawTherapee GUI
2. Setup up GUI to before the interaction you want to profile
3. Run the Tracy profiler GUI and connect
4. Perform a user interaction to profile
5. Disconnect the Tracy profiler
