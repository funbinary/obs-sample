#pragma once
#include <string>
#include <vector>

#include "obs.h"
#include "obs.hpp"
class ObsCtl
{
public:
    ObsCtl();
    ~ObsCtl();

    bool loadVideoScene();

    bool searchVideoTarget();

    bool updateVideo();

    bool updateAudio();

    bool start();

    void loadCameraId(obs_source_t* src);

private:
    bool resetAudio();
    bool resetVideo();

private:
    OBSEncoder audioStreaming;
    OBSEncoder videoStreaming;
    OBSOutput m_output = nullptr; // 输出
    OBSOutput m_fileOutput = nullptr; // 输出
    obs_source_t* fade_transition_ = nullptr; // 声音
    OBSService m_service = nullptr;
    obs_scene_t* m_scene = nullptr; // 场景
    obs_source_t* m_captureSource = nullptr;
    obs_properties_t* m_properties = nullptr;

    OBSEncoder aac_track_[MAX_AUDIO_MIXES]; // 音频track, 比如mic+system 两路声音
    std::string aac_encoder_id_[MAX_AUDIO_MIXES]; // 每路编码器的名字

    std::vector<std::string> m_recTargets; //存储查找出来的显示器或窗口
    obs_property_t* m_property = nullptr;
    obs_data_t* m_settingSource = nullptr;

    std::vector<std::string> m_cameraID;
};
