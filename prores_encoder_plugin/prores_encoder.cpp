#include "prores_encoder.h"
#include "mov_container.h"
#include <assert.h>
#include <cstring>
#include <vector>
#include <stdint.h>

#include <algorithm>
#include <thread>
#include "prores_props.h"


#include "x264.h"

const uint8_t ProResEncoder::s_UUID[] = { 0x71, 0x40, 0x3b, 0xa6, 0x7a, 0x34, 0x11, 0xee, 0x8c, 0xf8, 0x7f, 0x2a, 0x35, 0xe2, 0x8b, 0x49 };

static const char * const prores_profile_names[] = { "422 Proxy", "422 LT", "422", "422 HQ", "4444", "4444 XQ", 0 };


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

        p_pValues->GetINT32("prores_profile", m_Profile);
        //p_pValues->GetINT32("x264_bitrate", m_BitRate);
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
        m_Profile = 2;
        //m_BitRate = 0;
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

        // Profile combobox
        {
            HostUIConfigEntryRef item("prores_profile");

            std::vector<std::string> textsVec;
            std::vector<int> valuesVec;

            int count = 0;
            while( prores_profile_names[count] != 0 )
            {
                textsVec.push_back(prores_profile_names[count]);
                valuesVec.push_back(count);
                count++;
            }

            item.MakeComboBox("profile", textsVec, valuesVec, m_Profile);
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

        // {
        //     HostUIConfigEntryRef item("x264_bitrate");
        //     item.MakeSlider("Bit Rate", "KBps", m_BitRate, 100, 3000, 1);


        //     if (!item.IsSuccess() || !p_pSettingsList->Append(&item))
        //     {
        //         g_Log(logLevelError, "X264 Plugin :: Failed to populate bitrate slider UI entry");
        //         return errFail;
        //     }
        // }

        return errNone;
    }

public:

    int32_t GetProfile() const
    {
        return m_Profile;
    }

    // int32_t GetBitRate() const
    // {
    //     return m_BitRate * 8;
    // }



private:
    HostCodecConfigCommon m_CommonProps;
    int32_t m_Profile;
    //int32_t m_BitRate;
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

    const char* pCodecName = "ProRes";
    codecInfo.SetProperty(pIOPropName, propTypeString, pCodecName, strlen(pCodecName));

    const char* pCodecGroup = "ProRes";
    codecInfo.SetProperty(pIOPropGroup, propTypeString, pCodecGroup, strlen(pCodecGroup));

    uint32_t val = 'avc1';
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

    // uint8_t val8 = 0;
    // codecInfo.SetProperty(pIOPropThreadSafe, propTypeUInt8, &val8, 1 );

    // fill supported containers, would one need the plugin container to handle the encoding internally,
    // just create a dummy passthrough codec which will pass the buffer for output unchanged
    // but if nothing extraordinary is required let Resolve trigger the codec encode function and pass the output buffer to the writer
    std::vector<std::string> containerVec;
    containerVec.push_back( MovContainer::s_UUIDStr );
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
    : m_codec(0)
    , m_codecContext(0)
    , m_frame(0)
    , m_packet(0)
    , m_Error(errNone)
{


}

ProResEncoder::~ProResEncoder()
{
    g_Log(logLevelError, "X264 Plugin :: Destructor");
    CloseAV();
}

void ProResEncoder::DoFlush()
    
{
  g_Log(logLevelInfo, "X264 Plugin :: DoFlush");

  CloseAV();



    if (m_Error != errNone)
    {
        return;
    }

    StatusCode sts = DoProcess(NULL);
    while (sts == errNone)
    {
        sts = DoProcess(NULL);
    }


    return;
}

StatusCode ProResEncoder::DoInit(HostPropertyCollectionRef* p_pProps)
{
    // fill average frame size if have byte rate
    g_Log(logLevelInfo, "X264 Plugin :: DoInit");
    uint32_t val = clrUYVY;
    p_pProps->SetProperty(pIOPropColorModel, propTypeUInt32, &val, 1);

    uint8_t hSampling = 2;
    uint8_t vSampling = 1;
    p_pProps->SetProperty(pIOPropHSubsampling, propTypeUInt8, &hSampling, 1);
    p_pProps->SetProperty(pIOPropVSubsampling, propTypeUInt8, &vSampling, 1);

    val = 'apch';
    p_pProps->SetProperty(pIOPropFourCC, propTypeUInt32, &val, 1);

    
    return errNone;
}
void ProResEncoder::OpenAV()
{ 
    g_Log(logLevelInfo, "OpenAV");
    av_register_all();

    // Initialize FFmpeg codecs and formats
    avcodec_register_all();

    int width = m_CommonProps.GetWidth();
    int height = m_CommonProps.GetHeight();

    // Find the ProRes codec
    m_codec = avcodec_find_encoder(AV_CODEC_ID_PRORES);
    if (!m_codec) {
         g_Log(logLevelError,"ProRes codec not found" );
        return;
    }

    // Initialize the codec context
    m_codecContext = avcodec_alloc_context3(m_codec);
    if (!m_codecContext) {
         g_Log(logLevelError, "Failed to allocate codec context");
        return;
    }

    g_Log(logLevelInfo, "image %dx%d", width, height);

    // Set codec parameters (e.g., width, height, bitrate, etc.)

    m_codecContext->width = width;
    m_codecContext->height = height;
    m_codecContext->profile = m_profile;
    m_codecContext->codec_id = AV_CODEC_ID_PRORES;
    m_codecContext->codec_type = AVMEDIA_TYPE_VIDEO;
    m_codecContext->pix_fmt = AV_PIX_FMT_YUV422P10;
    m_codecContext->thread_count = std::thread::hardware_concurrency();
    m_codecContext->framerate.num = m_CommonProps.GetFrameRateNum();
    m_codecContext->framerate.den = m_CommonProps.GetFrameRateDen();
    m_codecContext->time_base.num =  m_codecContext->framerate.den;
    m_codecContext->time_base.den =  m_codecContext->framerate.num;
    

    if (avcodec_open2(m_codecContext, m_codec, nullptr) < 0) {
        g_Log(logLevelError, "Could not open codec");
        return;
    }

    g_Log(logLevelInfo, "OpenAV complete");

}

void ProResEncoder::CloseAV()
{
    // Clean up and close the output file
    if ( m_codecContext ) {
      g_Log(logLevelInfo, "X264 Plugin :: CloseAV" );

      avcodec_free_context(&m_codecContext);
      m_codecContext = NULL;
    }
}

StatusCode ProResEncoder::DoOpen(HostBufferRef* p_pBuff)
{
    g_Log(logLevelInfo, "X264 Plugin :: DoOpen");
    
    m_CommonProps.Load(p_pBuff);

    m_pSettings.reset(new UISettingsController(m_CommonProps));
    m_pSettings->Load(p_pBuff);
    m_profile = m_pSettings->GetProfile();

    OpenAV();

    
    uint64_t val = reinterpret_cast<uint64_t>(m_codec);
    StatusCode res = p_pBuff->SetProperty( pIOPropAVCodec, propTypeUInt64, reinterpret_cast<const void*>(&val), 1 );    
    if (res != errNone)
    {
        g_Log(logLevelError,"Failed to set codec" );
        return res;
    }

    val = reinterpret_cast<uint64_t>(m_codecContext);
    res = p_pBuff->SetProperty( pIOPropAVCodecContext, propTypeUInt64, reinterpret_cast<const void*>(&val), 1 );    
    if (res != errNone)
    {
        g_Log(logLevelError,"Failed to set codec context" );
        return res;
    }


    return errNone;
}

StatusCode ProResEncoder::DoProcess(HostBufferRef* p_pBuff)
{

    if (m_Error != errNone)
    {
        return m_Error;
    }

    if ( p_pBuff == NULL) 
    {
        return errMoreData;
    }

    int bytes = 0;
    int64_t pts = -1;
    if ((p_pBuff == NULL) || !p_pBuff->IsValid())
    {
        // flushing
        g_Log(logLevelError, "X264 Plugin :: FLUSHING");
        CloseAV();
    }
    else
    {
        char* pBuf = NULL;
        size_t bufSize = 0;
        if (!p_pBuff->LockBuffer(&pBuf, &bufSize))
        {
            g_Log(logLevelError, "X264 Plugin :: Failed to lock the buffer");
            return errFail;
        }

        if (pBuf == NULL || bufSize == 0)
        {
            g_Log(logLevelError, "X264 Plugin :: No data to encode");
            p_pBuff->UnlockBuffer();
            return errUnsupported;
        }

        uint32_t width = 0;
        uint32_t height = 0;
        if (!p_pBuff->GetUINT32(pIOPropWidth, width) || !p_pBuff->GetUINT32(pIOPropHeight, height))
        {
            g_Log(logLevelError, "X264 Plugin :: Width/Height not set when encoding the frame");
            return errNoParam;
        }

        if (!p_pBuff->GetINT64(pIOPropPTS, pts))
        {
            g_Log(logLevelError, "X264 Plugin :: PTS not set when encoding the frame");
            return errNoParam;
        }

        //g_Log(logLevelInfo, "X264 Plugin :: PTS %ld", pts );


        // Initialize a packet for holding the encoded data
        AVPacket packet;
        av_init_packet(&packet);
        packet.data = nullptr;
        packet.size = 0;
        
        // Create a frame for encoding
        AVFrame* frame = av_frame_alloc();
        if (!frame) 
        {
          g_Log(logLevelError, "Could not allocate frame" );
          return errFail;
        }

          // Initialize the frame parameters
        float framerate = (float)m_codecContext->framerate.num / (float)m_codecContext->framerate.den;
        frame->format = AV_PIX_FMT_YUV422P10;
        frame->width =  m_codecContext->width;
        frame->height =  m_codecContext->height;            
        frame->pts = int64_t(pts * (90000./ framerate) );


        //g_Log(logLevelError, "PTS %ld", frame->pts );


          // Allocate memory for the frame data
        if (av_frame_get_buffer(frame, 0) < 0) {
          g_Log(logLevelError, "Could not allocate frame data" );
          return errFail;
        }

        uint8_t* pSrc = reinterpret_cast<uint8_t*>(const_cast<char*>(pBuf));


        for (int y = 0; y < height; ++y)
        {
            uint16_t* row = (uint16_t*)(frame->data[0] + y * frame->linesize[0]);
            uint16_t* rowU = (uint16_t*)(frame->data[1] + y * frame->linesize[1]);
            uint16_t* rowV = (uint16_t*)(frame->data[2] + y * frame->linesize[2]);

            for (int x = 0; x < width; x += 2)
            {
                // scale 8 bit to 10 bit
                row[x] = pSrc[1] << 2;
                row[x + 1] = pSrc[3] << 2;
                rowU[x/2] = pSrc[0] << 2;
                rowV[x/2] = pSrc[2] << 2;
                pSrc += 4;
            }
        }

        p_pBuff->UnlockBuffer();

              // Encode the frame
        int ret = avcodec_send_frame(m_codecContext, frame);
        if (ret < 0) {
          g_Log(logLevelError, "error sending");
        }
        while (ret >= 0) {
            ret = avcodec_receive_packet(m_codecContext, &packet);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
              break;
            } else if (ret < 0) {
              g_Log(logLevelError, "error encoding");
            }

            // write packet to output buffer
            HostBufferRef outBuf(false);
            bytes = packet.size;

            if (bytes < 0)
            {
            return errFail;
            }
            else if (bytes == 0)
            {
            return errMoreData;
            }
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


            memcpy(pOutBuf, packet.data, bytes );

            int64_t packet_pts = packet.pts;
            int64_t packet_dts = packet.dts;

            outBuf.SetProperty(pIOPropPTS, propTypeInt64, &packet_pts , 1);
            outBuf.SetProperty(pIOPropDTS, propTypeInt64, &packet_dts , 1);
            m_pCallback->SendOutput(&outBuf);
            outBuf.UnlockBuffer();

            av_packet_unref(&packet);
          }
      av_frame_free(&frame);
  }

  return errNone;

}
