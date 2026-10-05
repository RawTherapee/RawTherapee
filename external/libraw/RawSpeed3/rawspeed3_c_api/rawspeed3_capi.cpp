#include "rawspeed3_capi.h"
#include "RawSpeed-API.h"
#define HAVE_PUGIXML
#include <../pugixml/pugixml.hpp> // for xml_document, xml_pars...

extern const char* _rawspeed3_data_xml;

class rawspeed3_handle_data
{
public:
    rawspeed3_handle_data(const char* cameradefs, bool is_file);
    void release();
    int decodefile(rawspeed3_ret_t* resultp, const void *data, size_t datasize, bool allowunknown);
    ~rawspeed3_handle_data();
private:
    std::unique_ptr <rawspeed::CameraMetaData> cameraMeta;
    std::unique_ptr<rawspeed::RawParser> rawParser;
    std::unique_ptr<rawspeed::RawDecoder> rawDecoder;
};


/* API calls */

extern "C"
{

/*
    void rawspeed3_clearresult(rawspeed3_ret_t* r)
   
    Clears (inits) results structure
*/
DllDef void rawspeed3_clearresult(rawspeed3_ret_t* r)
{
    if(!r) return;
    r->width = r->height = r->bpp = r->cpp = 0;
    r->status = rawspeed_inited;
    r->pitch = r->filters = 0;
    r->pixeldata = nullptr;
}

/*
    rawspeed3_init()
    Init rawspeed3 Camera, returns: 0 on failure, pointer to data block on success
    Cameradefs: cameras.xml in string (is_file == false) or file (is_file == true)
*/

DllDef rawspeed3_handle_t rawspeed3_init(const char* cameradefs, bool is_file)
{
    /* Create rawspeed3_handle_data and return it as void document */
    if(!cameradefs) return nullptr;
    try
    {
        /* code */
		auto *handle = new rawspeed3_handle_data(cameradefs, is_file);
        return (void*)handle;
    }
    catch(const std::exception& e)
    {
        return nullptr;
    }  
}

/*
    rawspeed3_initdefault()
    Init rawspeed3 Cameradefs with built-in cameras.xml (converted to _rawspeed3_data_xml),
    returns: 0 on failure, pointer to data block on success
*/

DllDef rawspeed3_handle_t rawspeed3_initdefault()
{
    return rawspeed3_init(_rawspeed3_data_xml,false);
}

/*
    rawspeed3_decodefile(..)
    parse/decode RAW file passed via memory
    Parameters:
      handle - rawspeed3_handle_t => handle created by rawspeed3_init()
      resultp -> pointer to rawspeed3_ret_t to be filled with
      data -> data buffer with raw file
      datasize -> size of this buffer
      allowunknown -> allow to process unknown cameras (not listed in cameras.xml)
    Return values:
      0 -> OK 
      >=1 -> decode error
     -1 -> Incorrect parameters passed (handle, or data, or datasize)
*/

DllDef int rawspeed3_decodefile(rawspeed3_handle_t handle, rawspeed3_ret_t* resultp, 
    const void *data, size_t datasize, bool allowunknown)
{
    if(!handle || !resultp || !data || datasize > 2UL * 1024UL * 1024UL * 1024UL)
    {
        if(resultp)
            resultp->status = rawspeed3_param_error;
        return -1;
    }
    rawspeed3_clearresult(resultp);
	auto *p = static_cast<rawspeed3_handle_data*>(handle);
    return p->decodefile(resultp,data,datasize,allowunknown);
}

/*
    void rawspeed3_release(rawspeed3_handle_t handle)

    release internal raw data buffer and error code;
*/

/* release internal raw data buffer and errmessage (if any) */
DllDef void rawspeed3_release(rawspeed3_handle_t handle)
{
    if(!handle) return;
	auto *p =  static_cast<rawspeed3_handle_data*>(handle);
    p->release();
}

/* close handle: release all internal data */
DllDef void rawspeed3_close(rawspeed3_handle_t handle)
{
    if(!handle) return;
	auto *p = static_cast<rawspeed3_handle_data*>(handle);
    delete p;
}

} /* extern "C" */

// == Implementation

int rawspeed3_handle_data::decodefile(rawspeed3_ret_t* resultp, 
    const void *data, size_t datasize, bool allowunknown)
{
    if(!cameraMeta)
    {
        resultp->status = rawspeed3_not_inited;
        return rawspeed3_not_inited;
    }
    try
    {
        rawspeed::Buffer buffer( static_cast<const uint8_t*>(data),datasize);
        release();
        rawParser = std::make_unique<rawspeed::RawParser>(buffer);
        auto d(rawParser->getDecoder(cameraMeta.get()));
        if(!d)
        {
            resultp->status = rawspeed3_no_decoder;
            return rawspeed3_no_decoder;
        }

        d->applyCrop = false;
        d->failOnUnknown = !allowunknown;
        d->interpolateBadPixels = false;
        d->applyStage1DngOpcodes = false;
        d->fujiRotate = false;
        d->applyCrop = false;

        try {
          d->checkSupport(cameraMeta.get());
        } catch (...) {
            release();
            resultp->status = rawspeed3_not_supported;
            return resultp->status;
        }

        rawspeed::RawImage r = d->mRaw;
        d->decodeMetaData(cameraMeta.get());

        d->checkSupport(cameraMeta.get());
        d->decodeRaw();
        d->decodeMetaData(cameraMeta.get());
        r = d->mRaw;

        rawDecoder = std::move(d);
        // we're here w/o exceptions: success
        const rawspeed::iPoint2D dimUncropped = r->getUncroppedDim();
        resultp->width = dimUncropped.x;
        resultp->height = dimUncropped.y;
        resultp->filters = r->cfa.getDcrawFilter();
        resultp->cpp = r->getCpp();
        resultp->bpp = r->getBpp();
        resultp->pitch = r->pitch;
        resultp->pixeldata = r->getDataUncropped(0,0);
        const auto errors = r->getErrors();
        resultp->status = errors.empty()? rawspeed3_ok : rawspeed3_ok_warnings;
        return resultp->status;
        /* code */
    }
    catch(...)
    {
        resultp->status = rawspeed3_processing_error;
        return rawspeed3_processing_error;
    }  
}

namespace rawspeed
{
	class CameraMetaDataFromMem : public CameraMetaData
	{
	public:
		explicit CameraMetaDataFromMem(const char* xmlstring);
	};
}


rawspeed3_handle_data::rawspeed3_handle_data(const char* cameradefs, bool is_file)
    : rawParser(nullptr)
{
  cameraMeta = is_file ? std::make_unique<rawspeed::CameraMetaData>(cameradefs)
                       : std::make_unique <rawspeed::CameraMetaDataFromMem>(cameradefs);
}
rawspeed3_handle_data::~rawspeed3_handle_data()
{
    release();
    cameraMeta.reset();
}

void rawspeed3_handle_data::release()
{
    if(rawDecoder)
        rawDecoder.reset();
    if (rawParser)
        rawParser.reset();
}

// Camera metadata from mem
namespace rawspeed
{
	CameraMetaDataFromMem::CameraMetaDataFromMem(const char* document)
	{
		using pugi::xml_node;
		using pugi::xml_document;
		using pugi::xml_parse_result;

		xml_document doc;
		xml_parse_result result = doc.load_string(document);

		if (!result)
			throw "Camera definitions parse error";

		for (xml_node camera : doc.child("Cameras").children("Camera"))
		{
			const auto* cam = addCamera(std::make_unique<Camera>(camera));

			if (cam == nullptr)
				continue;

			// Create cameras for aliases.
			for (auto i = 0UL; i < cam->aliases.size(); i++)
				addCamera(std::make_unique<Camera>(cam, i));
		}
	}
} // namespace rawspeed
