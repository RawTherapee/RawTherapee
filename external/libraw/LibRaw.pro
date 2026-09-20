TEMPLATE=subdirs
CONFIG+=ordered
SUBDIRS= \
	buildfiles/dcraw_emu.pro \
	buildfiles/libraw.pro \
	buildfiles/postprocessing_benchmark.pro \
	buildfiles/dcraw_half.pro \
#	buildfiles/half_mt.pro \   
	buildfiles/mem_image.pro \
	buildfiles/raw-identify.pro \
	buildfiles/simple_dcraw.pro \
	buildfiles/multirender_test.pro \
	buildfiles/unprocessed_raw.pro \
	buildfiles/4channels.pro  \
	buildfiles/rawtextdump.pro  \
	buildfiles/openbayer_sample.pro  

CONFIG-=qt

