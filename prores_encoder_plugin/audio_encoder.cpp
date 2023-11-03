#include <memory>
#include "audio_encoder.h"
#include "mov_container.h"


// NOTE: When creating a plugin for release, please generate a new Codec UUID in order to prevent conflicts with other third-party plugins.
// ad903d5702f24ac19dde8faca3488051
const uint8_t AudioEncoder::s_UUID[] = { 0xad, 0x90, 0x3d, 0x57, 0x02, 0xf2, 0x4a, 0xc1, 0x9d, 0xde, 0x8f, 0xac, 0xa3, 0x48, 0x80, 0x51 };


class UIAudioSettingsController
{
public:
    UIAudioSettingsController()
    {
        InitDefaults();
    }

    ~UIAudioSettingsController()
    {
    }

    void Load(IPropertyProvider* p_pValues)
    {
        p_pValues->GetINT32("aud_enc_bitrate", m_BitRate);
    }

    StatusCode Render(HostListRef* p_pSettingsList)
    {
        HostUIConfigEntryRef item("aud_enc_bitrate");
        item.MakeSlider("Bit Rate", "kbps", m_BitRate, 128, 512, 128);

        item.SetTriggersUpdate(true);
        if (!item.IsSuccess() || !p_pSettingsList->Append(&item))
        {
            g_Log(logLevelError, "Audio Plugin :: Failed to populate bitrate slider UI entry");
            return errFail;
        }

        return errNone;
    }

    int32_t GetBitRate() const
    {
        return m_BitRate;
    }

private:
    void InitDefaults()
    {
        m_BitRate = 128;
    }

private:
    int32_t m_BitRate;
};

StatusCode AudioEncoder::s_RegisterCodecs(HostListRef* p_pList)
{
    // add audio encoder
    HostPropertyCollectionRef codecInfo;
    if (!codecInfo.IsValid())
    {
        return errAlloc;
    }

    codecInfo.SetProperty(pIOPropUUID, propTypeUInt8, AudioEncoder::s_UUID, 16);

    const char* pCodecName = "Audio Plugin";
    codecInfo.SetProperty(pIOPropName, propTypeString, pCodecName, strlen(pCodecName));

    uint32_t val = ' aac';
    codecInfo.SetProperty(pIOPropFourCC, propTypeUInt32, &val, 1);

    val = mediaAudio;
    codecInfo.SetProperty(pIOPropMediaType, propTypeUInt32, &val, 1);

    val = dirEncode;
    codecInfo.SetProperty(pIOPropCodecDirection, propTypeUInt32, &val, 1);

    // if need ieeefloat, set pIOPropIsFloat to 1 with bitdepth 32, supports only single bitdepth option of 32
    std::vector<uint32_t> bitDepths({16, 24});
    codecInfo.SetProperty(pIOPropBitDepth, propTypeUInt32, bitDepths.data(), bitDepths.size());
    codecInfo.SetProperty(pIOPropBitsPerSample, propTypeUInt32, &val, 1);

    // supported sampling rates, or empty
    std::vector<uint32_t> samplingRates({44100, 48000});
    codecInfo.SetProperty(pIOPropSamplingRate, propTypeUInt32, samplingRates.data(), samplingRates.size());

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

StatusCode AudioEncoder::s_GetEncoderSettings(HostPropertyCollectionRef* p_pValues, HostListRef* p_pSettingsList)
{
    UIAudioSettingsController settings;
    settings.Load(p_pValues);

    return settings.Render(p_pSettingsList);
}

AudioEncoder::AudioEncoder()
{
}

AudioEncoder::~AudioEncoder()
{
}

StatusCode AudioEncoder::DoInit(HostPropertyCollectionRef* p_pProps)
{
    uint32_t bitDepth = 0;
    p_pProps->GetUINT32(pIOPropBitDepth, bitDepth);

    uint8_t isFloat = 0;
    p_pProps->GetUINT8(pIOPropIsFloat, isFloat);

    uint32_t samplingRate = 0;
    p_pProps->GetUINT32(pIOPropSamplingRate, samplingRate);

    uint32_t numChannels = 0;
    p_pProps->GetUINT32(pIOPropNumChannels, numChannels);

    uint32_t trackIdx = 0xFFFFFFFF;
    p_pProps->GetUINT32(pIOPropTrackIdx, trackIdx);

    g_Log(logLevelWarn, "Dummy Audio Plugin :: Init Bit Depth: %d, isFloat: %d, Sampling Rate: %d, Num Channels: %d, Track Index: %d", bitDepth, isFloat, samplingRate, numChannels, trackIdx);

    return errNone;
}

void AudioEncoder::DoFlush()
{
    // Flush codec if needed
    g_Log(logLevelWarn, "Dummy Audio Plugin :: Flush");
}

StatusCode AudioEncoder::DoOpen(HostBufferRef* p_pBuff)
{
    m_pSettings.reset(new UIAudioSettingsController());
    m_pSettings->Load(p_pBuff);

    // fill bitrate for output
    if (m_pSettings->GetBitRate() > 0)
    {
        const uint64_t bitRate = static_cast<uint64_t>(m_pSettings->GetBitRate()) * 1000;
        StatusCode sts = p_pBuff->SetProperty(pIOPropBitRate, propTypeUInt32, &bitRate, 1);
        if (sts != errNone)
        {
            return sts;
        }
    }

    uint32_t bitDepth = 0;
    p_pBuff->GetUINT32(pIOPropBitDepth, bitDepth);

    uint8_t isFloat = 0;
    p_pBuff->GetUINT8(pIOPropIsFloat, isFloat);

    uint32_t samplingRate = 0;
    p_pBuff->GetUINT32(pIOPropSamplingRate, samplingRate);

    uint32_t numChannels = 0;
    p_pBuff->GetUINT32(pIOPropNumChannels, numChannels);

    uint32_t trackIdx = 0xFFFFFFFF;
    p_pBuff->GetUINT32(pIOPropTrackIdx, trackIdx);

    g_Log(logLevelWarn, "Dummy Audio Plugin :: Open Bit Depth: %d, isFloat: %d, Sampling Rate: %d, Num Channels: %d, Track Index: %d, Bit Rate: %d", bitDepth, isFloat, samplingRate, numChannels, trackIdx, m_pSettings->GetBitRate());

    // fill magic cookie here if needed
    return errNone;
}

StatusCode AudioEncoder::DoProcess(HostBufferRef* p_pBuff)
{
    if (p_pBuff != NULL)
    {
        int64_t pts = -1;
        p_pBuff->GetINT64(pIOPropPTS, pts);

        int64_t duration = 0; // in sampling rate units
        p_pBuff->GetINT64(pIOPropDuration, duration);

        char* pBuf = NULL;
        size_t bufSize = 0;
        if (p_pBuff->LockBuffer(&pBuf, &bufSize))
        {
            // put the encoding code here
            g_Log(logLevelWarn, "Dummy Audio Plugin :: Process buffer PTS: %lld, Duration: %lld, size: %ld", pts, duration, bufSize);
            p_pBuff->UnlockBuffer();
        }
    }
    else
    {
        g_Log(logLevelWarn, "Dummy Audio Plugin :: Process flush buffer");
    }

    // Passthrough the buffer to the writer to be processed there
    return IPluginCodecRef::DoProcess(p_pBuff);
}
