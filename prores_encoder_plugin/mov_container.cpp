#include "mov_container.h"

#include <assert.h>

#include "prores_encoder.h"
#include "prores_props.h"

using namespace IOPlugin;

// NOTE: When creating a plugin for release, please generate a new Container UUID in order to prevent conflicts with other third-party plugins.
const uint8_t MovContainer::s_UUID[] = { 0x87, 0x7b, 0x52, 0x48, 0x7a, 0x34, 0x11, 0xee, 0x86, 0x52, 0x83, 0x47, 0xc2, 0x64, 0x80, 0x3a };
const char * MovContainer::s_UUIDStr = "877b52487a3411ee86528347c264803a";



class MovTrackWriter : public IPluginTrackBase, public IPluginTrackWriter
{
public:
    MovTrackWriter(MovContainer* p_pContainer, uint32_t p_TrackIdx, bool p_IsVideo)
        : IPluginTrackBase(p_pContainer)
        , m_TrackIdx(p_TrackIdx)
        , m_IsVideo(p_IsVideo)
    {
    }
    virtual ~MovTrackWriter() = default;

public:
    virtual StatusCode DoWrite(HostBufferRef* p_pBuf)
    {
        MovContainer* pContainer = dynamic_cast<MovContainer*>(m_pContainer);
        assert(pContainer != NULL);


        return m_IsVideo ? pContainer->WriteVideo(m_TrackIdx, p_pBuf) : pContainer->WriteAudio(m_TrackIdx, p_pBuf);
    }

private:
    uint32_t m_TrackIdx;
    bool m_IsVideo;
};

StatusCode MovContainer::s_Register(HostListRef* p_pList)
{
    HostPropertyCollectionRef containerInfo;
    if (!containerInfo.IsValid())
    {
        return errAlloc;
    }

    containerInfo.SetProperty(pIOPropUUID, propTypeUInt8, MovContainer::s_UUID, 16);

    const char* pContainerName = "QuickTime (ffmpeg)";
    containerInfo.SetProperty(pIOPropName, propTypeString, pContainerName, strlen(pContainerName));

    const uint32_t mediaType = (mediaAudio | mediaVideo);
    containerInfo.SetProperty(pIOPropMediaType, propTypeUInt32, &mediaType, 1);

    // set extension, otherwise default will be used. It is required even for folder path
    containerInfo.SetProperty(pIOPropContainerExt, propTypeString, "mov", 3);

    if (!p_pList->Append(&containerInfo))
    {
        return errFail;
    }

    return errNone;
}

MovContainer::MovContainer() : m_outFormatContext(0), m_outStream(0)
{
}

MovContainer::~MovContainer()
{
}

StatusCode MovContainer::DoInit(HostPropertyCollectionRef* p_pProps)
{
    return errNone;
}

StatusCode MovContainer::DoOpen(HostPropertyCollectionRef* p_pProps)
{
    std::string path;
    p_pProps->GetString(pIOPropPath, path);
    

    g_Log(logLevelWarn, "Dummy Container Plugin :: open container with path: %s", path.c_str());

 

    return errNone;
}

StatusCode MovContainer::DoAddTrack(HostPropertyCollectionRef* p_pProps, HostPropertyCollectionRef* p_pCodecProps, IPluginTrackBase** p_pTrack)
{

    g_Log(logLevelWarn, "Dummy Container Plugin :: DoAddTrack ");

    uint32_t mediaType = 0;
    if (!p_pProps->GetUINT32(pIOPropMediaType, mediaType) || ((mediaType != mediaVideo) && (mediaType != mediaAudio)))
    {
        return errInvalidParam;
    }
    const bool isVideo = (mediaType == mediaVideo);

    // extract some params for demo purposes
    PropertyType propType;
    const void* pVal = NULL;
    int numVals = 0;

    //
    std::string name;
    p_pProps->GetString(pIOPropName, name);

    std::string reel;
    p_pProps->GetString(pIOPropReel, reel);

    // Edit rate shared between audio and video, frame rate for the clip
    uint32_t fpsNum = 0;
    uint32_t fpsDen = 0;
    if ((errNone == p_pProps->GetProperty(pIOPropFrameRate, &propType, &pVal, &numVals)) &&
        (propType == propTypeUInt32) && (pVal != NULL) && (numVals == 2))
    {
        const uint32_t* pFrameRate = static_cast<const uint32_t*>(pVal);
        fpsNum = pFrameRate[0];
        fpsDen = pFrameRate[1];
    }

    uint8_t isDropFrame = 0;
    p_pProps->GetUINT8(pIOPropFrameRateIsDrop, isDropFrame);

    double startTimeSeconds = 0.0; // start timecode, should be converted via frame rate
    p_pProps->GetDouble(pIOPropStartTime, startTimeSeconds);

    if (isVideo)
    {
        HostCodecConfigCommon trackCfg;
        trackCfg.Load(p_pProps);

        double duration = 0.0; // optional estimated duration in seconds
        p_pProps->GetDouble(pIOPropDuration, duration);

        double par = 1.0; // pixel aspect ratio
        p_pProps->GetDouble(pIOPropPAR, par);

        HostCodecConfigCommon codecCfg;
        codecCfg.Load(p_pCodecProps);

        AVCodec* codec;
        const void* pVal = NULL;
        PropertyType propType;
        int numVals = 0;
        StatusCode ret = p_pCodecProps->GetProperty(pIOPropAVCodec, &propType, &pVal, &numVals );
        if (ret != errNone) {
            g_Log(logLevelError,"Failed to get codec");
            return errFail;
        }
        uint64_t val = *reinterpret_cast<const uint64_t*>(pVal);
        codec = reinterpret_cast<AVCodec*>(val);

        AVCodecContext* codecContext;
        ret = p_pCodecProps->GetProperty(pIOPropAVCodecContext, &propType, &pVal, &numVals );
        if (ret != errNone) {
            g_Log(logLevelError,"Failed to get codec context");
            return errFail;
        }
        val = *reinterpret_cast<const uint64_t*>(pVal);
        codecContext = reinterpret_cast<AVCodecContext*>(val);


        // extract extra options from p_pCodecProps if needed such as magic cookie etc, whichever the codec has set
              // Create a new AVStream for the video

      // Create a new AVFormatContext to represent the output format
        std::string path;
        p_pProps->GetString(pIOPropPath, path);

        if (avformat_alloc_output_context2(&m_outFormatContext, nullptr, "mov", path.c_str() )  < 0) {
            g_Log(logLevelError,"Failed to create output stream");
            return errFail;
        }              

        m_outStream = avformat_new_stream(m_outFormatContext, codec);
        if (!m_outStream) {
            g_Log(logLevelError,"Failed to create new stream");
            return errFail;
        }    

        m_outStream->codecpar->codec_tag = 0;
        avcodec_parameters_from_context(m_outStream->codecpar, codecContext);


            // Open the output file
        if (avio_open(&m_outFormatContext->pb,  path.c_str() , AVIO_FLAG_WRITE) < 0) {
            g_Log(logLevelError, "Could not open output file" );
            return errFail;;
        }

        // Write the file header
        if (avformat_write_header(m_outFormatContext, nullptr) < 0) {
            g_Log(logLevelError,  "Error writing file header" );
            return errFail;
        }

        // try to find the sample x264 plugin config entry if it was set
        std::string markerColor;
        p_pCodecProps->GetString("x264_enc_markers", markerColor);
        if (!markerColor.empty())
        {
            g_Log(logLevelWarn, "Dummy Container Plugin :: Using marker color: %s", markerColor.c_str());

            // get all markers data from track props and find those matching codec settings color
            PropertyType type = propTypeNull;
            const void* pVal = NULL;
            int numVals = 0;
            const StatusCode err = p_pProps->GetProperty(pIOPropMarkersBlob, &type, &pVal, &numVals);
            if ((err == errNone) && (type == propTypeUInt8) && (numVals > 0))
            {
                HostMarkersMap markers;
                if (markers.FromBuffer(static_cast<const uint8_t*>(pVal), numVals))
                {
                    const std::map<double, HostMarkerInfo>& markersMap = markers.GetMarkersMap();
                    for (auto it = markersMap.begin(); it != markersMap.end(); ++it)
                    {
                        const HostMarkerInfo& marker = it->second;
                        if (marker.GetColor() == markerColor)
                        {
                            g_Log(logLevelWarn, "Dummy Container Plugin :: Matchind marker at %f seconds with name: %s", marker.GetPositionSeconds(), marker.GetName().c_str());
                        }
                    }
                }
            }
        }
    }
    else
    {
        uint32_t samplingRate = 0;
        p_pProps->GetUINT32(pIOPropSamplingRate, samplingRate);

        // always interleaved signed int packing, specs:
        uint32_t numChannels = 0;
        p_pProps->GetUINT32(pIOPropNumChannels, numChannels);

        uint32_t bitDepth = 0; // storage depth
        p_pProps->GetUINT32(pIOPropBitDepth, bitDepth);

        uint32_t dataBitDepth = 0; // data bit depth - less or equal to storage depth, currently shall be equal
        p_pProps->GetUINT32(pIOPropBitsPerSample, dataBitDepth);

        uint8_t isFloat = 0; // 0 - integer data, 1 - floating point data
        p_pProps->GetUINT8(pIOPropIsFloat, isFloat);

        uint32_t channelLayout = 0; // AudioChannelLayout enum value
        p_pProps->GetUINT32(pIOPropAudioChannelLayout, channelLayout);
    }

    MovTrackWriter* pTrack = new MovTrackWriter(this, isVideo ? m_VideoTrackVec.size() : m_AudioTrackVec.size(), isVideo);
    pTrack->Retain();

    *p_pTrack = pTrack;
    if (isVideo)
    {
        m_VideoTrackVec.push_back(pTrack);
    }
    else
    {
        m_AudioTrackVec.push_back(pTrack);
    }
    return errNone;
}

StatusCode MovContainer::DoClose()
{
    // release all tracks and dereferencing the container

    for (size_t i = 0; i < m_AudioTrackVec.size(); ++i)
    {
        m_AudioTrackVec[i]->Release();
    }
    m_AudioTrackVec.clear();

    for (size_t i = 0; i < m_VideoTrackVec.size(); ++i)
    {
        m_VideoTrackVec[i]->Release();
    }
    m_VideoTrackVec.clear();

    // Clean up and close the output file
    av_write_trailer(m_outFormatContext);

    avio_close(m_outFormatContext->pb);


    return errNone;
}

StatusCode MovContainer::WriteVideo(uint32_t p_TrackIdx, HostBufferRef* p_pBuf)
{
    if (p_pBuf == NULL)
    {
        // flush
        return errNone;
    }

    int64_t pts = -1LL;
    int64_t dts = -1LL;
    double duration = 0.0; // optional, may be invalid and default to 1 frame in track fps
    p_pBuf->GetINT64(pIOPropPTS, pts);
    if (!p_pBuf->GetINT64(pIOPropDTS, dts))
    {
        dts = pts;
    }
    if (!p_pBuf->GetDouble(pIOPropDuration, duration))
    {
        // duration may be double or int64, if double - in seconds, if int64_t then in time base units (or fps)
        int64_t intDuration = 0;
        p_pBuf->GetINT64(pIOPropDuration, intDuration);
    }

    char* pBuf = NULL;
    size_t bufSize = 0;
    if (p_pBuf->LockBuffer(&pBuf, &bufSize))
    {
        // put the writing code here
        g_Log(logLevelWarn, "Dummy Container Plugin :: Write Video of %ld for track %d: pts: %lld, dts: %lld, duration: %f", bufSize, p_TrackIdx, pts, dts, duration);
        // Write the encoded data to the output file
        AVPacket packet;
        av_init_packet(&packet);
        av_packet_from_data(&packet, reinterpret_cast<uint8_t*>(pBuf), bufSize);

        packet.pts = pts;
        packet.dts = dts;
        packet.flags = AV_PKT_FLAG_KEY;

        if (av_write_frame(m_outFormatContext, &packet) < 0) {
          g_Log(logLevelError, "error writing");
        }

        p_pBuf->UnlockBuffer();
    }

    return errNone;
}

StatusCode MovContainer::WriteAudio(uint32_t p_TrackIdx, HostBufferRef* p_pBuf)
{
    if (p_pBuf == NULL)
    {
        // flush
        return errNone;
    }

    int64_t pts = -1LL;
    int64_t dts = -1LL;
    int64_t duration = 0;
    p_pBuf->GetINT64(pIOPropPTS, pts);
    if (!p_pBuf->GetINT64(pIOPropDTS, dts))
    {
        dts = pts;
    }

    p_pBuf->GetINT64(pIOPropDuration, duration); // may be double in seconds or int64 in samples

    // if pIOPropTimeBase is set then pts/dts/duration in seconds will be the corresponding value multiplied by time base
    // otherwise it's the value divided by frame rate of the track

    char* pBuf = NULL;
    size_t bufSize = 0;
    if (p_pBuf->LockBuffer(&pBuf, &bufSize))
    {
        // put the writing code here
        g_Log(logLevelWarn, "Dummy Container Plugin :: Write Audio of %ld for track %d: pts: %lld, dts: %lld, duration: %d", bufSize, p_TrackIdx, pts, dts, duration);
        p_pBuf->UnlockBuffer();
    }

    return errNone;
}
