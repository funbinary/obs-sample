#ifndef OBSIMP_H
#define OBSIMP_H
#include <iostream>
#include "obs.h"
#include "obs.hpp"
#include <vector>

using namespace std;
enum REC_VIDEO_TYPE {
    REC_VIDEO_DESKTOP = 0,  // 桌面录制
    REC_VIDEO_CAMERA,   // 摄像头录制
    REC_VIDEO_WINDOWS, // 窗口录制
    REC_VIDEO_NO,       // 不录制画面
};

enum REC_AUDIO_TYPE {
    REC_AUDIO_MIC_SYS = 0,  // 麦和系统声音
    REC_AUDIO_MIC_ONLY,   // 仅麦克风
    REC_AUDIO_SYS_ONLY, // 仅系统声音
    REC_AUDIO_NO,   // 不录制声音
};

enum REC_PUSH_TYPE {
    REC_LOCAL = 0,  // 录制本地
    REC_RTMP,   // 录制RTMP推流
			REC_WHIP,
    REC_LOCAL_RTMP, // 录制+推流
};



class OBSImp
{
public:
    OBSImp();
    virtual ~OBSImp();

    bool InitOBS();
    int Start(std::string &url, REC_PUSH_TYPE rec_push_type);
    int Stop();
    void get_camera_id(obs_source_t* src);
    // 设置录制的源
    // 视频相关的源选择
    int AddVideoScensSource(REC_VIDEO_TYPE rec_video_type);
    int SearchRecVideoTargets(REC_VIDEO_TYPE rec_video_type);


    int AddAudioSource(REC_AUDIO_TYPE rec_audio_type);

    int UpdateVideoRecItem(const char *target, REC_VIDEO_TYPE rec_video_type);
    int UpdateAudioRecItem(REC_AUDIO_TYPE rec_audio_type);

    string GetTimeSecondsString();

    vector<string> GetRecTargets() const
    {
        return vec_rec_targets_;
    }
protected:
    void recSystemAudio();	// 录制系统声音
    void recMicAudio();     // 录制麦克风声音
    int resetAudio();
    int resetVideo();
    bool createOutputMode();
    //    bool createRtmpOutput();
    int setupLocalRecord(string &file_name);// 本地录制
    int setupWhipPush(string &url);
    int setupRtmpPush(string &url);
protected:
    REC_PUSH_TYPE rec_push_type_ = REC_LOCAL;//默认本地录制
    OBSOutput file_output_ = nullptr;   // 文件输出
    OBSOutput rtmp_output_ = nullptr;   // 文件输出
    obs_service_t* service = nullptr;
    obs_output_t* streamingOutput = nullptr;
    obs_source_t * fade_transition_ = nullptr;	// 声音
    obs_scene_t* scene_ = nullptr;  // 场景
    obs_source_t* capture_source = nullptr;
    obs_properties_t* properties = nullptr;

    OBSEncoder aac_track_[MAX_AUDIO_MIXES]; // 音频track, 比如mic+system 两路声音
    std::string aac_encoder_id_[MAX_AUDIO_MIXES];   // 每路编码器的名字

    std::vector<std::string> vec_rec_targets_;  //存储查找出来的显示器或窗口
    obs_property_t* property_ = nullptr;
    obs_data_t* setting_source_ = nullptr;

    vector<std::string> camera_id_;
};



#endif // OBSIMP_H
