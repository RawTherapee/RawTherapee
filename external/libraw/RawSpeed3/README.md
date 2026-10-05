# About RawSpeed v3

The current version of the RawSpeed library (https://github.com/darktable-org/rawspeed ) does not have a version number, we call it "version 3" to distinguish it from RawSpeed/master, the version number is taken from darktable 3.x (which uses this library).

This library provides faster decoding of some RAW formats (even faster than RawSpeed v1), the difference can be tens of percent (without using OpenMP) for Huffman-compressed files (Canon CR2, Nikon NEF, etc).

Unfortunately, the RawSpeed-v3 source code is constantly changing and this is due not so much to the development of the library (support for new cameras, formats, etc.), but to the fact that the current maintainer is using it as an aid in self-development as a programmer.

They are extremely reluctant to accept 3rd-party patches to this library, therefore, the fixes necessary for correct work with LibRaw have been added to the LibRaw distribution (see below). All our patches, of course, are suitable only for a specific version (commit-id) of RawSpeed-v3, if you want to use a different version, you will have to adjust them.

# Building RawSpeed v3 and C-API wrapper

## Preparing the source
1. Take a specific commit from github
   https://github.com/darktable-org/rawspeed/commit/de70ef5fbc62cde91009c8cff7a206272abe631e

2. Apply the following patches from LibRaw/RawSpeed3/patches folder:
  * **01.CameraMeta-extensibility.patch** - allows derived classes from CameraMeta, which allows loading camera descriptions not only from a disk file.
  * **02.Makernotes-processing.patch** - fixes an error in processing the Makernotes tag.
  * **03.remove-limits-and-logging.patch** - removes debug printing and file size limits. This patch is optional, but if you are going to decode files from cameras that were not available at the time a particular version of RawSpeed was created, then this patch can be useful.
  * **04.clang-cl-compatibility.patch -** fixes for compatibility with Microsoft C++ library.
  * **05.no-phase-one-correction.patch -** disables RawSpeed's PhaseOne flat field correction because LibRaw implements own

## Building the RawSpeed-v3 library

We **do not** offer advice on building RawSpeed3, if you need such advice please contact the maintainer of the library. At a minimum, it **can be** compiled with Clang (XCode) on macOS and clang-cl (MSVC) on Windows.

Use your favorite build system. Files for CMake come with RawSpeed, but any other build system can be used (in that case you probably will need to create your own rawspeedconfig.h instead of one created via CMake).

In our projects we use RawSpeed-v3 without OpenMP.

## Building the C-API wrapper
To simplify the integration with LibRaw, we have implemented a simple wrapper with a C interface, which hides everything unnecessary, thus:
* To build LibRaw (with RawSpeed-v3 support) you do not need access to RawSpeed .h-files
* When building LibRaw and RawSpeed-v3 using different compilers there is no issue with C++ name mangling.

The wrapper sources are in the **LibRaw/RawSpeed3/rawspeed3_c_api folder**, they include four files:
1. **rawspeed3_capi.h** - header file
2. **rawspeed3_capi.cpp** - wrapper sources
3. **rsxml2c.sh** - shell script that will convert RawSpeed/data/cameras.xml to a C++ file containing camera definitions
4. **rawspeed3_capi_test.cpp** - test program for checking the build correctness and operation ability.

The wrapper provides simple *decode only* interface (init library, decode RAW passed via buffer, free decoded file buffer, release the library) it is self-documented via comments in the rawspeed3_capi.h file.

To make a file with camera definitions, run the command (you can add it to the build system):
`sh ./rawspeed3_c_api/rsxml2c.sh < path/to/RawSpeed/data/cameras.xml > rawspeed3_c_api/cameras.cpp`

The **rsxml2c.sh** script requires **cat**, **tr**, and **sed** unix command-line utilities installed, there is no specific version requirements.

Add the resulting file (LibRaw/RawSpeed3/rawspeed3_c_api/cameras.cpp) and LibRaw/RawSpeed3/rawspeed3_c_api/rawspeed3_capi.cpp, to the build of the RawSpeed-v3 (dynamic) library, which you learned to build in the previous step.
If building Windows DLL: rawspeed3_capi.cpp should be compiled with -DRAWSPEED_BUILDLIB to create dll export entries automatically.

To check that everything works, you can use the LibRaw/RawSpeed3/rawspeed3_c_api/rawspeed3_capi_test.cpp test program, it should be possible to build it and link with RawSpeed-v3 (+ C-API); and after that this test program should be ready to decode RAW files passed to it from command line.

# Building LibRaw with RawSpeed-v3 support
When building, specify the following parameters
* LibRaw/RawSpeed3/rawspeed3_c_api to the search path for include files
* Specify preprocessor flags -DUSE_RAWSPEED3 -DUSE_RAWSPEED_BITS:
   * USE_RAWSPEED3 enables the use of RawSpeed-v3
   * USE_RAWSPEED_BITS enables (a more granulated) control of RawSpeed-v3 / RawSpeed processing
* set the RawSpeed-v3 (+ C-API) library, obtained in the previous steps, as an input linker file

# Using LibRaw with RawSpeed-v3 Support
Use LibRaw as usual. Enable RawSpeed3 use by setting bits in imgdata.rawparams.userawspeed (LIBRAW_RAWSPEEDV3_USE, LIBRAW_RAWSPEEDV3_IGNOREERRORS, LIBRAW_RAWSPEEDV3_FAILONUNKNOWN)
If RawSpeed was set as the decoder the following bits will be set in imgdata.processwarnings:
* LIBRAW_WARN_RAWSPEED3_PROCESSED - if decoding was successful
* LIBRAW_WARN_RAWSPEED3_PROBLEM - if decoding attempt failed
(flags and bits are described in LibRaw's Changelog)

With RawSpeed-v3, the entire RAW file is read into the memory buffer (as it is with RawSpeed version 1), so it increases LibRaw memory footprint.
