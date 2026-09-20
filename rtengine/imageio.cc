/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2004-2010 Gabor Horvath <hgabor@rawtherapee.com>
 *  Copyright (c) 2010 Oliver Duis <www.oliverduis.de>
 *
 *  RawTherapee is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  RawTherapee is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with RawTherapee.  If not, see <https://www.gnu.org/licenses/>.
 */
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#ifdef LIBJXL
#include "jxl/decode_cxx.h"
#include "jxl/resizable_parallel_runner_cxx.h"
#endif

#include <fcntl.h>
#include <glib/gstdio.h>
#include <jpeg_ijg/jpeg.h>
#include <png.h>
#include <tiff.h>
#include <tiffio.h>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif

#include "color.h"
#include "iccjpeg.h"
#include "imagedata.h"
#include "imageio.h"
#include "procparams.h"
#include "rt_math.h"
#include "settings.h"
#include "utils.h"

#include "rtgui/options.h"
#include "rtgui/version.h"


using namespace std;
using namespace rtengine;
using namespace rtengine::procparams;

namespace rtengine { extern const Settings *settings; }

namespace
{

// Opens a file for binary writing and request exclusive lock (cases were you need "wb" mode plus locking)
FILE* g_fopen_withBinaryAndLock(const Glib::ustring& fname)
{

#ifdef _WIN32

    // Use native function to disallow sharing, i.e. lock the file for exclusive access.
    // This is important to e.g. prevent Windows Explorer from crashing RT due to concurrently scanning an image file.
    std::unique_ptr<wchar_t, GFreeFunc> wfname (reinterpret_cast<wchar_t*>(g_utf8_to_utf16 (fname.c_str (), -1, NULL, NULL, NULL)), g_free);

    HANDLE hFile = CreateFileW ( wfname.get (), GENERIC_READ | GENERIC_WRITE, 0 /* no sharing allowed */, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    FILE* f = nullptr;

    if (hFile != INVALID_HANDLE_VALUE) {
        f = _fdopen (_open_osfhandle ((intptr_t)hFile, 0), "wb");
    }

#else

    FILE* f = ::g_fopen (fname.c_str (), "wb");

#endif

    return f;
}

template <typename Iterator, typename Integer = std::size_t>
auto to_long(const Iterator &iter, Integer n = Integer{0}) -> decltype(
#if EXIV2_TEST_VERSION(0,28,0)
    iter->toInt64()
) {
    return iter->toInt64(n);
#else
    iter->toLong()
) {
    return iter->toLong(n);
#endif
}

}

void ImageIO::setMetadata(Exiv2Metadata info)
{
    metadataInfo = std::move(info);
}

void ImageIO::setOutputProfile(const std::string& pdata)
{
    profileData = pdata;
}

ImageIO::ImageIO() :
    pl(nullptr),
    embProfile(nullptr),
    profileLength(0),
    loadedProfileData(nullptr),
    loadedProfileLength(0),
    sampleFormat(IIOSF_UNKNOWN),
    sampleArrangement(IIOSA_UNKNOWN)
{
}

ImageIO::~ImageIO ()
{

    if (embProfile) {
        cmsCloseProfile(embProfile);
    }

    deleteLoadedProfileData();
}

void png_read_data(png_struct_def  *png_ptr, unsigned char *data, size_t length);
void png_write_data(png_struct_def *png_ptr, unsigned char *data, size_t length);
void png_flush(png_struct_def *png_ptr);

int ImageIO::getPNGSampleFormat (const Glib::ustring &fname, IIOSampleFormat &sFormat, IIOSampleArrangement &sArrangement)
{
    FILE *file = g_fopen (fname.c_str (), "rb");

    if (!file) {
        return IMIO_CANNOTREADFILE;
    }

    //reading PNG header
    unsigned char header[8];

    if (fread (header, 1, 8, file) != 8 || png_sig_cmp (header, 0, 8)) {
        fclose(file);
        return IMIO_HEADERERROR;
    }

    //initializing main structures
    png_structp png = png_create_read_struct (PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);

    if (!png) {
        fclose (file);
        return IMIO_HEADERERROR;
    }

    png_infop info = png_create_info_struct (png);
    png_infop end_info = png_create_info_struct (png);

    if (!end_info || !info) {
        png_destroy_read_struct (&png, &info, &end_info);
        fclose (file);
        return IMIO_HEADERERROR;
    }

    if (setjmp (png_jmpbuf(png))) {
        png_destroy_read_struct (&png, &info, &end_info);
        fclose (file);
        return IMIO_READERROR;
    }

    //set up png read
    png_set_read_fn (png, file, png_read_data);
    png_set_sig_bytes (png, 8);

    png_read_info(png, info);

    //retrieving image information
    png_uint_32 width, height;
    int bit_depth, color_type, interlace_type, compression_type, filter_method;
    png_get_IHDR(png, info, &width, &height, &bit_depth, &color_type, &interlace_type, &compression_type, &filter_method);

    png_destroy_read_struct (&png, &info, &end_info);
    fclose (file);

    if (interlace_type != PNG_INTERLACE_NONE) {
        return IMIO_VARIANTNOTSUPPORTED;
    }

    if (bit_depth == 8) {
        sArrangement = IIOSA_CHUNKY;
        sFormat = IIOSF_UNSIGNED_CHAR;
        return IMIO_SUCCESS;
    } else if (bit_depth == 16) {
        sArrangement = IIOSA_CHUNKY;
        sFormat = IIOSF_UNSIGNED_SHORT;
        return IMIO_SUCCESS;
    } else {
        sArrangement = IIOSA_UNKNOWN;
        sFormat = IIOSF_UNKNOWN;
        return IMIO_VARIANTNOTSUPPORTED;
    }
}

int ImageIO::loadPNG  (const Glib::ustring &fname)
{

    FILE *file = g_fopen (fname.c_str (), "rb");

    if (!file) {
        return IMIO_CANNOTREADFILE;
    }

    if (pl) {
        pl->setProgressStr ("PROGRESSBAR_LOADPNG");
        pl->setProgress (0.0);
    }

    //reading PNG header
    unsigned char header[8];

    if (fread (header, 1, 8, file) != 8 || png_sig_cmp (header, 0, 8)) {
        fclose(file);
        return IMIO_HEADERERROR;
    }

    //initializing main structures
    png_structp png = png_create_read_struct (PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);

    if (!png) {
        fclose (file);
        return IMIO_HEADERERROR;
    }

    // silence the warning about "invalid" sRGB profiles -- see #4260
#if defined(PNG_SKIP_sRGB_CHECK_PROFILE) && defined(PNG_SET_OPTION_SUPPORTED)
    png_set_option(png, PNG_SKIP_sRGB_CHECK_PROFILE, PNG_OPTION_ON);
#endif

    png_infop info = png_create_info_struct (png);
    png_infop end_info = png_create_info_struct (png);

    if (!end_info || !info) {
        png_destroy_read_struct (&png, &info, &end_info);
        fclose (file);
        return IMIO_HEADERERROR;
    }

    if (setjmp (png_jmpbuf(png))) {
        png_destroy_read_struct (&png, &info, &end_info);
        fclose (file);
        return IMIO_READERROR;
    }

    //set up png read
    png_set_read_fn (png, file, png_read_data);
    png_set_sig_bytes (png, 8);

    png_read_info(png, info);

    embProfile = nullptr;

    //retrieving image information
    png_uint_32 width, height;
    int bit_depth, color_type, interlace_type, compression_type, filter_method;
    png_get_IHDR(png, info, &width, &height, &bit_depth, &color_type, &interlace_type, &compression_type, &filter_method);

    if (color_type == PNG_COLOR_TYPE_PALETTE || interlace_type != PNG_INTERLACE_NONE )  {
        // we don't support interlaced png or png with palette
        png_destroy_read_struct (&png, &info, &end_info);
        fclose (file);
        printf("%s uses an unsupported feature: <palette-indexed colors|interlacing>. Skipping.\n", fname.data());
        return IMIO_VARIANTNOTSUPPORTED;
    }

    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(png);
    }

    if (png_get_valid(png, info, PNG_INFO_tRNS)) {
        png_set_tRNS_to_alpha(png);
        png_set_strip_alpha(png);
    }

    if (color_type & PNG_COLOR_MASK_ALPHA) {
        png_set_strip_alpha(png);
    }

    // reading the embedded ICC profile if any
    if (png_get_valid(png, info, PNG_INFO_iCCP)) {
        png_charp name;
        int compression_type;
#if PNG_LIBPNG_VER < 10500
        png_charp profdata;
#else
        png_bytep profdata;
#endif
        png_uint_32 proflen;
        png_get_iCCP(png, info, &name, &compression_type, &profdata, &proflen);
        embProfile = cmsOpenProfileFromMem(profdata, proflen);
        loadedProfileData = new char[proflen];
        memcpy(loadedProfileData, profdata, proflen);
    }

    //setting gamma
    double gamma;

    if (png_get_gAMA(png, info, &gamma)) {
        png_set_gamma(png, 1.0 / gamma, gamma);    // use gamma from metadata
    } else {
        png_set_gamma(png, 2.2, 1.0 / 2.2);    // no gamma in metadata, suppose gamma 2.2
    }

//  if (bps==8 && bit_depth==16) png_set_strip_16(png);

    //updating png info struct
    png_read_update_info(png, info);
    png_get_IHDR(png, info, &width, &height, &bit_depth, &color_type, &interlace_type, &compression_type, &filter_method);

    allocate (width, height);

    int rowlen = width * 3 * bit_depth / 8;
    unsigned char *row = new unsigned char [rowlen];

    // set a new jump point to avoid memory leak
    if (setjmp (png_jmpbuf(png))) {
        png_destroy_read_struct (&png, &info, &end_info);
        fclose (file);
        delete [] row;
        return IMIO_READERROR;
    }

    for (unsigned int i = 0; i < height; i++) {

        png_read_row (png, (png_byte*)row, nullptr);

        if (bit_depth == 16) { // convert scanline to host byte order
            unsigned short* srow = (unsigned short*)row;

            for (unsigned int j = 0; j < width * 3; j++) {
                srow[j] = ntohs (srow[j]);
            }
        }

        setScanline (i, row, bit_depth);

        if (pl && !(i % 100)) {
            pl->setProgress ((double)(i + 1) / height);
        }
    }

    png_read_end (png, nullptr);
    png_destroy_read_struct (&png, &info, &end_info);

    delete [] row;
    fclose(file);

    if (pl) {
        pl->setProgressStr ("PROGRESSBAR_READY");
        pl->setProgress (1.0);
    }

    return IMIO_SUCCESS;
}

typedef struct  {
    struct jpeg_error_mgr pub;  /* "public" fields */
    jmp_buf setjmp_buffer;  /* for return to caller */
} my_error_mgr;

void my_error_exit (j_common_ptr cinfo)
{
    /* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
    my_error_mgr *myerr = (my_error_mgr*) cinfo->err;
    /* Always display the message. */
    /* We could postpone this until after returning, if we chose. */
    (*cinfo->err->output_message) (cinfo);

    /* Return control to the setjmp point */
#if defined( _WIN32 ) && defined( __x86_64__ ) && !defined(__clang__)
    __builtin_longjmp(myerr->setjmp_buffer, 1);
#else
    longjmp(myerr->setjmp_buffer, 1);
#endif
}


int ImageIO::loadJPEGFromMemory (const char* buffer, int bufsize)
{
    jpeg_decompress_struct cinfo;
    jpeg_create_decompress(&cinfo);
    jpeg_memory_src (&cinfo, (const JOCTET*)buffer, bufsize);

    /* We use our private extension JPEG error handler.
       Note that this struct must live as long as the main JPEG parameter
       struct, to avoid dangling-pointer problems.
    */
    my_error_mgr jerr;
    /* We set up the normal JPEG error routines, then override error_exit. */
    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = my_error_exit;

    /* Establish the setjmp return context for my_error_exit to use. */
#if defined( _WIN32 ) && defined( __x86_64__ ) && !defined(__clang__)

    if (__builtin_setjmp(jerr.setjmp_buffer)) {
#else

    if (setjmp(jerr.setjmp_buffer)) {
#endif
        /* If we get here, the JPEG code has signaled an error.
           We need to clean up the JPEG object and return.
        */
        jpeg_destroy_decompress(&cinfo);
        return IMIO_READERROR;
    }


    if (pl) {
        pl->setProgressStr ("PROGRESSBAR_LOADJPEG");
        pl->setProgress (0.0);

    }

    setup_read_icc_profile (&cinfo);

    jpeg_read_header(&cinfo, TRUE);

    deleteLoadedProfileData();
    bool hasprofile = read_icc_profile (&cinfo, (JOCTET**)&loadedProfileData, (unsigned int*)&loadedProfileLength);

    if (hasprofile) {
        embProfile = cmsOpenProfileFromMem (loadedProfileData, loadedProfileLength);
    } else {
        embProfile = nullptr;
    }

    jpeg_start_decompress(&cinfo);

    unsigned int width = cinfo.output_width;
    unsigned int height = cinfo.output_height;

    allocate (width, height);

    unsigned char *row = new unsigned char[width * 3];

    while (cinfo.output_scanline < height) {
        if (jpeg_read_scanlines(&cinfo, &row, 1) < 1) {
            jpeg_finish_decompress(&cinfo);
            jpeg_destroy_decompress(&cinfo);
            delete [] row;
            return IMIO_READERROR;
        }

        setScanline (cinfo.output_scanline - 1, row, 8, cinfo.num_components);

        if (pl && !(cinfo.output_scanline % 100)) {
            pl->setProgress ((double)(cinfo.output_scanline) / cinfo.output_height);
        }
    }

    delete [] row;

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    if (pl) {
        pl->setProgressStr ("PROGRESSBAR_READY");
        pl->setProgress (1.0);
    }

    return IMIO_SUCCESS;
}

int ImageIO::loadJPEG (const Glib::ustring &fname)
{
    std::unique_ptr<FILE, void (*)(FILE *)> file(
        g_fopen(fname.c_str(), "rb"),
        [](FILE *f) {
            fclose(f);
        });

    if (!file) {
        return IMIO_CANNOTREADFILE;
    }

    jpeg_decompress_struct cinfo;
    jpeg_error_mgr jerr;
    cinfo.err = my_jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);

    my_jpeg_stdio_src (&cinfo, file.get());

#if defined( _WIN32 ) && defined( __x86_64__ ) && !defined(__clang__)
    if ( __builtin_setjmp((reinterpret_cast<rt_jpeg_error_mgr*>(cinfo.src))->error_jmp_buf) == 0 ) {
#else
    if ( setjmp((reinterpret_cast<rt_jpeg_error_mgr*>(cinfo.src))->error_jmp_buf) == 0 ) {
#endif
        if (pl) {
            pl->setProgressStr ("PROGRESSBAR_LOADJPEG");
            pl->setProgress (0.0);
        }

        setup_read_icc_profile (&cinfo);

        //jpeg_stdio_src(&cinfo,file);
        jpeg_read_header(&cinfo, TRUE);

        //if JPEG is CMYK, then abort reading
        if (cinfo.jpeg_color_space == JCS_CMYK || cinfo.jpeg_color_space == JCS_YCCK) {
            jpeg_destroy_decompress(&cinfo);
            return IMIO_READERROR;
        }

        cinfo.out_color_space = JCS_RGB;

        deleteLoadedProfileData();
        bool hasprofile = read_icc_profile (&cinfo, (JOCTET**)&loadedProfileData, (unsigned int*)&loadedProfileLength);

        if (hasprofile) {
            embProfile = cmsOpenProfileFromMem (loadedProfileData, loadedProfileLength);
        } else {
            embProfile = nullptr;
        }

        jpeg_start_decompress(&cinfo);

        unsigned int width = cinfo.output_width;
        unsigned int height = cinfo.output_height;

        allocate (width, height);

        unsigned char *row = new unsigned char[width * 3];

        while (cinfo.output_scanline < height) {
            if (jpeg_read_scanlines(&cinfo, &row, 1) < 1) {
                jpeg_finish_decompress(&cinfo);
                jpeg_destroy_decompress(&cinfo);
                delete [] row;
                return IMIO_READERROR;
            }

            setScanline (cinfo.output_scanline - 1, row, 8);

            if (pl && !(cinfo.output_scanline % 100)) {
                pl->setProgress ((double)(cinfo.output_scanline) / cinfo.output_height);
            }
        }

        delete [] row;

        jpeg_finish_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);
        file.reset();

        if (pl) {
            pl->setProgressStr ("PROGRESSBAR_READY");
            pl->setProgress (1.0);
        }

        return IMIO_SUCCESS;
    } else {
        jpeg_destroy_decompress(&cinfo);
        return IMIO_READERROR;
    }
}

int ImageIO::getTIFFSampleFormat (const Glib::ustring &fname, IIOSampleFormat &sFormat, IIOSampleArrangement &sArrangement)
{
#ifdef _WIN32
    wchar_t *wfilename = (wchar_t*)g_utf8_to_utf16 (fname.c_str(), -1, NULL, NULL, NULL);
    TIFF* in = TIFFOpenW (wfilename, "r");
    g_free (wfilename);
#else
    TIFF* in = TIFFOpen(fname.c_str(), "r");
#endif

    if (in == nullptr) {
        return IMIO_CANNOTREADFILE;
    }

    std::uint16_t bitspersample = 0, samplesperpixel = 0, sampleformat = 0;
    int hasTag = TIFFGetField(in, TIFFTAG_BITSPERSAMPLE, &bitspersample);
    hasTag &= TIFFGetField(in, TIFFTAG_SAMPLESPERPIXEL, &samplesperpixel);

    if (!hasTag) {
        // These are needed
        TIFFClose(in);
        sFormat = IIOSF_UNKNOWN;
        return IMIO_VARIANTNOTSUPPORTED;
    }

    if (!TIFFGetField(in, TIFFTAG_SAMPLEFORMAT, &sampleformat)) {
        /*
         * WARNING: This is a dirty hack!
         * We assume that files which doesn't contain the TIFFTAG_SAMPLEFORMAT tag
         * (which is the case with uncompressed TIFFs produced by RT!) are RGB files,
         * but that may be not true.   --- Hombre
         */
        sampleformat = SAMPLEFORMAT_UINT;
    } else if (sampleformat == SAMPLEFORMAT_VOID) {
        // according to https://www.awaresystems.be/imaging/tiff/tifftags/sampleformat.html
        // we assume SAMPLEFORMAT_UINT if SAMPLEFORMAT_VOID is set
        sampleformat = SAMPLEFORMAT_UINT;
    }

    std::uint16_t config;
    TIFFGetField(in, TIFFTAG_PLANARCONFIG, &config);

    if (config == PLANARCONFIG_CONTIG) {
        sArrangement = IIOSA_CHUNKY;
    } else {
        sFormat = IIOSF_UNKNOWN;
        sArrangement = IIOSA_UNKNOWN;
        TIFFClose(in);
        return IMIO_VARIANTNOTSUPPORTED;
    }

    std::uint16_t photometric;

    if (!TIFFGetField(in, TIFFTAG_PHOTOMETRIC, &photometric)) {
        TIFFClose(in);
        return IMIO_VARIANTNOTSUPPORTED;
    }

    std::uint16_t compression;

    if (photometric == PHOTOMETRIC_LOGLUV)
        if (!TIFFGetField(in, TIFFTAG_COMPRESSION, &compression)) {
            compression = COMPRESSION_NONE;
        }

    TIFFClose(in);

    if (photometric == PHOTOMETRIC_RGB || photometric == PHOTOMETRIC_MINISBLACK) {
        if ((samplesperpixel == 1 || samplesperpixel == 3 || samplesperpixel == 4) && sampleformat == SAMPLEFORMAT_UINT) {
            if (bitspersample == 8) {
                sFormat = IIOSF_UNSIGNED_CHAR;
                return IMIO_SUCCESS;
            }

            if (bitspersample == 16) {
                sFormat = IIOSF_UNSIGNED_SHORT;
                return IMIO_SUCCESS;
            }
        } else if ((samplesperpixel == 3 || samplesperpixel == 4) && sampleformat == SAMPLEFORMAT_IEEEFP) {
            if (bitspersample==16) {
                sFormat = IIOSF_FLOAT16;
                return IMIO_SUCCESS;
            }
            if (bitspersample == 24) {
                sFormat = IIOSF_FLOAT24;
                return IMIO_SUCCESS;
            }
            if (bitspersample == 32) {
                sFormat = IIOSF_FLOAT32;
                return IMIO_SUCCESS;
            }
        }
    } else if ((samplesperpixel == 3 || samplesperpixel == 4) && photometric == PHOTOMETRIC_LOGLUV) {
        if (compression == COMPRESSION_SGILOG24) {
            sFormat = IIOSF_LOGLUV24;
            return IMIO_SUCCESS;
        } else if (compression == COMPRESSION_SGILOG) {
            sFormat = IIOSF_LOGLUV32;
            return IMIO_SUCCESS;
        }
    }

    return IMIO_VARIANTNOTSUPPORTED;
}

int ImageIO::loadTIFF (const Glib::ustring &fname)
{

    static MyMutex thumbMutex;
    MyMutex::MyLock lock(thumbMutex);

    if(!App::get().options().serializeTiffRead) {
        lock.release();
    }

#ifdef _WIN32
    wchar_t *wfilename = (wchar_t*)g_utf8_to_utf16 (fname.c_str(), -1, NULL, NULL, NULL);
    TIFF* in = TIFFOpenW (wfilename, "r");
    g_free (wfilename);
#else
    TIFF* in = TIFFOpen(fname.c_str(), "r");
#endif

    if (in == nullptr) {
        return IMIO_CANNOTREADFILE;
    }

    if (pl) {
        pl->setProgressStr ("PROGRESSBAR_LOADTIFF");
        pl->setProgress (0.0);
    }

    int width, height;
    TIFFGetField(in, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField(in, TIFFTAG_IMAGELENGTH, &height);

    std::uint16_t bitspersample, samplesperpixel;
    int hasTag = TIFFGetField(in, TIFFTAG_BITSPERSAMPLE, &bitspersample);
    hasTag &= TIFFGetField(in, TIFFTAG_SAMPLESPERPIXEL, &samplesperpixel);

    if (!hasTag) {
        // These are needed
        TIFFClose(in);
        fprintf(stderr, "Error 1 loading %s\n", fname.c_str());
        fflush(stderr);
        return IMIO_VARIANTNOTSUPPORTED;
    }

    std::uint16_t config;
    TIFFGetField(in, TIFFTAG_PLANARCONFIG, &config);

    if (config != PLANARCONFIG_CONTIG) {
        TIFFClose(in);
        fprintf(stderr, "Error 2 loading %s\n", fname.c_str());
        fflush(stderr);
        return IMIO_VARIANTNOTSUPPORTED;
    }

    if (sampleFormat & (IIOSF_LOGLUV24 | IIOSF_LOGLUV32)) {
        TIFFSetField(in, TIFFTAG_SGILOGDATAFMT, SGILOGDATAFMT_FLOAT);
    }

    /*
     * We could use the min/max values set in TIFFTAG_SMINSAMPLEVALUE and
     * TIFFTAG_SMAXSAMPLEVALUE, but for now, we normalize the image to the
     * effective minimum and maximum values
     */
    if (settings->verbose) {
        printf("Information of \"%s\":\n", fname.c_str());
        std::uint16_t tiffDefaultScale, tiffBaselineExposure, tiffLinearResponseLimit;
        if (TIFFGetField(in, TIFFTAG_DEFAULTSCALE, &tiffDefaultScale)) {
            printf("   DefaultScale: %d\n", tiffDefaultScale);
        }
        else
            printf("   No DefaultScale value!\n");
        if (TIFFGetField(in, TIFFTAG_BASELINEEXPOSURE, &tiffBaselineExposure)) {
            printf("   BaselineExposure: %d\n", tiffBaselineExposure);
        }
        else
            printf("   No BaselineExposure value!\n");
        if (TIFFGetField(in, TIFFTAG_LINEARRESPONSELIMIT, &tiffLinearResponseLimit)) {
            printf("   LinearResponseLimit: %d\n", tiffLinearResponseLimit);
        }
        else
            printf("   No LinearResponseLimit value!\n");

        std::uint16_t tiffMinValue, tiffMaxValue;
        if (TIFFGetField(in, TIFFTAG_SMINSAMPLEVALUE, &tiffMinValue)) {
            printf("   MinValue: %d\n", tiffMinValue);
        }
        else
            printf("   No minimum value!\n");
        if (TIFFGetField(in, TIFFTAG_SMAXSAMPLEVALUE, &tiffMaxValue)) {
            printf("   MaxValue: %d\n\n", tiffMaxValue);
        }
        else
            printf("   No maximum value!\n\n");
        printf("   Those values are not taken into account, the image data are normalized to a [0;1] range\n\n");
    }

    char* profdata;
    deleteLoadedProfileData();

    if (TIFFGetField(in, TIFFTAG_ICCPROFILE, &loadedProfileLength, &profdata)) {
        embProfile = cmsOpenProfileFromMem (profdata, loadedProfileLength);
        loadedProfileData = new char [loadedProfileLength];
        memcpy (loadedProfileData, profdata, loadedProfileLength);
    } else {
        embProfile = nullptr;
    }

    allocate (width, height);

    std::unique_ptr<unsigned char[]> linebuffer(new unsigned char[TIFFScanlineSize(in) * (samplesperpixel == 1 ? 3 : 1)]);

    for (int row = 0; row < height; row++) {
        if (TIFFReadScanline(in, linebuffer.get(), row, 0) < 0) {
            TIFFClose(in);
            fprintf(stderr, "Error 3 loading %s\n", fname.c_str());
            fflush(stderr);
            return IMIO_READERROR;
        }

        if (samplesperpixel > 3) {
            for (int i = 0; i < width; i++) {
                memmove(linebuffer.get() + i * 3 * bitspersample / 8, linebuffer.get() + i * samplesperpixel * bitspersample / 8, 3 * bitspersample / 8);
            }
        }
        else if (samplesperpixel == 1) {
            const size_t bytes = bitspersample / 8;
            for (int i = width - 1; i >= 0; --i) {
                const unsigned char* const src = linebuffer.get() + i * bytes;
                unsigned char* const dest = linebuffer.get() + i * 3 * bytes;
                memcpy(dest + 2 * bytes, src, bytes);
                memcpy(dest + 1 * bytes, src, bytes);
                memcpy(dest + 0 * bytes, src, bytes);
            }
        }

        setScanline (row, linebuffer.get(), bitspersample);

        if (pl && !(row % 100)) {
            pl->setProgress ((double)(row + 1) / height);
        }
    }

    TIFFClose(in);

    if (pl) {
        pl->setProgressStr ("PROGRESSBAR_READY");
        pl->setProgress (1.0);
    }

    return IMIO_SUCCESS;
}

#ifdef LIBJXL
#define _PROFILE_ JXL_COLOR_PROFILE_TARGET_ORIGINAL
// adapted from libjxl
int ImageIO::loadJXL(const Glib::ustring &fname)
{
    if (pl) {
        pl->setProgressStr("PROGRESSBAR_LOADJXL");
        pl->setProgress(0.0);
    }

    std::vector<std::uint8_t> icc_profile;
    std::vector<std::uint8_t> buffer;
    std::size_t buffer_size = 0;

    JxlBasicInfo info = {};
    JxlPixelFormat format = {};

    format.num_channels = 3;
    format.data_type = JXL_TYPE_FLOAT;
    format.endianness = JXL_NATIVE_ENDIAN;
    format.align = 0;

    std::vector<std::uint8_t> const compressed = getFileData(fname);

    if (compressed.empty()) {
        std::cerr << "Error: loadJXL failed to get data from file" << std::endl;
        return IMIO_READERROR;
    }

    // multi-threaded parallel runner.
    auto runner = JxlResizableParallelRunnerMake(nullptr);

    auto dec = JxlDecoderMake(nullptr);

    if (JXL_DEC_SUCCESS !=
            JxlDecoderSubscribeEvents(dec.get(), JXL_DEC_BASIC_INFO |
                                      JXL_DEC_COLOR_ENCODING |
                                      JXL_DEC_FULL_IMAGE)) {
        std::cerr << "Error: JxlDecoderSubscribeEvents failed" << std::endl;
        return IMIO_HEADERERROR;
    }

    if (JXL_DEC_SUCCESS !=
            JxlDecoderSetParallelRunner(dec.get(), JxlResizableParallelRunner,
                                        runner.get())) {
        std::cerr << "Error: JxlDecoderSetParallelRunner failed" << std::endl;
        return IMIO_HEADERERROR;
    }

    // grand decode loop...
    JxlDecoderSetInput(dec.get(), compressed.data(), compressed.size());

    while (true) {
        JxlDecoderStatus status = JxlDecoderProcessInput(dec.get());

        if (status == JXL_DEC_BASIC_INFO) {
            if (JXL_DEC_SUCCESS != JxlDecoderGetBasicInfo(dec.get(), &info)) {
                std::cerr << "Error: JxlDecoderGetBasicInfo failed" << std::endl;
                return IMIO_HEADERERROR;
            }

            JxlResizableParallelRunnerSetThreads(
                runner.get(),
                JxlResizableParallelRunnerSuggestThreads(info.xsize, info.ysize));
        } else if (status == JXL_DEC_COLOR_ENCODING) {
            // check for ICC profile
            deleteLoadedProfileData();
            embProfile = nullptr;
            std::size_t icc_size = 0;

            if (JXL_DEC_SUCCESS !=
#if JPEGXL_NUMERIC_VERSION < JPEGXL_COMPUTE_NUMERIC_VERSION(0, 9, 0)
                    JxlDecoderGetICCProfileSize(dec.get(), &format, _PROFILE_, &icc_size)
#else
                    JxlDecoderGetICCProfileSize(dec.get(), _PROFILE_, &icc_size)
#endif
               ) {
                std::cerr << "Warning: JxlDecoderGetICCProfileSize failed" << std::endl;
            }

            if (icc_size > 0) {
                icc_profile.resize(icc_size);

                if (JXL_DEC_SUCCESS !=
#if JPEGXL_NUMERIC_VERSION < JPEGXL_COMPUTE_NUMERIC_VERSION(0, 9, 0)
                        JxlDecoderGetColorAsICCProfile(
                            dec.get(), &format, _PROFILE_,
                            icc_profile.data(), icc_profile.size())
#else
                        JxlDecoderGetColorAsICCProfile(
                            dec.get(), _PROFILE_,
                            icc_profile.data(), icc_profile.size())
#endif
                   ) {
                    std::cerr << "Warning: JxlDecoderGetColorAsICCProfile failed" << std::endl;
                } else {
                    embProfile = cmsOpenProfileFromMem(icc_profile.data(),
                                                       icc_profile.size());
                }
            } else {
                std::cerr << "Warning: Empty ICC data." << std::endl;
            }
        } else if (status == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
            // Note: If assert is triggered, change to assignment.
            // We want maximum bit depth from the decoder,
            // regardless of the original encoding intent.
            assert(format.data_type == JXL_TYPE_FLOAT);

            if (JXL_DEC_SUCCESS !=
                    JxlDecoderImageOutBufferSize(dec.get(), &format, &buffer_size)) {
                std::cerr << "Error: JxlDecoderImageOutBufferSize failed" << std::endl;
                return IMIO_READERROR;
            }

            buffer.resize(buffer_size);

            if (JXL_DEC_SUCCESS != JxlDecoderSetImageOutBuffer(dec.get(), &format, buffer.data(), buffer.size())) {
                std::cerr << "Error: JxlDecoderSetImageOutBuffer failed" << std::endl;
                return IMIO_READERROR;
            }
        } else if (status == JXL_DEC_FULL_IMAGE ||
                   status == JXL_DEC_FRAME) {
            // Nothing to do. If the image is an animation, more full frames
            // may be decoded. This example only keeps the first one.
            break;
        } else if (status == JXL_DEC_SUCCESS) {
            // Decoding complete.  Decoder will be released automatically.
            break;
        } else if (status == JXL_DEC_NEED_MORE_INPUT) {
            std::cerr << "Error: Decoder needs more input data" << std::endl;
            return IMIO_READERROR;
        } else if (status == JXL_DEC_ERROR) {
            std::cerr << "Error: Decoder error" << std::endl;
            return IMIO_READERROR;
        } else {
            std::cerr << "Error: Unknown decoder status" << std::endl;
            return IMIO_READERROR;
        }
    } // end grand decode loop

    std::size_t width = info.xsize;
    std::size_t height = info.ysize;

    allocate(width, height);

    std::size_t line_length = width * 3 * 4;

    for (std::size_t row = 0; row < height; ++row) {
        setScanline(row, buffer.data() + (row * line_length), 32);

        if (pl && !(row % 100)) {
            pl->setProgress((double)(row + 1) / height);
        }
    }

    if (pl) {
        pl->setProgressStr("PROGRESSBAR_READY");
        pl->setProgress(1.0);
    }

    return IMIO_SUCCESS;
}
#undef _PROFILE_
#endif // LIBJXL

int ImageIO::loadPPMFromMemory(const char* buffer, int width, int height, bool swap, int bps)
{
    allocate (width, height);

    int line_length(width * 3 * (bps / 8));

    if ( swap && bps > 8 ) {
        char swapped[line_length];

        for ( int row = 0; row < height; ++row ) {
            ::rtengine::swab(((const char*)buffer) + (row * line_length), swapped, line_length);
            setScanline(row, (unsigned char*)&swapped[0], bps);
        }
    } else {
        for ( int row = 0; row < height; ++row ) {
            setScanline(row, ((const unsigned char*)buffer) + (row * line_length), bps);
        }
    }

    return IMIO_SUCCESS;
}


int ImageIO::savePNG  (const Glib::ustring &fname, volatile int bps) const
{
    if (getWidth() < 1 || getHeight() < 1) {
        return IMIO_HEADERERROR;
    }

    FILE* const file = g_fopen_withBinaryAndLock (fname);

    if (!file) {
        return IMIO_CANNOTWRITEFILE;
    }

    if (pl) {
        pl->setProgressStr ("PROGRESSBAR_SAVEPNG");
        pl->setProgress (0.0);
    }

    png_structp png = png_create_write_struct (PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);

    if (!png) {
        fclose (file);
        return IMIO_HEADERERROR;
    }

    // silence the warning about "invalid" sRGB profiles -- see #4260
#if defined(PNG_SKIP_sRGB_CHECK_PROFILE) && defined(PNG_SET_OPTION_SUPPORTED)
    png_set_option(png, PNG_SKIP_sRGB_CHECK_PROFILE, PNG_OPTION_ON);
#endif

    png_infop info = png_create_info_struct(png);

    if (!info) {
        png_destroy_write_struct (&png, nullptr);
        fclose (file);
        return IMIO_HEADERERROR;
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct (&png, &info);
        fclose(file);
        return IMIO_CANNOTWRITEFILE;
    }

    png_set_write_fn (png, file, png_write_data, png_flush);

    png_set_filter(png, 0, PNG_FILTER_PAETH);
    png_set_compression_level(png, 6);
    png_set_compression_strategy(png, 3);

    int width = getWidth ();
    int height = getHeight ();

    if (bps < 0) {
        bps = getBPS ();
    }
    if (bps > 16) {
        bps = 16;
    }

    png_set_IHDR(png, info, width, height, bps, PNG_COLOR_TYPE_RGB,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_BASE);

    if (!profileData.empty()) {
#if PNG_LIBPNG_VER < 10500
        png_const_charp profdata = reinterpret_cast<png_const_charp>(profileData.data());
#else
        png_const_bytep profdata = reinterpret_cast<png_const_bytep>(profileData.data());
#endif
        png_set_iCCP(png, info, "icc", 0, profdata, profileData.size());
    }

    int rowlen = width * 3 * bps / 8;
    unsigned char *row = new unsigned char [rowlen];

    png_write_info(png, info);

    for (int i = 0; i < height; i++) {
        getScanline (i, row, bps);

        if (bps == 16) {
            // convert to network byte order
#if __BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__
            for (int j = 0; j < width * 6; j += 2) {
                unsigned char tmp = row[j];
                row[j] = row[j + 1];
                row[j + 1] = tmp;
            }

#endif
        }

        png_write_row (png, (png_byte*)row);

        if (pl && !(i % 100)) {
            pl->setProgress ((double)(i + 1) / height);
        }
    }

    png_write_end(png, info);
    png_destroy_write_struct(&png, &info);

    delete [] row;
    fclose (file);

    if (!saveMetadata(fname)) {
        g_remove(fname.c_str());
        return IMIO_CANNOTWRITEFILE;
    }

    if (pl) {
        pl->setProgressStr ("PROGRESSBAR_READY");
        pl->setProgress (1.0);
    }

    return IMIO_SUCCESS;
}



// Quality 0..100, subsampling: 1=low quality, 2=medium, 3=high
int ImageIO::saveJPEG (const Glib::ustring &fname, int quality, int subSamp) const
{
    if (getWidth() < 1 || getHeight() < 1) {
        return IMIO_HEADERERROR;
    }

    FILE* const file = g_fopen_withBinaryAndLock (fname);

    if (!file) {
        return IMIO_CANNOTWRITEFILE;
    }

    jpeg_compress_struct cinfo;
    /* We use our private extension JPEG error handler.
       Note that this struct must live as long as the main JPEG parameter
       struct, to avoid dangling-pointer problems.
    */
    my_error_mgr jerr;
    /* We set up the normal JPEG error routines, then override error_exit. */
    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = my_error_exit;

    /* Establish the setjmp return context for my_error_exit to use. */
#if defined( _WIN32 ) && defined( __x86_64__ ) && !defined(__clang__)

    if (__builtin_setjmp(jerr.setjmp_buffer)) {
#else

    if (setjmp(jerr.setjmp_buffer)) {
#endif
        /* If we get here, the JPEG code has signaled an error.
           We need to clean up the JPEG object, close the file, remove the already saved part of the file and return.
        */
        jpeg_destroy_compress(&cinfo);
        fclose(file);
        g_remove (fname.c_str());
        return IMIO_CANNOTWRITEFILE;
    }

    jpeg_create_compress (&cinfo);



    if (pl) {
        pl->setProgressStr ("PROGRESSBAR_SAVEJPEG");
        pl->setProgress (0.0);
    }

    jpeg_stdio_dest (&cinfo, file);

    int width = getWidth ();
    int height = getHeight ();

    cinfo.image_width  = width;
    cinfo.image_height = height;
    cinfo.in_color_space = JCS_RGB;
    cinfo.input_components = 3;
    jpeg_set_defaults (&cinfo);
    cinfo.write_JFIF_header = FALSE;

    // compute optimal Huffman coding tables for the image. Bit slower to generate, but size of result image is a bit less (default was FALSE)
    cinfo.optimize_coding = TRUE;

    // Since math coprocessors are common these days, FLOAT should be a bit more accurate AND fast (default is ISLOW)
    // (machine dependency is not really an issue, since we all run on x86 and having exactly the same file is not a requirement)
    cinfo.dct_method = JDCT_FLOAT;

    if (quality >= 0 && quality <= 100) {
        jpeg_set_quality (&cinfo, quality, true);
    }

    cinfo.comp_info[1].h_samp_factor = cinfo.comp_info[1].v_samp_factor = 1;
    cinfo.comp_info[2].h_samp_factor = cinfo.comp_info[2].v_samp_factor = 1;

    if (subSamp == 1) {
        // Best compression, default of the JPEG library:  2x2, 1x1, 1x1 (4:2:0)
        cinfo.comp_info[0].h_samp_factor = cinfo.comp_info[0].v_samp_factor = 2;
    } else if (subSamp == 2) {
        // Widely used normal ratio 2x1, 1x1, 1x1 (4:2:2)
        cinfo.comp_info[0].h_samp_factor = 2;
        cinfo.comp_info[0].v_samp_factor = 1;
    } else if (subSamp == 3) {
        // Best quality 1x1 1x1 1x1 (4:4:4)
        cinfo.comp_info[0].h_samp_factor = cinfo.comp_info[0].v_samp_factor = 1;
    }

    jpeg_start_compress(&cinfo, TRUE);

    // write icc profile to the output
    if (!profileData.empty()) {
        write_icc_profile (&cinfo, reinterpret_cast<const JOCTET*>(profileData.data()), profileData.size());
    }

    // write image data
    int rowlen = width * 3;
    unsigned char *row = new unsigned char [rowlen];

    /* To avoid memory leaks we establish a new setjmp return context for my_error_exit to use. */
#if defined( _WIN32 ) && defined( __x86_64__ ) && !defined(__clang__)

    if (__builtin_setjmp(jerr.setjmp_buffer)) {
#else

    if (setjmp(jerr.setjmp_buffer)) {
#endif
        /* If we get here, the JPEG code has signaled an error.
           We need to clean up the JPEG object, close the file, remove the already saved part of the file and return.
        */
        delete [] row;
        jpeg_destroy_compress(&cinfo);
        fclose(file);
        g_remove (fname.c_str());
        return IMIO_CANNOTWRITEFILE;
    }

    while (cinfo.next_scanline < cinfo.image_height) {

        getScanline (cinfo.next_scanline, row, 8);

        if (jpeg_write_scanlines (&cinfo, &row, 1) < 1) {
            jpeg_destroy_compress (&cinfo);
            delete [] row;
            fclose (file);
            g_remove (fname.c_str());
            return IMIO_CANNOTWRITEFILE;
        }

        if (pl && !(cinfo.next_scanline % 100)) {
            pl->setProgress ((double)(cinfo.next_scanline) / cinfo.image_height);
        }
    }

    jpeg_finish_compress (&cinfo);
    jpeg_destroy_compress (&cinfo);

    delete [] row;

    fclose (file);

    if (!saveMetadata(fname)) {
        g_remove(fname.c_str());
        return IMIO_CANNOTWRITEFILE;
    }

    if (pl) {
        pl->setProgressStr ("PROGRESSBAR_READY");
        pl->setProgress (1.0);
    }

    return IMIO_SUCCESS;
}


int ImageIO::saveTIFF (
    const Glib::ustring &fname,
    int bps,
    bool isFloat,
    bool uncompressed,
    bool big
) const
{
    if (getWidth() < 1 || getHeight() < 1) {
        return IMIO_HEADERERROR;
    }

    bool writeOk = true;
    int width = getWidth ();
    int height = getHeight ();

    if (bps < 0) {
        bps = getBPS ();
    }

    int lineWidth = width * 3 * (bps / 8);
    std::vector<unsigned char> linebuffer(lineWidth);

    std::string mode = "w";

    if (big) {
        mode += '8';
    }

#ifdef _WIN32
    FILE *file = g_fopen_withBinaryAndLock (fname);
    int fileno = _fileno(file);
    int osfileno = _get_osfhandle(fileno);
    TIFF* out = TIFFFdOpen (osfileno, fname.c_str(), mode.c_str());
#else
    TIFF* out = TIFFOpen(fname.c_str(), mode.c_str());
    // int fileno = TIFFFileno (out);
#endif

    if (!out) {
        return IMIO_CANNOTWRITEFILE;
    }

    if (pl) {
        pl->setProgressStr ("PROGRESSBAR_SAVETIFF");
        pl->setProgress (0.0);
    }

    bool needsReverse = false;

    TIFFSetField (out, TIFFTAG_SOFTWARE, "RawTherapee " RTVERSION);
    TIFFSetField (out, TIFFTAG_IMAGEWIDTH, width);
    TIFFSetField (out, TIFFTAG_IMAGELENGTH, height);
    TIFFSetField (out, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField (out, TIFFTAG_SAMPLESPERPIXEL, 3);
    TIFFSetField (out, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(out, 0));
    TIFFSetField (out, TIFFTAG_BITSPERSAMPLE, bps);
    TIFFSetField (out, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField (out, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
    TIFFSetField (out, TIFFTAG_COMPRESSION, uncompressed ? COMPRESSION_NONE : COMPRESSION_ADOBE_DEFLATE);
    TIFFSetField (out, TIFFTAG_SAMPLEFORMAT, (bps == 16 || bps == 32) && isFloat ? SAMPLEFORMAT_IEEEFP : SAMPLEFORMAT_UINT);

    // somehow Exiv2 (tested with 0.27.3) doesn't seem to be able to update
    // XResolution and YResolution, so we do it ourselves here....
    constexpr float default_resolution = 300.f;
    float x_res = default_resolution;
    float y_res = default_resolution;
    int res_unit = RESUNIT_INCH;
    if (!metadataInfo.filename().empty()) {
        auto exif = metadataInfo.getOutputExifData();
        auto it = exif.findKey(Exiv2::ExifKey("Exif.Image.XResolution"));
        if (it != exif.end()) {
            x_res = it->toFloat();
        }
        it = exif.findKey(Exiv2::ExifKey("Exif.Image.YResolution"));
        if (it != exif.end()) {
            y_res = it->toFloat();
        }
        it = exif.findKey(Exiv2::ExifKey("Exif.Image.ResolutionUnit"));
        if (it != exif.end()) {
            res_unit = to_long(it);
        }
    }
    TIFFSetField(out, TIFFTAG_XRESOLUTION, x_res);
    TIFFSetField(out, TIFFTAG_YRESOLUTION, y_res);
    TIFFSetField(out, TIFFTAG_RESOLUTIONUNIT, res_unit);

    if (!uncompressed) {
        TIFFSetField (out, TIFFTAG_PREDICTOR, (bps == 16 || bps == 32) && isFloat ? PREDICTOR_FLOATINGPOINT : PREDICTOR_HORIZONTAL);
    }
    if (!profileData.empty()) {
        TIFFSetField (out, TIFFTAG_ICCPROFILE, profileData.size(), profileData.data());
    }

    for (int row = 0; row < height; row++) {
        getScanline (row, linebuffer.data(), bps, isFloat);

        if (bps == 16) {
            if(needsReverse && !uncompressed && isFloat) {
                for(int i = 0; i < lineWidth; i += 2) {
                    std::swap(linebuffer[i], linebuffer[i + 1]);
                }
            }
        } else if (bps == 32) {
            if(needsReverse && !uncompressed) {
                for(int i = 0; i < lineWidth; i += 4) {
                    std::swap(linebuffer[i], linebuffer[i + 3]);
                    std::swap(linebuffer[i + 1], linebuffer[i + 2]);
                }
            }
        }

        if (TIFFWriteScanline (out, linebuffer.data(), row, 0) < 0) {
            TIFFClose (out);
            return IMIO_CANNOTWRITEFILE;
        }

        if (pl && !(row % 100)) {
            pl->setProgress ((double)(row + 1) / height);
        }
    }

    if (TIFFFlush(out) != 1) {
        writeOk = false;
    }

    TIFFClose (out);
#ifdef _WIN32
    fclose (file);
#endif

    if (!saveMetadata(fname)) {
        writeOk = false;
    }

    if (pl) {
        pl->setProgressStr ("PROGRESSBAR_READY");
        pl->setProgress (1.0);
    }

    if(writeOk) {
        return IMIO_SUCCESS;
    } else {
        g_remove (fname.c_str());
        return IMIO_CANNOTWRITEFILE;
    }
}

// PNG read and write routines:

void png_read_data(png_structp png_ptr, png_bytep data, png_size_t length)
{
    png_size_t check;

    /* fread() returns 0 on error, so it is OK to store this in a png_size_t
     * instead of an int, which is what fread() actually returns.
     */
    check = (png_size_t)fread(data, (png_size_t)1, length, (FILE *)png_get_io_ptr(png_ptr));

    if (check != length) {
        png_error(png_ptr, "Read Error");
    }
}

void png_write_data(png_structp png_ptr, png_bytep data, png_size_t length)
{
    png_uint_32 check;

    check = fwrite(data, 1, length, (FILE *)png_get_io_ptr(png_ptr));

    if (check != length) {
        png_error(png_ptr, "Write Error");
    }
}

void png_flush(png_structp png_ptr)
{
    FILE *io_ptr;
    io_ptr = (FILE *)(png_get_io_ptr(png_ptr));

    if (io_ptr != nullptr) {
        fflush(io_ptr);
    }
}

int ImageIO::load (const Glib::ustring &fname)
{

    if (hasPngExtension(fname)) {
        return loadPNG (fname);
    } else if (hasJpegExtension(fname)) {
        return loadJPEG (fname);
#ifdef LIBJXL
    } else if (hasJxlExtension(fname)) {
        return loadJXL(fname);
#endif
    } else if (hasTiffExtension(fname)) {
        return loadTIFF (fname);
    } else {
        return IMIO_FILETYPENOTSUPPORTED;
    }
}

int ImageIO::save (const Glib::ustring &fname) const
{
    if (hasPngExtension(fname)) {
        return savePNG (fname);
    } else if (hasJpegExtension(fname)) {
        return saveJPEG (fname);
    } else if (hasTiffExtension(fname)) {
        return saveTIFF (fname);
    } else {
        return IMIO_FILETYPENOTSUPPORTED;
    }
}

void ImageIO::setProgressListener (ProgressListener* l)
{
    pl = l;
}

void ImageIO::setSampleFormat(IIOSampleFormat sFormat)
{
    sampleFormat = sFormat;
}

IIOSampleFormat ImageIO::getSampleFormat() const
{
    return sampleFormat;
}

void ImageIO::setSampleArrangement(IIOSampleArrangement sArrangement)
{
    sampleArrangement = sArrangement;
}

IIOSampleArrangement ImageIO::getSampleArrangement() const
{
    return sampleArrangement;
}

cmsHPROFILE ImageIO::getEmbeddedProfile () const
{
    return embProfile;
}

void ImageIO::getEmbeddedProfileData (int& length, unsigned char*& pdata) const
{
    length = loadedProfileLength;
    pdata = (unsigned char*)loadedProfileData;
}

MyMutex& ImageIO::mutex ()
{
    return imutex;
}

void ImageIO::deleteLoadedProfileData( )
{
    if(loadedProfileData) {
        delete[] loadedProfileData;
    }

    loadedProfileData = nullptr;
}

bool ImageIO::saveMetadata(const Glib::ustring &fname) const
{
    if (metadataInfo.filename().empty()) {
        return true;
    }

    bool has_meta = true;
    try {
        metadataInfo.load();
    } catch (const std::exception& exc) {
        if (settings->verbose) {
            std::cout << "EXIF LOAD ERROR: " << exc.what() << std::endl;
        }
        has_meta = false;
    }

    if (has_meta) {
        try {
            metadataInfo.saveToImage(fname, false);
            // auto src = open_exiv2(metadataInfo.filename());
            // auto dst = open_exiv2(fname);
            // src->readMetadata();
            // dst->setMetadata(*src);
            // dst->exifData()["Exif.Image.Software"] = "RawTherapee " RTVERSION;
            // for (const auto& p : metadataInfo.exif()) {
            //     try {
            //         dst->exifData()[p.first] = p.second;
            //     } catch (const Exiv2::AnyError& exc) {
            //     }
            // }
            // for (const auto& p : metadataInfo.iptc()) {
            //     try {
            //         auto& v = p.second;
            //         if (!v.empty()) {
            //             dst->iptcData()[p.first] = v[0];
            //             for (size_t j = 1; j < v.size(); ++j) {
            //                 Exiv2::Iptcdatum d(Exiv2::IptcKey(p.first));
            //                 d.setValue(v[j]);
            //                 dst->iptcData().add(d);
            //             }
            //         }
            //     } catch (const Exiv2::AnyError& exc) {
            //     }
            // }
            // dst->writeMetadata();
        } catch (const std::exception& exc) {
            std::cout << "EXIF ERROR: " << exc.what() << std::endl;
            //return false;
        }
    }

    return true;
}
