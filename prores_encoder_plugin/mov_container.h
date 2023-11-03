#pragma once

#include "wrapper/plugin_api.h"
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
}

using namespace IOPlugin;


class MovTrackWriter;
class MovContainer : public IPluginContainerRef
{
public:
    static const uint8_t s_UUID[];

public:
    MovContainer();

    static StatusCode s_Register(HostListRef* p_pList);

    StatusCode WriteVideo(uint32_t p_TrackIdx, HostBufferRef* p_pBuf);
    StatusCode WriteAudio(uint32_t p_TrackIdx, HostBufferRef* p_pBuf);

protected:
    // interface imp
    virtual StatusCode DoInit(HostPropertyCollectionRef* p_pProps);
    virtual StatusCode DoOpen(HostPropertyCollectionRef* p_pProps);

    virtual StatusCode DoAddTrack(HostPropertyCollectionRef* p_pProps, HostPropertyCollectionRef* p_pCodecProps, IPluginTrackBase** p_pTrack);
    virtual StatusCode DoClose();

protected:
    virtual ~MovContainer();
    std::vector<MovTrackWriter*> m_VideoTrackVec;
    std::vector<MovTrackWriter*> m_AudioTrackVec;

    AVStream* m_outStream;
    AVFormatContext* m_outFormatContext;

};