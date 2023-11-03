#pragma once

#include <memory>


extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
}

#include "wrapper/plugin_api.h"




using namespace IOPlugin;

//fwd decl
struct x264_t;
struct x264_param_t;
class UISettingsController;


class ProResEncoder : public IPluginCodecRef
{
public:
    static const uint8_t s_UUID[];
  
public:
    ProResEncoder();
    ~ProResEncoder();

    static StatusCode s_RegisterCodecs(HostListRef* p_pList);
    static StatusCode s_GetEncoderSettings(HostPropertyCollectionRef* p_pValues, HostListRef* p_pSettingsList);

protected:
    virtual void DoFlush() override;
    virtual StatusCode DoInit(HostPropertyCollectionRef* p_pProps) override;
    virtual StatusCode DoOpen(HostBufferRef* p_pBuff) override;
    virtual StatusCode DoProcess(HostBufferRef* p_pBuff) override;

private:
    void OpenAV();
    void CloseAV();

private:

    AVCodec* m_codec;
    AVCodecContext* m_codecContext;
    AVFrame* m_frame;
    AVPacket* m_packet;


    std::unique_ptr<UISettingsController> m_pSettings;
    HostCodecConfigCommon m_CommonProps;

    StatusCode m_Error;
};
