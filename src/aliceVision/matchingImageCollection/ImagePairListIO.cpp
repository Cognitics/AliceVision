// This file is part of the AliceVision project.
// Copyright (c) 2022 AliceVision contributors.
// Copyright (c) 2012 openMVG contributors.
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "ImagePairListIO.hpp"
#include <aliceVision/system/Logger.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <fstream>

namespace aliceVision {

bool loadPairs(const std::string &sFileName,
               PairSet & pairs,
               int rangeStart,
               int rangeSize)
{
    std::ifstream in(sFileName.c_str());
    if (!in.is_open())
    {
        ALICEVISION_LOG_WARNING("loadPairs: Impossible to read the specified file: \"" << sFileName << "\".");
        return false;
    }
    std::size_t nbLine = 0;
    std::string sValue;

    for (; std::getline( in, sValue ); ++nbLine)
    {
        if (rangeStart != -1 && rangeSize != 0)
        {
            if (nbLine < rangeStart)
                continue;
            if (nbLine >= rangeStart + rangeSize)
                break;
        }

        std::vector<std::string> vec_str;
        boost::trim(sValue);
        boost::split(vec_str, sValue, boost::is_any_of("\t "), boost::token_compress_on);

        const size_t str_size = vec_str.size();
        if (str_size < 2)
        {
            ALICEVISION_LOG_WARNING("loadPairs: Invalid input file: \"" << sFileName << "\".");
            return false;
        }
        std::stringstream oss;
        oss.clear(); oss.str(vec_str[0]);
        size_t I, J;
        oss >> I;
        for (size_t i=1; i<str_size ; ++i)
        {
            oss.clear(); oss.str(vec_str[i]);
            oss >> J;
            if( I == J )
            {
                ALICEVISION_LOG_WARNING("loadPairs: Invalid input file. Image " << I
                                        << " see itself. File: \"" << sFileName << "\".");
                return false;
            }
            Pair pairToInsert = (I < J) ? std::make_pair(I, J) : std::make_pair(J, I);
            if(pairs.find(pairToInsert) != pairs.end())
            {
                // There is no reason to have the same image pair twice in the list of image pairs to match.
                ALICEVISION_LOG_WARNING("loadPairs: image pair (" << I << ", " << J
                                        << ") already added. File: \"" << sFileName << "\".");
            }
            ALICEVISION_LOG_INFO("loadPairs: image pair (" << I << ", " << J << ") added. File: \"" << sFileName << "\".");
            pairs.insert(pairToInsert);
        }
    }
    in.close();
    return true;
}

bool savePairs(const std::string &sFileName, const PairSet & pairs)
{
    std::ofstream outStream(sFileName.c_str());
    if(!outStream.is_open())
    {
        ALICEVISION_LOG_WARNING("savePairs: Impossible to open the output specified file: \"" << sFileName << "\".");
        return false;
    }
    for (PairSet::const_iterator iterP = pairs.begin(); iterP != pairs.end(); ++iterP)
    {
        outStream << iterP->first << ' ' << iterP->second << '\n';
    }
    bool bOk = !outStream.bad();
    outStream.close();
    return bOk;
}
} // namespace aliceVision
