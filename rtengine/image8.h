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
//
// A class representing a 8 bit rgb image without alpha channel
//
#pragma once

#include "imageio.h"

namespace rtengine
{
class Imagefloat;

class Image8 final : public IImage8, public ImageIO
{

public:

    Image8 ();
    Image8 (int width, int height);
    ~Image8 () override;

    Image8* copy () const;

    void getStdImage (const ColorTemp &ctemp, int tran, Imagefloat* image, const PreviewProps &pp) const override;

    const char* getType () const override
    {
        return sImage8;
    }

    int getBPS () const override
    {
        return 8 * sizeof(unsigned char);
    }

    void getScanline (int row, unsigned char* buffer, int bps, bool isFloat = false) const override;
    void setScanline (int row, const unsigned char* buffer, int bps, unsigned int numSamples) override;

    // functions inherited from IImage*:
    MyMutex& getMutex () override
    {
        return mutex ();
    }

    cmsHPROFILE getProfile () const override
    {
        return getEmbeddedProfile ();
    }

    int saveToFile (const Glib::ustring &fname) const override
    {
        return save (fname);
    }

    int saveAsPNG (const Glib::ustring &fname, int bps = -1) const override
    {
        return savePNG (fname, bps);
    }

    int saveAsJPEG (const Glib::ustring &fname, int quality = 100, int subSamp = 3) const override
    {
        return saveJPEG (fname, quality, subSamp);
    }

    int saveAsTIFF (
        const Glib::ustring &fname,
        int bps = -1,
        bool isFloat = false,
        bool uncompressed = false,
        bool big = false
    ) const override
    {
        return saveTIFF (fname, bps, isFloat, uncompressed, big);
    }

    int saveAsAVIF (const Glib::ustring &fname, int bps = 8, int quality = 90) const override
    {
        return saveAVIF (fname, bps, quality);
    }

    void setSaveProgressListener (ProgressListener* pl) override
    {
        setProgressListener (pl);
    }

};

}
