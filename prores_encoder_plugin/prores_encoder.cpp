#include "prores_encoder.h"

#include <assert.h>
#include <cstring>
#include <vector>
#include <stdint.h>
#include <iostream>

#include <algorithm>


enum { X264_RC_CQP, X264_RC_CRF, X264_RC_ABR };
static const char * const prores_preset_names[] = { "proxy", "LT", "422", "422HQ", "4444", "4444HQ", 0 };

static const char * const prores_tune_names[] = { "film", "animation", "grain", "stillimage", "psnr", "ssim", "fastdecode", "zerolatency", 0 };

static const char * const prores_profile_names[] = { "baseline", "main", "high", "high10", "high422", "high444", 0 };

// NOTE: When creating a plugin for release, please generate a new Codec UUID in order to prevent conflicts with other third-party plugins.
const uint8_t ProResEncoder::s_UUID[] = { 0x6a, 0x88, 0xe8, 0x41, 0xd8, 0xe4, 0x41, 0x4b, 0x87, 0x9e, 0xa4, 0x80, 0xfc, 0x90, 0xda, 0xb4 };

static std::string s_TmpFileName = "/tmp/x264_multipass.log";

class UISettingsController
{
public:
    UISettingsController()
    {
        InitDefaults();
    }

    explicit UISettingsController(const HostCodecConfigCommon& p_CommonProps)
        : m_CommonProps(p_CommonProps)
    {
        InitDefaults();
    }

    ~UISettingsController()
    {
    }

    void Load(IPropertyProvider* p_pValues)
    {
        uint8_t val8 = 0;
        p_pValues->GetUINT8("x264_reset", val8);
        if (val8 != 0)
        {
            *this = UISettingsController();
            return;
        }

        p_pValues->GetINT32("x264_enc_preset", m_EncPreset);
        p_pValues->GetINT32("x264_tune", m_Tune);
        p_pValues->GetINT32("x264_profile", m_Profile);

        p_pValues->GetINT32("x264_num_passes", m_NumPasses);
        p_pValues->GetINT32("x264_q_mode", m_QualityMode);
        p_pValues->GetINT32("x264_qp", m_QP);
        p_pValues->GetINT32("x264_bitrate", m_BitRate);
        p_pValues->GetString("x264_enc_markers", m_MarkerColor);
    }

    StatusCode Render(HostListRef* p_pSettingsList)
    {
        StatusCode err = RenderGeneral(p_pSettingsList);
        if (err != errNone)
        {
            return err;
        }

        {
            HostUIConfigEntryRef item("x264_separator");
            item.MakeSeparator();
            if (!item.IsSuccess() || !p_pSettingsList->Append(&item))
            {
                g_Log(logLevelError, "X264 Plugin :: Failed to add a separator entry");
                return errFail;
            }
        }

        err = RenderQuality(p_pSettingsList);
        if (err != errNone)
        {
            return err;
        }

        {
            HostUIConfigEntryRef item("x264_reset");
            item.MakeButton("Reset");
            item.SetTriggersUpdate(true);
            if (!item.IsSuccess() || !p_pSettingsList->Append(&item))
            {
                g_Log(logLevelError, "X264 Plugin :: Failed to populate the button entry");
                return errFail;
            }
        }

        return errNone;
    }

private:
    void InitDefaults()
    {
        m_EncPreset = 1;
        m_Tune = 1;
        m_Profile = 0;
        m_NumPasses = 1;
        m_QualityMode = 0;// X264_RC_CQP;
        m_QP = 25;
        m_BitRate = 0;
    }

    StatusCode RenderGeneral(HostListRef* p_pSettingsList)
    {
        if (0)
        {
            HostUIConfigEntryRef item("x264_lbl_general");
            item.MakeLabel("General Settings");

            if (!item.IsSuccess() || !p_pSettingsList->Append(&item))
            {
                g_Log(logLevelError, "X264 Plugin :: Failed to populate general label entry");
                return errFail;
            }
        }

        // Markers selection
        if (m_CommonProps.GetContainer().size() >= 32)
        {
            // plugin container in string "plugin_UUID:container_UUID" or "container_UUID" with UUID 32 characters
            // or "mov", "mp4" etc if non-plugin container
            HostUIConfigEntryRef item("x264_enc_markers");
            item.MakeMarkerColorSelector("Chapter Marker", "Marker 1", m_MarkerColor);
            if (!item.IsSuccess() || !p_pSettingsList->Append(&item))
            {
                g_Log(logLevelError, "X264 Plugin :: Failed to populate encoder preset UI entry");
                assert(false);
                return errFail;
            }
        }

        // Preset combobox
        {
            HostUIConfigEntryRef item("x264_enc_preset");

            std::vector<std::string> textsVec;
            std::vector<int> valuesVec;

            int32_t curVal = 1;
            const char* const* pPresets = prores_preset_names;
            while (*pPresets != 0)
            {
                valuesVec.push_back(curVal++);
                textsVec.push_back(*pPresets);
                ++pPresets;
            }

            item.MakeComboBox("Encoder Preset", textsVec, valuesVec, m_EncPreset);
            if (!item.IsSuccess() || !p_pSettingsList->Append(&item))
            {
                g_Log(logLevelError, "X264 Plugin :: Failed to populate encoder preset UI entry");
                return errFail;
            }
        }

        // Tune combobox
        {
            HostUIConfigEntryRef item("x264_tune");

            std::vector<std::string> textsVec;
            std::vector<int> valuesVec;

            int32_t curVal = 1;
            const char* const* pPresets = prores_preset_names;
            while (*pPresets != 0)
            {
                valuesVec.push_back(curVal++);
                textsVec.push_back(*pPresets);
                ++pPresets;
            }

            item.MakeComboBox("Tune", textsVec, valuesVec, m_Tune);
            if (!item.IsSuccess() || !p_pSettingsList->Append(&item))
            {
                g_Log(logLevelError, "X264 Plugin :: Failed to populate tune UI entry");
                return errFail;
            }
        }

        // Profile combobox
        {
            HostUIConfigEntryRef item("x264_profile");

            std::vector<std::string> textsVec;
            std::vector<int> valuesVec;

            textsVec.push_back("Auto");
            valuesVec.push_back(3);
            textsVec.push_back("Baseline");
            valuesVec.push_back(1);
            textsVec.push_back("Main");
            valuesVec.push_back(2);
            textsVec.push_back("High");
            valuesVec.push_back(3);
            textsVec.push_back("High 422");
            valuesVec.push_back(4);

            item.MakeComboBox("h264 Profile", textsVec, valuesVec, m_Profile);
            if (!item.IsSuccess() || !p_pSettingsList->Append(&item))
            {
                g_Log(logLevelError, "X264 Plugin :: Failed to populate profile UI entry");
                return errFail;
            }
        }

        return errNone;
    }

    StatusCode RenderQuality(HostListRef* p_pSettingsList)
    {
        if (0)
        {
            HostUIConfigEntryRef item("x264_lbl_quality");
            item.MakeLabel("Quality Settings");

            if (!item.IsSuccess() || !p_pSettingsList->Append(&item))
            {
                g_Log(logLevelError, "X264 Plugin :: Failed to populate quality label entry");
                return errFail;
            }
        }

        {
            HostUIConfigEntryRef item("x264_num_passes");

            std::vector<std::string> textsVec;
            std::vector<int> valuesVec;

            textsVec.push_back("1-Pass");
            valuesVec.push_back(1);
            textsVec.push_back("2-Pass");
            valuesVec.push_back(2);

            item.MakeComboBox("Passes", textsVec, valuesVec, m_NumPasses);
            item.SetTriggersUpdate(true);
            if (!item.IsSuccess() || !p_pSettingsList->Append(&item))
            {
                g_Log(logLevelError, "X264 Plugin :: Failed to populate passes UI entry");
                return errFail;
            }
        }

        if (m_NumPasses < 2)
        {
            HostUIConfigEntryRef item("x264_q_mode");

            std::vector<std::string> textsVec;
            std::vector<int> valuesVec;

            textsVec.push_back("Constant Quality");
            valuesVec.push_back(X264_RC_CQP);
            textsVec.push_back("Constant Rate Factor");
            valuesVec.push_back(X264_RC_CRF);

            textsVec.push_back("Variable Rate");
            valuesVec.push_back(X264_RC_ABR);

            item.MakeRadioBox("Quality Control", textsVec, valuesVec, GetQualityMode());
            item.SetTriggersUpdate(true);

            if (!item.IsSuccess() || !p_pSettingsList->Append(&item))
            {
                g_Log(logLevelError, "X264 Plugin :: Failed to populate quality UI entry");
                return errFail;
            }
        }

        {
            HostUIConfigEntryRef item("x264_qp");
            const char* pLabel = NULL;
            if (m_QP < 17)
            {
                pLabel = "(high)";
            }
            else if (m_QP < 34)
            {
                pLabel = "(medium)";
            }
            else
            {
                pLabel = "(low)";
            }
            item.MakeSlider("Factor", pLabel, m_QP, 1, 51, 25);
            item.SetTriggersUpdate(true);
            item.SetHidden((m_QualityMode == X264_RC_ABR) || (m_NumPasses > 1));
            if (!item.IsSuccess() || !p_pSettingsList->Append(&item))
            {
                g_Log(logLevelError, "X264 Plugin :: Failed to populate qp slider UI entry");
                return errFail;
            }
        }

        {
            HostUIConfigEntryRef item("x264_bitrate");
            item.MakeSlider("Bit Rate", "KBps", m_BitRate, 100, 3000, 1);
            item.SetHidden((m_QualityMode != X264_RC_ABR) && (m_NumPasses < 2));

            if (!item.IsSuccess() || !p_pSettingsList->Append(&item))
            {
                g_Log(logLevelError, "X264 Plugin :: Failed to populate bitrate slider UI entry");
                return errFail;
            }
        }

        return errNone;
    }

public:
    int32_t GetNumPasses()
    {
        return m_NumPasses;
    }

    const char* GetEncPreset() const
    {
        return prores_preset_names[m_EncPreset];
    }

    const char* GetTune() const
    {
        return prores_tune_names[m_Tune];
    }

    const char* GetProfile() const
    {
        const char* pProfile = NULL;
        switch (m_Profile)
        {
            case 1:
                pProfile = prores_profile_names[0];
                break;
            case 2:
                pProfile = prores_profile_names[1];
                break;
            case 3:
                pProfile = prores_profile_names[2];
                break;
            case 4:
                pProfile = prores_profile_names[4];
            default:
                break;
        }

        return pProfile;
    }

    int32_t GetQualityMode() const
    {
        return (m_NumPasses == 2) ? X264_RC_ABR : m_QualityMode;
    }

    int32_t GetQP() const
    {
        return std::max<int>(0, m_QP);
    }

    int32_t GetBitRate() const
    {
        return m_BitRate * 8;
    }

    const std::string& GetMarkerColor() const
    {
        return m_MarkerColor;
    }

private:
    HostCodecConfigCommon m_CommonProps;
    std::string m_MarkerColor;
    int32_t m_EncPreset;
    int32_t m_Tune;
    int32_t m_Profile;

    int32_t m_NumPasses;
    int32_t m_QualityMode;
    int32_t m_QP;
    int32_t m_BitRate;
};

StatusCode ProResEncoder::s_GetEncoderSettings(HostPropertyCollectionRef* p_pValues, HostListRef* p_pSettingsList)
{
    HostCodecConfigCommon commonProps;
    commonProps.Load(p_pValues);

    UISettingsController settings(commonProps);
    settings.Load(p_pValues);

    return settings.Render(p_pSettingsList);
}

StatusCode ProResEncoder::s_RegisterCodecs(HostListRef* p_pList)
{
    // add x264 encoder
    HostPropertyCollectionRef codecInfo;
    if (!codecInfo.IsValid())
    {
        return errAlloc;
    }

    codecInfo.SetProperty(pIOPropUUID, propTypeUInt8, ProResEncoder::s_UUID, 16);

    const char* pCodecName = "ProRes Plugin";
    codecInfo.SetProperty(pIOPropName, propTypeString, pCodecName, strlen(pCodecName));

    const char* pCodecGroup = "ProRes";
    codecInfo.SetProperty(pIOPropGroup, propTypeString, pCodecGroup, strlen(pCodecGroup));

    uint32_t val = 'apcn';
    codecInfo.SetProperty(pIOPropFourCC, propTypeUInt32, &val, 1);

    val = mediaVideo;
    codecInfo.SetProperty(pIOPropMediaType, propTypeUInt32, &val, 1);

    val = dirEncode;
    codecInfo.SetProperty(pIOPropCodecDirection, propTypeUInt32, &val, 1);

    val = clrUYVY;
    codecInfo.SetProperty(pIOPropColorModel, propTypeUInt32, &val, 1);

    // Optionally enable both Data Ranges, Video will be default for "Auto" thus "0" value goes first
    std::vector<uint8_t> dataRangeVec;
    dataRangeVec.push_back(0);
    dataRangeVec.push_back(1);
    codecInfo.SetProperty(pIOPropDataRange, propTypeUInt8, dataRangeVec.data(), dataRangeVec.size());

    uint8_t hSampling = 2;
    uint8_t vSampling = 1;
    codecInfo.SetProperty(pIOPropHSubsampling, propTypeUInt8, &hSampling, 1);
    codecInfo.SetProperty(pIOPropVSubsampling, propTypeUInt8, &vSampling, 1);

    val = 8;
    codecInfo.SetProperty(pIOPropBitDepth, propTypeUInt32, &val, 1);
    codecInfo.SetProperty(pIOPropBitsPerSample, propTypeUInt32, &val, 1);

    const uint32_t temp = 0;
    codecInfo.SetProperty(pIOPropTemporalReordering, propTypeUInt32, &temp, 1);

    const uint8_t fieldSupport = (fieldProgressive | fieldTop | fieldBottom);
    codecInfo.SetProperty(pIOPropFieldOrder, propTypeUInt8, &fieldSupport, 1);

    // fill supported containers, would one need the plugin container to handle the encoding internally,
    // just create a dummy passthrough codec which will pass the buffer for output unchanged
    // but if nothing extraordinary is required let Resolve trigger the codec encode function and pass the output buffer to the writer
    std::vector<std::string> containerVec;
    containerVec.push_back("mp4");
    containerVec.push_back("mov");
    containerVec.push_back("ad903d5702f24ac19dde8faca3488051");
    std::string valStrings;
    for (size_t i = 0; i < containerVec.size(); ++i)
    {
        valStrings.append(containerVec[i]);
        if (i < (containerVec.size() - 1))
        {
            valStrings.append(1, '\0');
        }
    }

    codecInfo.SetProperty(pIOPropContainerList, propTypeString, valStrings.c_str(), valStrings.size());

    if (!p_pList->Append(&codecInfo))
    {
        return errFail;
    }

    return errNone;
}

ProResEncoder::ProResEncoder()
    : m_pContext(0)
    , m_ColorModel(-1)
    , m_Error(errNone)
{
    av_register_all();
    avcodec_register_all();
}

ProResEncoder::~ProResEncoder()
{
    if (m_pContext)
    {
        avio_close(m_pContext->pb);
        m_pContext = 0;
    }
}

void ProResEncoder::DoFlush()
{
    if (m_Error != errNone)
    {
        return;
    }

    StatusCode sts = DoProcess(NULL);
    while (sts == errNone)
    {
        sts = DoProcess(NULL);
    }

}

StatusCode ProResEncoder::DoInit(HostPropertyCollectionRef* p_pProps)
{
    // fill average frame size if have byte rate
    uint32_t val = clrUYVY;
    p_pProps->SetProperty(pIOPropColorModel, propTypeUInt32, &val, 1);

    uint8_t hSampling = 2;
    uint8_t vSampling = 1;
    p_pProps->SetProperty(pIOPropHSubsampling, propTypeUInt8, &hSampling, 1);
    p_pProps->SetProperty(pIOPropVSubsampling, propTypeUInt8, &vSampling, 1);

    return errNone;
}

StatusCode ProResEncoder::DoOpen(HostBufferRef* p_pBuff)
{
    assert(m_pContext == NULL);

    m_CommonProps.Load(p_pBuff);

    const std::string& path = m_CommonProps.GetPath();

    m_pSettings.reset(new UISettingsController(m_CommonProps));
    m_pSettings->Load(p_pBuff);

  if (m_pContext)
    {
        avio_close(m_pContext->pb);
        m_pContext = 0;
    }

    // Find the ProRes codec
    AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_PRORES);
    if (!codec) {
        std::cerr << "ProRes codec not found" << std::endl;
        return errFail;
    }

    // Create a new AVStream for the video
    AVStream* outStream = avformat_new_stream(m_pContext, codec);
    if (!outStream) {
        std::cerr << "Failed to create new stream" << std::endl;
        return errFail;
    }

    // Initialize the codec context
    AVCodecContext* codecContext = avcodec_alloc_context3(codec);
    if (!codecContext) {
        std::cerr << "Failed to allocate codec context" << std::endl;
        return errFail;
    }

    // Set codec parameters (e.g., width, height, bitrate, etc.)
    codecContext->width = m_CommonProps.GetWidth();
    codecContext->height = m_CommonProps.GetHeight();
    codecContext->bit_rate = 5000000;
    codecContext->codec_id = AV_CODEC_ID_PRORES;
    codecContext->codec_type = AVMEDIA_TYPE_VIDEO;
    codecContext->pix_fmt = AV_PIX_FMT_YUV422P10;
    codecContext->time_base = {
        static_cast<int>(m_CommonProps.GetFrameRateNum()), 
        static_cast<int>(m_CommonProps.GetFrameRateDen())
        };

   
    if (avcodec_open2(codecContext, codec, nullptr) < 0) {
        std::cerr << "Could not open codec" << std::endl;
        return errFail;
    }

    
    return errNone;
}

StatusCode ProResEncoder::DoProcess(HostBufferRef* p_pBuff)
{
    if (m_Error != errNone)
    {
        return m_Error;
    }
  
    if ((p_pBuff == NULL) || !p_pBuff->IsValid())
    {
        // flushing
       
        //bytes = x264_encoder_encode(m_pContext, &pNals, &numNals, 0, &outPic);
    }
    else
    {
        char* pBuf = NULL;
        size_t bufSize = 0;
        if (!p_pBuff->LockBuffer(&pBuf, &bufSize))
        {
            g_Log(logLevelError, "prores Plugin :: Failed to lock the buffer");
            return errFail;
        }

        if (pBuf == NULL || bufSize == 0)
        {
            g_Log(logLevelError, "prores Plugin :: No data to encode");
            p_pBuff->UnlockBuffer();
            return errUnsupported;
        }

        uint32_t width = 0;
        uint32_t height = 0;
        int64_t pts = 0;
        if (!p_pBuff->GetINT64(pIOPropPTS, pts))
        {
            g_Log(logLevelError, "prores Plugin :: PTS not set when encoding the frame");
            return errNoParam;
        }

        

        // repack
        std::vector<uint8_t> yPlane;
        yPlane.reserve(width * height);
        std::vector<uint8_t> uvPlane;
        uvPlane.reserve(width * height / 2);
        const uint8_t* pSrc = reinterpret_cast<uint8_t*>(const_cast<char*>(pBuf));

        for (int h = 0; h < height; ++h)
        {
            for (int w = 0; w < width; w += 2)
            {
                yPlane.push_back(pSrc[1]);
                yPlane.push_back(pSrc[3]);
                if ((h % 2) == 0)
                {
                    uvPlane.push_back(pSrc[0]);
                    uvPlane.push_back(pSrc[2]);
                }

                pSrc += 4;
            }
        }

        p_pBuff->UnlockBuffer();

        // inPic.img.i_plane = 2;
        // inPic.img.i_stride[0] = width;
        // inPic.img.plane[0] = yPlane.data();
        // inPic.img.i_stride[1] = width;
        // inPic.img.plane[1] = uvPlane.data();
        // bytes = x264_encoder_encode(m_pContext, &pNals, &numNals, &inPic, &outPic);
    }

    int bytes = 0;

    if (bytes < 0)
    {
        return errFail;
    }
    else if (bytes == 0)
    {
        return errMoreData;
    }
    else
    {
        return errNone;
    }

    // fill the out buffer and info
    HostBufferRef outBuf(false);
    if (!outBuf.IsValid() || !outBuf.Resize(bytes))
    {
        return errAlloc;
    }

    char* pOutBuf = NULL;
    size_t outBufSize = 0;
    if (!outBuf.LockBuffer(&pOutBuf, &outBufSize))
    {
        return errAlloc;
    }

    assert(outBufSize == bytes);

    // memcpy(pOutBuf, pNals[0].p_payload, bytes);

    // int64_t ts = outPic.i_pts;
    // outBuf.SetProperty(pIOPropPTS, propTypeInt64, &ts, 1);

    // ts = outPic.i_dts;
    // outBuf.SetProperty(pIOPropDTS, propTypeInt64, &ts, 1);

    // uint8_t isKeyFrame = IS_X264_TYPE_I(outPic.i_type) ? 1 : 0;
    // outBuf.SetProperty(pIOPropIsKeyFrame, propTypeUInt8, &isKeyFrame, 1);

    return m_pCallback->SendOutput(&outBuf);
}
