#include "rawspeed3_capi.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <stdint.h>



std::vector<uint8_t> readfile(char *fn)
{
    std::ifstream is(fn,std::ifstream::binary);
    if(is)
    {
      is.seekg(0, is.end);
      size_t length = is.tellg();
      is.seekg(0, is.beg);
      std::vector<uint8_t> ret;
      ret.resize(length+1);
      is.read ((char*)ret.data(),length);
      is.close();
      ret[length] = 0; // zero terminated
      std::cout << "File: " << fn << "  size:" << ret.size() << std::endl;
      return ret;
    }
    else
        return std::vector<uint8_t>();
}


int main(int ac, char *av[])
{
    if(ac < 2)
    {
        std::cout << "Usage: " << av[0] << " rawfile rawfile2 ...." << std::endl;
        return 1;
    }
    rawspeed3_handle_t  handle = rawspeed3_initdefault();
    if(!handle)
    {
      std::cerr << "Unable to init rs3" << std::endl;
      return 2;
    }

    for(int i = 1; i < ac; i++)
    {
       std::vector<uint8_t> rawdata = readfile(av[i]);
       if (rawdata.size() < 100) {
         std::cerr << "Input file " << av[i] << " too small or nonexistent" << std::endl;
         continue;
       }
       rawspeed3_ret_t result;
       int q = rawspeed3_decodefile(handle, &result, rawdata.data(),
                                    rawdata.size(), true);
       if (q >= rawspeed3_ok && q <= rawspeed3_ok_warnings) {
         std::cout << "File decoded code=" << result.status <<" width=" << result.width
                   << " height=" << result.height 
                   << " pitch=" << result.pitch
                   << " filters=" << std::hex << result.filters << std::dec
                   << " channels=" << result.cpp
                   << " bpp=" << result.bpp
                   << std::endl;
       } else
         std::cout << "RawSpeed wrapper error code:" << result.status
                   << std::endl;

       rawspeed3_release(handle);
    }

    rawspeed3_close(handle);

}