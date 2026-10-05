/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2004-2010 Gabor Horvath <hgabor@rawtherapee.com>
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

#ifdef __GNUC__
#if defined(__FAST_MATH__)
#error Using the -ffast-math CFLAG is known to lead to problems. Disable it to compile RawTherapee.
#endif
#endif

#include "config.h"
#include <gtkmm.h>
#include <giomm.h>
#include <iostream>
#include <tiffio.h>
#include <cstring>
#include <cstdlib>
#include <locale.h>
#include "rtengine/newdelete.h"
#include "rtengine/procparams.h"
#include "rtengine/profilestore.h"
#include "rtengine/rtengine.h"
#include "options.h"
#include "soundman.h"
#include "rtimage.h"
#include "version.h"
#include "extprog.h"
#include "pathutils.h"

#ifndef _WIN32
#include <glibmm/fileutils.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <glibmm/threads.h>
#else
#include "rtengine/leanwindows.h"
#include <conio.h>
#include <shlobj.h>

#include <glibmm/thread.h>
#endif // _WIN32

// Set this to 1 to make RT work when started with Eclipse and arguments, at least on Windows platform
#define ECLIPSE_ARGS 0

namespace
{

bool fast_export = false;

}

/* Process line command options
 * Returns
 *  0 if process in batch has executed
 *  1 to start GUI (with a dir or file option)
 *  2 to start GUI because no files found
 *  -1 if there is an error in parameters
 *  -2 if an error occurred during processing
 *  -3 if at least one required procparam file was not found */
int processLineParams ( int argc, char **argv );

bool dontLoadCache ( int argc, char **argv );

int main (int argc, char **argv)
{
    setlocale (LC_ALL, "");
    setlocale (LC_NUMERIC, "C"); // to set decimal point to "."

    rtengine::initTcmallocHooks();

    Gio::init ();

    auto& app = App::get();

#ifdef BUILD_BUNDLE
    char exname[512] = {0};
    Glib::ustring exePath;
    // get the path where the rawtherapee executable is stored
#ifdef _WIN32
    WCHAR exnameU[512] = {0};
    GetModuleFileNameW (NULL, exnameU, 511);
    WideCharToMultiByte (CP_UTF8, 0, exnameU, -1, exname, 511, 0, 0 );
#else

    if (readlink ("/proc/self/exe", exname, 511) < 0) {
        strncpy (exname, argv[0], 511);
    }

#endif // _WIN32
    exePath = Glib::path_get_dirname (exname);

    // set paths
    if (Glib::path_is_absolute (DATA_SEARCH_PATH)) {
        app.setArgv0(DATA_SEARCH_PATH);
    } else {
        app.setArgv0(Glib::build_filename(exePath, DATA_SEARCH_PATH));
    }

    if (Glib::path_is_absolute (CREDITS_SEARCH_PATH)) {
        app.setCreditsPath(CREDITS_SEARCH_PATH);
    } else {
        app.setCreditsPath(Glib::build_filename(exePath, CREDITS_SEARCH_PATH));
    }

    if (Glib::path_is_absolute (LICENCE_SEARCH_PATH)) {
        app.setLicensePath(LICENCE_SEARCH_PATH);
    } else {
        app.setLicensePath(Glib::build_filename(exePath, LICENCE_SEARCH_PATH));
    }
#else
    app.setArgv0(DATA_SEARCH_PATH);
    app.setCreditsPath(CREDITS_SEARCH_PATH);
    app.setLicensePath(LICENCE_SEARCH_PATH);
#endif // BUILD_BUNDLE

    Options& options = app.mut_options();
    options.rtSettings.lensfunDbDirectory = LENSFUN_DB_PATH;
    options.rtSettings.lensfunDbBundleDirectory = LENSFUN_DB_PATH;

    bool quickstart = dontLoadCache (argc, argv);

    try {
        Options::load (quickstart);
    } catch (Options::Error &e) {
        std::cerr << std::endl
                  << "FATAL ERROR:" << std::endl
                  << e.get_msg() << std::endl;
        return -2;
    }

    if (options.is_defProfRawMissing()) {
        options.defProfRaw = DEFPROFILE_RAW;
        std::cerr << std::endl
                  << "The default profile for raw photos could not be found or is not set." << std::endl
                  << "Please check your profiles' directory, it may be missing or damaged." << std::endl
                  << "\"" << DEFPROFILE_RAW << "\" will be used instead." << std::endl << std::endl;
    }
    if (options.is_bundledDefProfRawMissing()) {
        std::cerr << std::endl
                  << "The bundled profile \"" << options.defProfRaw << "\" could not be found!" << std::endl
                  << "Your installation could be damaged." << std::endl
                  << "Default internal values will be used instead." << std::endl << std::endl;
        options.defProfRaw = DEFPROFILE_INTERNAL;
    }

    if (options.is_defProfImgMissing()) {
        options.defProfImg = DEFPROFILE_IMG;
        std::cerr << std::endl
                  << "The default profile for non-raw photos could not be found or is not set." << std::endl
                  << "Please check your profiles' directory, it may be missing or damaged." << std::endl
                  << "\"" << DEFPROFILE_IMG << "\" will be used instead." << std::endl << std::endl;
    }
    if (options.is_bundledDefProfImgMissing()) {
        std::cerr << std::endl
                  << "The bundled profile " << options.defProfImg << " could not be found!" << std::endl
                  << "Your installation could be damaged." << std::endl
                  << "Default internal values will be used instead." << std::endl << std::endl;
        options.defProfImg = DEFPROFILE_INTERNAL;
    }

    TIFFSetWarningHandler (nullptr);   // avoid annoying message boxes

#ifndef _WIN32

    // Move the old path to the new one if the new does not exist
    if (Glib::file_test (Glib::build_filename (options.rtdir, "cache"), Glib::FILE_TEST_IS_DIR) && !Glib::file_test (options.cacheBaseDir, Glib::FILE_TEST_IS_DIR)) {
        if (g_rename (Glib::build_filename (options.rtdir, "cache").c_str (), options.cacheBaseDir.c_str ()) == -1) {
            std::cout << "g_rename " <<  Glib::build_filename (options.rtdir, "cache").c_str () << " => " << options.cacheBaseDir.c_str () << " failed." << std::endl;
        }
    }

#endif

    int ret = 0;

    // printing RT's version in all case, particularly useful for the 'verbose' mode, but also for the batch processing
    std::cout << "RawTherapee, version " << RTVERSION << ", command line." << std::endl;

    if (argc > 1) {
        ret = processLineParams (argc, argv);
    } else {
        std::cout << "Terminating without anything to do." << std::endl;
    }

    return ret;
}

void deleteProcParams (std::vector<rtengine::procparams::PartialProfile*> &pparams)
{
    for (unsigned int i = 0; i < pparams.size(); i++) {
        pparams[i]->deleteInstance();
        delete pparams[i];
        pparams[i] = NULL;
    }

    return;
}


bool dontLoadCache ( int argc, char **argv )
{
    for (int iArg = 1; iArg < argc; iArg++) {
        Glib::ustring currParam (argv[iArg]);
#if ECLIPSE_ARGS
        currParam = currParam.substr (1, currParam.length() - 2);
#endif
        if ( currParam.length() > 1 && currParam.at(0) == '-' && currParam.at(1) == 'q' ) {
            return true;
        }
    }

    return false;
}

int processLineParams ( int argc, char **argv )
{
    rtengine::procparams::PartialProfile *rawParams = nullptr, *imgParams = nullptr;
    std::vector<Glib::ustring> inputFiles;
    Glib::ustring outputPath;
    std::vector<rtengine::procparams::PartialProfile*> processingParams;
    bool outputDirectory = false;
    bool leaveUntouched = false;
    bool overwriteFiles = false;
    bool sideProcParams = false;
    bool copyParamsFile = false;
    bool skipIfNoSidecar = false;
    bool allExtensions = false;
    bool useDefault = false;
    unsigned int sideCarFilePos = 0;
    int compression = 92;
    int subsampling = 3;
    int bits = -1;
    bool isFloat = false;
    std::string outputType;
    unsigned errors = 0;

    auto& options = App::get().mut_options();
    for ( int iArg = 1; iArg < argc; iArg++) {
        Glib::ustring currParam (argv[iArg]);
        if ( currParam.empty() ) {
            continue;
        }
#if ECLIPSE_ARGS
        currParam = currParam.substr (1, currParam.length() - 2);
#endif

        if ( currParam.at (0) == '-' && currParam.size() > 1) {
            switch ( currParam.at (1) ) {
                case '-':
                    // GTK --argument, we're skipping it
                    break;

                case 'O':
                    copyParamsFile = true;
                    // fall through

                case 'o': // outputfile or dir
                    if ( iArg + 1 < argc ) {
                        iArg++;
                        outputPath = Glib::ustring (fname_to_utf8 (argv[iArg]));
#if ECLIPSE_ARGS
                        outputPath = outputPath.substr (1, outputPath.length() - 2);
#endif

                        if (outputPath.substr (0, 9) == "/dev/null") {
                            outputPath.assign ("/dev/null"); // removing any useless chars or filename
                            outputDirectory = false;
                            leaveUntouched = true;
                        } else if (Glib::file_test (outputPath, Glib::FILE_TEST_IS_DIR)) {
                            outputDirectory = true;
                        }
                    }

                    break;

                case 'p': // processing parameters for all inputs; all set procparams are required, so

                    // RT stop if any of them can't be loaded for any reason.
                    if ( iArg + 1 < argc ) {
                        iArg++;
                        Glib::ustring fname (fname_to_utf8 (argv[iArg]));
#if ECLIPSE_ARGS
                        fname = fname.substr (1, fname.length() - 2);
#endif

                        if (fname.at (0) == '-') {
                            std::cerr << "Error: filename missing next to the -p switch." << std::endl;
                            deleteProcParams (processingParams);
                            return -3;
                        }

                        rtengine::procparams::PartialProfile* currentParams = new rtengine::procparams::PartialProfile (true);

                        if (! (currentParams->load ( fname ))) {
                            processingParams.push_back (currentParams);
                        } else {
                            std::cerr << "Error: \"" << fname << "\" not found." << std::endl;
                            deleteProcParams (processingParams);
                            return -3;
                        }
                    }

                    break;

                case 'S':
                    skipIfNoSidecar = true;
                    // fall through

                case 's': // Processing params next to file (file extension appended)
                    sideProcParams = true;
                    sideCarFilePos = processingParams.size();
                    break;

                case 'd':
                    useDefault = true;
                    break;

                case 'q':
                    break;

                case 'Y':
                    overwriteFiles = true;
                    break;

                case 'a':
                    allExtensions = true;
                    break;

                case 'j':
                    if (currParam.length() > 2 && currParam.at (2) == 's') {
                        if (currParam.length() == 3) {
                            std::cerr << "Error: the -js switch requires a mandatory value!" << std::endl;
                            deleteProcParams (processingParams);
                            return -3;
                        }

                        // looking for the subsampling parameter
                        subsampling = atoi (currParam.substr (3).c_str());

                        if (subsampling < 1 || subsampling > 3) {
                            std::cerr << "Error: the value accompanying the -js switch has to be in the [1-3] range!" << std::endl;
                            deleteProcParams (processingParams);
                            return -3;
                        }
                    } else {
                        outputType = "jpg";
                        if(currParam.size() < 3) {
                            compression = 92;
                        } else {
                            compression = atoi (currParam.substr (2).c_str());

                            if (compression < 0 || compression > 100) {
                                std::cerr << "Error: the value accompanying the -j switch has to be in the [0-100] range!" << std::endl;
                                deleteProcParams (processingParams);
                                return -3;
                            }
                        }
                    }

                    break;

                case 'b':
                    bits = atoi (currParam.substr (2).c_str());

                    if (currParam.length() >= 3 && currParam.at(2) == '8') { // -b8
                        bits = 8;
                    } else if (currParam.length() >= 4 && currParam.length() <= 5 && currParam.at(2) == '1' && currParam.at(3) == '6') { // -b16, -b16f
                        bits = 16;
                        if (currParam.length() == 5 && currParam.at(4) == 'f') {
                            isFloat = true;
                        }
                    } else if (currParam.length() >= 4 && currParam.length() <= 5 && currParam.at(2) == '3' && currParam.at(3) == '2') { // -b32 == -b32f
                        bits = 32;
                        isFloat = true;
                    }

                    if (bits != 8 && bits != 16 && bits != 32) {
                        std::cerr << "Error: specify output bit depth per channel as -b8 for 8-bit integer, -b16 for 16-bit integer, -b16f for 16-bit float or -b32 for 32-bit float." << std::endl;
                        deleteProcParams (processingParams);
                        return -3;
                    }

                    break;

                case 't':
                    outputType = "tif";
                    compression = ((currParam.size() < 3 || currParam.at (2) != 'z') ? 0 : 1);
                    break;

                case 'n':
                    outputType = "png";
                    compression = -1;
                    break;

                case 'f':
                    fast_export = true;
                    break;

                case 'c': // MUST be last option
                    while (iArg + 1 < argc) {
                        iArg++;
                        Glib::ustring argument (fname_to_utf8 (argv[iArg]));
#if ECLIPSE_ARGS
                        argument = argument.substr (1, argument.length() - 2);
#endif

                        if (!Glib::file_test (argument, Glib::FILE_TEST_EXISTS)) {
                            std::cout << "\"" << argument << "\"  doesn't exist!" << std::endl;
                            continue;
                        }

                        if (Glib::file_test (argument, Glib::FILE_TEST_IS_REGULAR)) {
                            bool notAll = allExtensions && !options.is_parse_extention (argument);
                            bool notRetained = !allExtensions && !options.has_retained_extention (argument);

                            if (notAll || notRetained) {
                                if (notAll) {
                                    std::cout << "\"" << argument << "\"  is not one of the parsed extensions. Image skipped." << std::endl;
                                } else if (notRetained) {
                                    std::cout << "\"" << argument << "\"  is not one of the selected parsed extensions. Image skipped." << std::endl;
                                }
                            } else {
                                inputFiles.emplace_back (argument);
                            }

                            continue;

                        }

                        if (Glib::file_test (argument, Glib::FILE_TEST_IS_DIR)) {

                            auto dir = Gio::File::create_for_path (argument);

                            if (!dir || !dir->query_exists()) {
                                continue;
                            }

                            try {

                                auto enumerator = dir->enumerate_children ("standard::name,standard::type");

                                while (auto file = enumerator->next_file()) {

                                    const auto fileName = Glib::build_filename (argument, file->get_name());
                                    bool isDir = file->get_file_type() == Gio::FILE_TYPE_DIRECTORY;
                                    bool notAll = allExtensions && !options.is_parse_extention (fileName);
                                    bool notRetained = !allExtensions && !options.has_retained_extention (fileName);

                                    if (isDir || notAll || notRetained) {
                                        if (isDir) {
                                            std::cout << "\"" << fileName << "\"  is a folder. Folder skipped" << std::endl;
                                        } else if (notAll) {
                                            std::cout << "\"" << fileName << "\"  is not one of the parsed extensions. Image skipped." << std::endl;
                                        } else if (notRetained) {
                                            std::cout << "\"" << fileName << "\"  is not one of the selected parsed extensions. Image skipped." << std::endl;
                                        }

                                        continue;

                                    }

                                    if (sideProcParams && skipIfNoSidecar) {
                                        // look for the sidecar proc params
                                        if (!Glib::file_test (fileName + App::PARAM_FILE_EXTENSION, Glib::FILE_TEST_EXISTS)) {
                                            std::cout << "\"" << fileName << "\"  has no side-car file. Image skipped." << std::endl;
                                            continue;
                                        }
                                    }

                                    inputFiles.emplace_back (fileName);
                                }

                            } catch (Glib::Exception&) {}

                            continue;
                        }

                        std::cerr << "\"" << argument << "\" is neither a regular file nor a directory." << std::endl;
                    }

                    break;

                case 'h':
                case '?':
                default: {
                    Glib::ustring pparamsExt = App::PARAM_FILE_EXTENSION.substr (1);
                    std::cout << "  An advanced, cross-platform program for developing raw photos." << std::endl;
                    std::cout << std::endl;
                    std::cout << "  Website: http://www.rawtherapee.com/" << std::endl;
                    std::cout << "  Documentation: http://rawpedia.rawtherapee.com/" << std::endl;
                    std::cout << "  Forum: https://discuss.pixls.us/c/software/rawtherapee" << std::endl;
                    std::cout << "  Code and bug reports: https://github.com/RawTherapee/RawTherapee" << std::endl;
                    std::cout << std::endl;
                    std::cout << "Symbols:" << std::endl;
                    std::cout << "  <Chevrons> indicate parameters you can change." << std::endl;
                    std::cout << "  [Square brackets] mean the parameter is optional." << std::endl;
                    std::cout << "  The pipe symbol | indicates a choice of one or the other." << std::endl;
                    std::cout << "  The dash symbol - denotes a range of possible values from one to the other." << std::endl;
                    std::cout << std::endl;
                    std::cout << "Usage:" << std::endl;
                    std::cout << "  " << Glib::path_get_basename (argv[0]) << " -c <dir>|<files>   Convert files in batch with default parameters." << std::endl;
                    std::cout << "  " << Glib::path_get_basename (argv[0]) << " <other options> -c <dir>|<files>   Convert files in batch with your own settings." << std::endl;
                    std::cout << std::endl;
                    std::cout << "Options:" << std::endl;
                    std::cout << "  " << Glib::path_get_basename (argv[0]) << "[-o <output>|-O <output>] [-q] [-a] [-s|-S] [-p <one.pp3> [-p <two.pp3> ...] ] [-d] [ -j[1-100] -js<1-3> | -t[z] -b<8|16|16f|32> | -n -b<8|16> ] [-Y] [-f] -c <input>" << std::endl;
                    std::cout << std::endl;
                    std::cout << "  -c <files>       Specify one or more input files or folders." << std::endl;
                    std::cout << "                   When specifying folders, Rawtherapee will look for image file types which comply" << std::endl;
                    std::cout << "                   with the selected extensions (see also '-a')." << std::endl;
                    std::cout << "                   -c must be the last option." << std::endl;
                    std::cout << "  -o <file>|<dir>  Set output file or folder." << std::endl;
                    std::cout << "                   Saves output file alongside input file if -o is not specified." << std::endl;
                    std::cout << "  -O <file>|<dir>  Set output file or folder and copy " << pparamsExt << " file into it." << std::endl;
                    std::cout << "                   Saves output file alongside input file if -O is not specified." << std::endl;
                    std::cout << "  -q               Quick-start mode. Does not load cached files to speedup start time." << std::endl;
                    std::cout << "  -a               Process all supported image file types when specifying a folder, even those" << std::endl;
                    std::cout << "                   not currently selected in Preferences > File Browser > Parsed Extensions." << std::endl;
                    std::cout << "  -s               Use the existing sidecar file to build the processing parameters," << std::endl;
                    std::cout << "                   e.g. for photo.raw there should be a photo.raw." << pparamsExt << " file in the same folder." << std::endl;
                    std::cout << "                   If the sidecar file does not exist, neutral values will be used." << std::endl;
                    std::cout << "  -S               Like -s but skip if the sidecar file does not exist." << std::endl;
                    std::cout << "  -p <file.pp3>    Specify processing profile to be used for all conversions." << std::endl;
                    std::cout << "                   You can specify as many sets of \"-p <file.pp3>\" options as you like," << std::endl;
                    std::cout << "                   each will be built on top of the previous one, as explained below." << std::endl;
                    std::cout << "  -d               Use the default raw or non-raw processing profile as set in" << std::endl;
                    std::cout << "                   Preferences > Image Processing > Default Processing Profile" << std::endl;
                    std::cout << "  -j[1-100]        Specify output to be JPEG (default, if -t and -n are not set)." << std::endl;
                    std::cout << "                   Optionally, specify compression 1-100 (default value: 92)." << std::endl;
                    std::cout << "  -js<1-3>         Specify the JPEG chroma subsampling parameter, where:" << std::endl;
                    std::cout << "                   1 = Best compression:   2x2, 1x1, 1x1 (4:2:0)" << std::endl;
                    std::cout << "                       Chroma halved vertically and horizontally." << std::endl;
                    std::cout << "                   2 = Balanced (default): 2x1, 1x1, 1x1 (4:2:2)" << std::endl;
                    std::cout << "                       Chroma halved horizontally." << std::endl;
                    std::cout << "                   3 = Best quality:       1x1, 1x1, 1x1 (4:4:4)" << std::endl;
                    std::cout << "                       No chroma subsampling." << std::endl;
                    std::cout << "  -b<8|16|16f|32>  Specify bit depth per channel." << std::endl;
                    std::cout << "                   8   = 8-bit integer.  Applies to JPEG, PNG and TIFF. Default for JPEG and PNG." << std::endl;
                    std::cout << "                   16  = 16-bit integer. Applies to TIFF and PNG. Default for TIFF." << std::endl;
                    std::cout << "                   16f = 16-bit float.   Applies to TIFF." << std::endl;
                    std::cout << "                   32  = 32-bit float.   Applies to TIFF." << std::endl;
                    std::cout << "  -t[z]            Specify output to be TIFF." << std::endl;
                    std::cout << "                   Uncompressed by default, or deflate compression with 'z'." << std::endl;
                    std::cout << "  -n               Specify output to be compressed PNG." << std::endl;
                    std::cout << "                   Compression is hard-coded to PNG_FILTER_PAETH, Z_RLE." << std::endl;
                    std::cout << "  -Y               Overwrite output if present." << std::endl;
                    std::cout << "  -f               Use the custom fast-export processing pipeline." << std::endl;
                    std::cout << std::endl;
                    std::cout << "Your " << pparamsExt << " files can be incomplete, RawTherapee will build the final values as follows:" << std::endl;
                    std::cout << "  1- A new processing profile is created using neutral values," << std::endl;
                    std::cout << "  2- If the \"-d\" option is set, the values are overridden by those found in" << std::endl;
                    std::cout << "     the default raw or non-raw processing profile." << std::endl;
                    std::cout << "  3- If one or more \"-p\" options are set, the values are overridden by those" << std::endl;
                    std::cout << "     found in these processing profiles." << std::endl;
                    std::cout << "  4- If the \"-s\" or \"-S\" options are set, the values are finally overridden by those" << std::endl;
                    std::cout << "     found in the sidecar files." << std::endl;
                    std::cout << "  The processing profiles are processed in the order specified on the command line." << std::endl;
                    return -1;
                }
            }
        } else {
            App::get().setArgv1(Glib::ustring (fname_to_utf8 (argv[iArg])));
#if ECLIPSE_ARGS
            App::get().setArgv1(App::get().argv1().substr (1, App::get().argv1().length() - 2));
#endif

            if ( outputDirectory ) {
                options.savePathFolder = outputPath;
                options.saveUsePathTemplate = false;
            } else {
                options.saveUsePathTemplate = true;

                if (options.savePathTemplate.empty())
                    // If the save path template is empty, we use its default value
                {
                    options.savePathTemplate = "%p1/converted/%f";
                }
            }

            if (outputType == "jpg") {
                options.saveFormat.format = outputType;
                options.saveFormat.jpegQuality = compression;
                options.saveFormat.jpegSubSamp = subsampling;
            } else if (outputType == "tif") {
                options.saveFormat.format = outputType;
            } else if (outputType == "png") {
                options.saveFormat.format = outputType;
            }

            break;
        }
    }

    if (bits == -1) {
        if (outputType == "jpg") {
            bits = 8;
        } else if (outputType == "png") {
            bits = 8;
        } else if (outputType == "tif") {
            bits = 16;
        } else {
            bits = 8;
        }
    }

    if ( !App::get().argv1().empty() ) {
        return 1;
    }

    if ( inputFiles.empty() ) {
        return 2;
    }

    if (useDefault) {
        rawParams = new rtengine::procparams::PartialProfile (true, true);
        Glib::ustring profPath = options.findProfilePath (options.defProfRaw);

        if (options.is_defProfRawMissing() || profPath.empty() || (profPath != DEFPROFILE_DYNAMIC && rawParams->load (profPath == DEFPROFILE_INTERNAL ? DEFPROFILE_INTERNAL : Glib::build_filename (profPath, Glib::path_get_basename (options.defProfRaw) + App::PARAM_FILE_EXTENSION)))) {
            std::cerr << "Error: default raw processing profile not found." << std::endl;
            rawParams->deleteInstance();
            delete rawParams;
            deleteProcParams (processingParams);
            return -3;
        }

        imgParams = new rtengine::procparams::PartialProfile (true);
        profPath = options.findProfilePath (options.defProfImg);

        if (options.is_defProfImgMissing() || profPath.empty() || (profPath != DEFPROFILE_DYNAMIC && imgParams->load (profPath == DEFPROFILE_INTERNAL ? DEFPROFILE_INTERNAL : Glib::build_filename (profPath, Glib::path_get_basename (options.defProfImg) + App::PARAM_FILE_EXTENSION)))) {
            std::cerr << "Error: default non-raw processing profile not found." << std::endl;
            imgParams->deleteInstance();
            delete imgParams;
            rawParams->deleteInstance();
            delete rawParams;
            deleteProcParams (processingParams);
            return -3;
        }
    }

    for ( size_t iFile = 0; iFile < inputFiles.size(); iFile++) {

        // Has to be reinstanciated at each profile to have a ProcParams object with default values
        rtengine::procparams::ProcParams currentParams;

        Glib::ustring inputFile = inputFiles[iFile];
        std::cout << "Output is " << bits << "-bit " << (isFloat ? "floating-point" : "integer") << "." << std::endl;
        std::cout << "Processing: " << inputFile << std::endl;

        rtengine::InitialImage* ii = nullptr;
        rtengine::ProcessingJob* job = nullptr;
        int errorCode;
        bool isRaw = false;

        Glib::ustring outputFile;

        if ( outputType.empty() ) {
            outputType = "jpg";
        }

        if ( outputPath.empty() ) {
            Glib::ustring s = inputFile;
            Glib::ustring::size_type ext = s.find_last_of ('.');
            outputFile = s.substr (0, ext) + "." + outputType;
        } else if ( outputDirectory ) {
            Glib::ustring s = Glib::path_get_basename ( inputFile );
            Glib::ustring::size_type ext = s.find_last_of ('.');
            outputFile = Glib::build_filename (outputPath, s.substr (0, ext) + "." + outputType);
        } else {
            if (leaveUntouched) {
                outputFile = outputPath;
            } else {
                Glib::ustring s = outputPath;
                Glib::ustring::size_type ext = s.find_last_of ('.');
                outputFile = s.substr (0, ext) + "." + outputType;
            }
        }

        if ( inputFile == outputFile) {
            std::cerr << "Cannot overwrite: " << inputFile << std::endl;
            continue;
        }

        if ( !overwriteFiles && Glib::file_test ( outputFile, Glib::FILE_TEST_EXISTS ) ) {
            std::cerr << outputFile  << " already exists: use -Y option to overwrite. This image has been skipped." << std::endl;
            continue;
        }

        // Load the image
        isRaw = true;
        Glib::ustring ext = getExtension (inputFile);

        if (ext.lowercase() == "jpg" || ext.lowercase() == "jpeg" || ext.lowercase() == "tif" || ext.lowercase() == "tiff" || ext.lowercase() == "png") {
            isRaw = false;
        }

        ii = rtengine::InitialImage::load ( inputFile, isRaw, &errorCode, nullptr );

        if (!ii) {
            errors++;
            std::cerr << "Error loading file: " << inputFile << std::endl;
            continue;
        }

        if (useDefault) {
            if (isRaw) {
                if (options.defProfRaw == DEFPROFILE_DYNAMIC) {
                    rawParams->deleteInstance();
                    delete rawParams;
                    rawParams = ProfileStore::getInstance()->loadDynamicProfile (ii->getMetaData(), inputFile);
                }

                std::cout << "  Merging default raw processing profile." << std::endl;
                rawParams->applyTo (&currentParams);
            } else {
                if (options.defProfImg == DEFPROFILE_DYNAMIC) {
                    imgParams->deleteInstance();
                    delete imgParams;
                    imgParams = ProfileStore::getInstance()->loadDynamicProfile (ii->getMetaData(), inputFile);
                }

                std::cout << "  Merging default non-raw processing profile." << std::endl;
                imgParams->applyTo (&currentParams);
            }
        }

        bool sideCarFound = false;
        unsigned int i = 0;

        // Iterate the procparams file list in order to build the final ProcParams
        do {
            if (sideProcParams && i == sideCarFilePos) {
                // using the sidecar file
                Glib::ustring sideProcessingParams = inputFile + App::PARAM_FILE_EXTENSION;

                // the "load" method don't reset the procparams values anymore, so values found in the procparam file override the one of currentParams
                if ( !Glib::file_test ( sideProcessingParams, Glib::FILE_TEST_EXISTS ) || currentParams.load ( sideProcessingParams )) {
                    std::cerr << "Warning: sidecar file requested but not found for: " << sideProcessingParams << std::endl;
                } else {
                    sideCarFound = true;
                    std::cout << "  Merging sidecar procparams." << std::endl;
                }
            }

            if ( processingParams.size() > i  ) {
                std::cout << "  Merging procparams #" << i << std::endl;
                processingParams[i]->applyTo (&currentParams);
            }

            i++;
        } while (i < processingParams.size() + (sideProcParams ? 1 : 0));

        if ( sideProcParams && !sideCarFound && skipIfNoSidecar ) {
            delete ii;
            errors++;
            std::cerr << "Error: no sidecar procparams found for: " << inputFile << std::endl;
            continue;
        }

        job = rtengine::ProcessingJob::create (ii, currentParams, fast_export);

        if ( !job ) {
            errors++;
            std::cerr << "Error creating processing for: " << inputFile << std::endl;
            ii->decreaseRef();
            continue;
        }

        // Process image
        rtengine::IImagefloat* resultImage = rtengine::processImage (job, errorCode, nullptr);

        if ( !resultImage ) {
            errors++;
            std::cerr << "Error processing: " << inputFile << std::endl;
            rtengine::ProcessingJob::destroy ( job );
            continue;
        }

        // save image to disk
        if ( outputType == "jpg" ) {
            errorCode = resultImage->saveAsJPEG ( outputFile, compression, subsampling );
        } else if ( outputType == "tif" ) {
            errorCode = resultImage->saveAsTIFF ( outputFile, bits, isFloat, compression == 0  );
        } else if ( outputType == "png" ) {
            errorCode = resultImage->saveAsPNG ( outputFile, bits );
        } else {
            errorCode = resultImage->saveToFile (outputFile);
        }

        if (errorCode) {
            errors++;
            std::cerr << "Error saving to: " << outputFile << std::endl;
        } else {
            if ( copyParamsFile ) {
                Glib::ustring outputProcessingParams = outputFile + App::PARAM_FILE_EXTENSION;
                currentParams.save ( outputProcessingParams );
            }
        }

        ii->decreaseRef();
        delete resultImage;
    }

    if (imgParams) {
        imgParams->deleteInstance();
        delete imgParams;
    }

    if (rawParams) {
        rawParams->deleteInstance();
        delete rawParams;
    }

    deleteProcParams (processingParams);

    return errors > 0 ? -2 : 0;
}
