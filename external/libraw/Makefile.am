# autoconf macros
ACLOCAL_AMFLAGS = -I m4
AUTOMAKE_OPTIONS=subdir-objects

LIBRAW_SHLIB_VER = @LIBRAW_SHLIB_VERSION@
LIBRAW_RELEASE_VER = @LIBRAW_RELEASE_VERSION@

# Headers
nobase_include_HEADERS = 	libraw/libraw.h \
				libraw/libraw_alloc.h \
				libraw/libraw_const.h \
				libraw/libraw_datastream.h \
				libraw/libraw_internal.h \
				libraw/libraw_types.h \
				libraw/libraw_version.h

# Docs
doc_DATA =	COPYRIGHT \
			LICENSE.CDDL \
			LICENSE.LGPL \
			Changelog.txt

# pkg-config .pc files
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libraw.pc libraw_r.pc

# Libraries
lib_LTLIBRARIES = lib/libraw.la lib/libraw_r.la

lib_libraw_a_CPPFLAGS = -DLIBRAW_NOTHREADS -w
lib_libraw_a_SOURCES = src/libraw_c_api.cpp \
	src/libraw_datastream.cpp src/decoders/canon_600.cpp \
	src/decoders/crx.cpp src/decoders/pana8.cpp src/decoders/decoders_dcraw.cpp \
	src/decoders/sonycc.cpp src/decompressors/losslessjpeg.cpp  \
	src/decoders/decoders_libraw_dcrdefs.cpp \
	src/decoders/olympus14.cpp \
	src/decoders/decoders_libraw.cpp src/decoders/dng.cpp \
	src/decoders/fp_dng.cpp src/decoders/fuji_compressed.cpp \
	src/decoders/generic.cpp src/decoders/kodak_decoders.cpp \
	src/decoders/load_mfbacks.cpp src/decoders/smal.cpp \
	src/decoders/unpack_thumb.cpp src/decoders/unpack.cpp \
	src/demosaic/aahd_demosaic.cpp src/demosaic/ahd_demosaic.cpp \
	src/demosaic/dcb_demosaic.cpp src/demosaic/dht_demosaic.cpp \
	src/demosaic/misc_demosaic.cpp src/demosaic/xtrans_demosaic.cpp \
	src/integration/dngsdk_glue.cpp src/integration/rawspeed_glue.cpp\
	src/metadata/adobepano.cpp src/metadata/canon.cpp \
	src/metadata/ciff.cpp src/metadata/cr3_parser.cpp \
	src/metadata/epson.cpp src/metadata/exif_gps.cpp \
	src/metadata/fuji.cpp src/metadata/identify_tools.cpp \
	src/metadata/identify.cpp src/metadata/kodak.cpp \
	src/metadata/leica.cpp src/metadata/makernotes.cpp \
	src/metadata/mediumformat.cpp src/metadata/minolta.cpp \
	src/metadata/misc_parsers.cpp src/metadata/nikon.cpp \
	src/metadata/normalize_model.cpp src/metadata/olympus.cpp \
	src/metadata/hasselblad_model.cpp \
	src/metadata/p1.cpp src/metadata/pentax.cpp src/metadata/samsung.cpp \
	src/metadata/sony.cpp src/metadata/tiff.cpp \
	src/postprocessing/aspect_ratio.cpp \
	src/postprocessing/dcraw_process.cpp src/postprocessing/mem_image.cpp \
	src/postprocessing/postprocessing_aux.cpp \
	src/postprocessing/postprocessing_utils_dcrdefs.cpp \
	src/postprocessing/postprocessing_utils.cpp \
	src/preprocessing/ext_preprocess.cpp src/preprocessing/raw2image.cpp \
	src/preprocessing/subtract_black.cpp src/tables/cameralist.cpp \
	src/tables/colorconst.cpp src/tables/colordata.cpp \
	src/tables/wblists.cpp src/utils/curves.cpp \
	src/utils/decoder_info.cpp src/utils/init_close_utils.cpp \
	src/utils/open.cpp src/utils/phaseone_processing.cpp \
	src/utils/read_utils.cpp src/utils/thumb_utils.cpp \
	src/utils/utils_dcraw.cpp src/utils/utils_libraw.cpp \
	src/write/apply_profile.cpp src/write/file_write.cpp \
	src/write/tiff_writer.cpp src/x3f/x3f_parse_process.cpp \
	src/x3f/x3f_utils_patched.cpp 


lib_libraw_r_a_CXXFLAGS = -pthread -w
lib_libraw_r_a_CFLAGS = -pthread -w
lib_libraw_la_SOURCES = $(lib_libraw_a_SOURCES)
lib_libraw_r_la_SOURCES = $(lib_libraw_a_SOURCES)

lib_libraw_la_LDFLAGS = -no-undefined -version-info $(LIBRAW_SHLIB_VER)
lib_libraw_r_la_LDFLAGS = -no-undefined -version-info $(LIBRAW_SHLIB_VER)


# Sample binaries
if EXAMPLES
bin_PROGRAMS =	bin/raw-identify \
		bin/unprocessed_raw \
		bin/4channels \
		bin/rawtextdump \
		bin/simple_dcraw \
		bin/mem_image \
		bin/dcraw_half \
		bin/half_mt \
		bin/multirender_test \
		bin/postprocessing_benchmark \
		bin/dcraw_emu
endif

bin_raw_identify_SOURCES = samples/raw-identify.cpp
bin_raw_identify_CPPFLAGS = $(lib_libraw_a_CPPFLAGS)
bin_raw_identify_LDADD = lib/libraw.la

bin_unprocessed_raw_SOURCES = samples/unprocessed_raw.cpp
bin_unprocessed_raw_CPPFLAGS = $(lib_libraw_a_CPPFLAGS)
bin_unprocessed_raw_LDADD = lib/libraw.la

bin_rawtextdump_SOURCES = samples/rawtextdump.cpp
bin_rawtextdump_CPPFLAGS = $(lib_libraw_a_CPPFLAGS)
bin_rawtextdump_LDADD = lib/libraw.la

bin_4channels_SOURCES = samples/4channels.cpp
bin_4channels_CPPFLAGS = $(lib_libraw_a_CPPFLAGS)
bin_4channels_LDADD = lib/libraw.la

bin_simple_dcraw_SOURCES = samples/simple_dcraw.cpp
bin_simple_dcraw_CPPFLAGS = $(lib_libraw_a_CPPFLAGS)
bin_simple_dcraw_LDADD = lib/libraw.la

bin_multirender_test_SOURCES = samples/multirender_test.cpp
bin_multirender_test_CPPFLAGS = $(lib_libraw_a_CPPFLAGS)
bin_multirender_test_LDADD = lib/libraw.la

bin_postprocessing_benchmark_SOURCES = samples/postprocessing_benchmark.cpp
bin_postprocessing_benchmark_CPPFLAGS = $(lib_libraw_a_CPPFLAGS)
bin_postprocessing_benchmark_LDADD = lib/libraw.la

bin_mem_image_SOURCES = samples/mem_image_sample.cpp
bin_mem_image_CPPFLAGS = $(lib_libraw_a_CPPFLAGS)
bin_mem_image_LDADD = lib/libraw.la

bin_dcraw_half_SOURCES = samples/dcraw_half.c
bin_dcraw_half_CPPFLAGS = $(lib_libraw_a_CPPFLAGS)
bin_dcraw_half_LDADD = lib/libraw.la

bin_half_mt_SOURCES = samples/half_mt.c
bin_half_mt_CFLAGS = $(lib_libraw_r_a_CXXFLAGS)
bin_half_mt_LDADD = lib/libraw_r.la

bin_dcraw_emu_SOURCES = samples/dcraw_emu.cpp
bin_dcraw_emu_CPPFLAGS = $(lib_libraw_a_CPPFLAGS)
bin_dcraw_emu_LDADD = lib/libraw.la
