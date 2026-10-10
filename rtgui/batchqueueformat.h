/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2026 RawTherapee contributors
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
#pragma once

#include "options.h"
#include <ostream>
#include <string>
#include <vector>

// Queue columns after the three paths. Append fields to preserve old records.
struct BatchQueueFormat {
    SaveFormat save;
    bool force;
    bool fast;
};

inline void writeBatchQueueFormat(std::ostream& out, const BatchQueueFormat& record)
{
    const auto& sf = record.save;
    out << sf.format << '|' << sf.jpegQuality << '|' << sf.jpegSubSamp << '|'
        << sf.pngBits << '|' << sf.tiffBits << '|' << sf.tiffFloat << '|'
        << sf.tiffUncompressed << '|' << sf.saveParams << '|' << record.force << '|'
        << record.fast << '|' << sf.bigTiff << '|'
        << sf.heifQuality << '|' << sf.heifBits << '|' << sf.heifSubSamp << '|';
}

inline BatchQueueFormat readBatchQueueFormat(std::vector<std::string>::const_iterator value,
    std::vector<std::string>::const_iterator end, const SaveFormat& defaults, bool forceDefault)
{
    BatchQueueFormat record{defaults, forceDefault, false};
    const auto nextInt = [&](int fallback) {
        if (value == end) return fallback;
        const auto& column = *value++;
        try { return std::stoi(column); }
        catch (const std::exception&) { return fallback; }
    };
    auto& sf = record.save;
    if (value != end) sf.format = *value++;
    sf.jpegQuality = nextInt(sf.jpegQuality);
    sf.jpegSubSamp = nextInt(sf.jpegSubSamp);
    sf.pngBits = nextInt(sf.pngBits);
    sf.tiffBits = nextInt(sf.tiffBits);
    sf.tiffFloat = nextInt(sf.tiffFloat) == 1;
    sf.tiffUncompressed = nextInt(sf.tiffUncompressed) != 0;
    sf.saveParams = nextInt(sf.saveParams) != 0;
    record.force = nextInt(record.force) != 0;
    record.fast = nextInt(false) != 0;
    sf.bigTiff = nextInt(sf.bigTiff) != 0;
    // Old records get the HEIF defaults even when current preferences differ.
    sf.heifQuality = nextInt(92);
    sf.heifBits = nextInt(10);
    sf.heifSubSamp = nextInt(1);
    return record;
}
