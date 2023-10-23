//
// Created by root on 23-10-13.
//

#include "obs_ctl.h"

#include <iostream>
#include <string>

#include "const_define.h"
using namespace std;

static void AddSource(void* _data, obs_scene_t* scene)
{
    obs_source_t* source = (obs_source_t*)_data;
    obs_scene_add(scene, source);
    obs_source_release(source);
}

static inline bool HasAudioDevices(const char* source_id)
{
    const char* output_id = source_id;
    obs_properties_t* props = obs_get_source_properties(output_id);
    size_t count = 0;

    if (!props)
        return false;

    obs_property_t* devices = obs_properties_get(props, "device_id");
    if (devices)
        count = obs_property_list_item_count(devices);

    obs_properties_destroy(props);

    return count != 0;
}

void ResetAudioDevice(const char* sourceId, const char* deviceId, const char* deviceDesc, int channel)
{
    bool disable = deviceId && strcmp(deviceId, "disabled") == 0;
    obs_source_t* source;
    obs_data_t* settings;

    source = obs_get_output_source(channel);
    if (source)
    {
        if (disable)
        {
            obs_set_output_source(channel, nullptr);
        }
        else
        {
            settings = obs_source_get_settings(source);
            const char* oldId = obs_data_get_string(settings, "device_id");
            if (strcmp(oldId, deviceId) != 0)
            {
                obs_data_set_string(settings, "device_id", deviceId);
                obs_source_update(source, settings);
            }
            obs_data_release(settings);
        }

        obs_source_release(source);
    }
    else if (!disable)
    {
        settings = obs_data_create();
        obs_data_set_string(settings, "device_id", deviceId);
        source = obs_source_create(sourceId, deviceDesc, settings, nullptr);
        obs_data_release(settings);

        obs_set_output_source(channel, source);
        obs_source_release(source);
    }
}

ObsCtl::ObsCtl()
{
    string cfg_path = "/root/.config/obs-studio/plugin_config"; // 文件所在目录

    if (!obs_initialized())
    {
        //初始化obs
        if (!obs_startup("zh-CN", cfg_path.c_str(), NULL))
        {
            throw("初始化obs失败");
        }

        obs_add_module_path("/workspace/learn/obs-sample/output/plugins", "/usr/local/share/obs/obs-plugin/%module%");
        obs_load_all_modules();
    }

    if (!resetAudio())
    {
        throw("初始化音频失败");
    }
    if (resetVideo() != OBS_VIDEO_SUCCESS)
    {
        throw("初始化视频失败");
    }
    // init service
    OBSDataAutoRelease whipsettings = obs_data_create();
    obs_data_set_string(whipsettings, "server", "http://192.168.3.247:9060/index/api/whip?app=live&stream=test");
    m_service = obs_service_create("whip_custom", "temp_service", whipsettings, nullptr);
    obs_service_release(m_service);

    //  清除通道
    for (int i = 0; i < MAX_CHANNELS; i++)
        obs_set_output_source(i, nullptr);

    m_scene = obs_scene_create("MyScene"); // 创建场景
    if (!m_scene)
    {
        throw("create scene failed");
    }

    size_t idx = 0;
    const char* id;

    /* automatically add transitions that have no configuration (things
   * such as cut/fade/etc) */
    while (obs_enum_transition_types(idx++, &id))
    {
        const char* name = obs_source_get_display_name(id);

        if (!obs_is_source_configurable(id))
        {
            auto tr = obs_source_create_private(id, name, NULL);
            cout << __FUNCTION__ << " tr:" << id << endl;
            if (strcmp(id, "cut_transition") == 0) // 查找淡出过渡
                fade_transition_ = tr;
        }
    }
    if (!fade_transition_)
    {
        throw("no found  fade transition");
    }
    obs_set_output_source(SOURCE_CHANNEL_TRANSITION, fade_transition_); // 将淡出过度和CHANNEL_TRANSITION 绑定
    obs_source_release(fade_transition_); // obs_set_output_source内部已经做了obs_source_addref(source);

    auto s = obs_get_output_source(SOURCE_CHANNEL_TRANSITION); // 获取fade_transition
    if (!s)
    {
        cout << "??" << endl;
    }

    obs_set_output_source(SOURCE_CHANNEL_TRANSITION, s);
    obs_transition_set(s, obs_scene_get_source(m_scene)); // 将fade_transition 和MyScene绑定
    //    obs_source_release(s);

    //    if (obs_audio_monitoring_available())
    //    {
    //        obs_set_audio_monitoring_device("默认", "default");
    //    }

    videoStreaming = obs_video_encoder_create("obs_x264", "simple_video_stream", nullptr, nullptr);
    if (!videoStreaming)
        throw "Failed to create video streaming encoder (simple output)";
    obs_encoder_release(videoStreaming);

    audioStreaming = obs_audio_encoder_create("ffmpeg_opus", "simple_opus", nullptr, 0, nullptr);

    if (audioStreaming)
    {
        obs_encoder_release(audioStreaming);
    }

    // conbine signal
    //    streamDelayStarting.Connect(obs_output_get_signal_handler(streamOutput), "starting", OBSStreamStarting, this);
    //    streamStopping.Connect(obs_output_get_signal_handler(streamOutput), "stopping", OBSStreamStopping, this);
    //    startStreaming.Connect(obs_output_get_signal_handler(streamOutput), "start", OBSStartStreaming, this);
    //    stopStreaming.Connect(obs_output_get_signal_handler(streamOutput), "stop", OBSStopStreaming, this);

    //    for (int i = 0; i < MAX_AUDIO_MIXES; i++)
    //    {
    //        char name[9];
    //        sprintf(name, "adv_aac%d", i);
    //
    //        if (!CreateAACEncoder(aac_track_[i], aac_encoder_id_[i], name,
    //                              i)) // 创建aac编码器
    //        {
    //            return false;
    //        }
    //
    //        obs_encoder_set_audio(aac_track_[i],
    //                              obs_get_audio()); // 绑定相应的音频编码器
    //    }
}

ObsCtl::~ObsCtl()
{
    obs_output_force_stop(m_output);
    //    obs_output_force_stop(m_fileOutput);
}

bool ObsCtl::loadVideoScene()
{
    m_captureSource = obs_source_create("v4l2_input", "source1", NULL, nullptr);
    if (m_captureSource)
    {
        obs_scene_atomic_update(m_scene, AddSource, m_captureSource); // 将m_captureSource和MyScene绑定
        loadCameraId(m_captureSource);
    }
    else
    {
        return false;
    }
    // 设置窗口捕获原的窗口或显示器
    m_settingSource = obs_data_create(); // 创建一个新的setting data
    // 获取m_captureSource的配置数据
    obs_data_t* curSetting = obs_source_get_settings(m_captureSource);
    // 将m_captureSource的数据拷贝到setting_source_ 以保留
    obs_data_apply(m_settingSource, curSetting); // 保存比如显示源的设置
    obs_data_release(curSetting);
    bool has_mic_audio = HasAudioDevices("alsa_input_capture");
    if (has_mic_audio)
        ResetAudioDevice("alsa_input_capture", "default", "Default Desktop Audio", SOURCE_CHANNEL_AUDIO_OUTPUT);
    // deviceID,目前是自己获取然后写死的，可写函数获取
    //    const char* deviceID = "";
    //    if (!CameraID.empty())
    //    {
    //        deviceID = CameraID[0].c_str();
    //    }
    cout << "deviceID:" << m_cameraID[0] << endl;
    obs_data_set_string(m_settingSource, "device_id", "/dev/video2");

    obs_data_set_int(m_settingSource, "input", 0);
    obs_data_set_int(m_settingSource, "standard", -1);
    obs_data_set_int(m_settingSource, "dv_timing", -1);
    obs_data_set_int(m_settingSource, "resolution", -1);
    obs_data_set_int(m_settingSource, "framerate", -1);
    obs_data_set_int(m_settingSource, "color_range", 0);
    obs_data_set_bool(m_settingSource, "auto_reset", false);
    obs_data_set_int(m_settingSource, "timeout_frames", 5);
    obs_source_update(m_captureSource, m_settingSource);

    m_properties = obs_source_properties(m_captureSource); // 获取属性， obs_properties_t通过链表的方式组织
    m_property = obs_properties_first(m_properties); // 第一个属性，后续可以根据他遍历后面的属性
    //    obs_output_set_mixer(m_output, 1); //混流器
    auto a = obs_get_audio();
    //设置音频、视频的数据源？
    //    obs_output_set_media(m_output, obs_get_video(), obs_get_audio());

    //    obs_data_t* settings = obs_data_create(); // 输出配置
    //
    //    obs_data_set_string(settings, "url", "1.mp4");
    //    obs_data_set_string(settings, "format_name", RECORD_OUTPUT_FORMAT);
    //    obs_data_set_string(settings, "format_mime_type", RECORD_OUTPUT_FORMAT_MIME);
    //    obs_data_set_string(settings, "muxer_settings", "movflags=faststart");
    //    obs_data_set_int(settings, "gop_size", VIDEO_FPS * 10);
    //    obs_data_set_string(settings, "video_encoder", VIDEO_ENCODER_NAME);
    //    obs_data_set_int(settings, "video_encoder_id", VIDEO_ENCODER_ID);
    //
    //    obs_data_set_string(settings, "video_settings", "profile=main x264-params=crf=22");
    //
    //    obs_data_set_int(settings, "audio_bitrate", AUDIO_BITRATE);
    //    obs_data_set_string(settings, "audio_encoder", "aac");
    //    obs_data_set_int(settings, "audio_encoder_id", AV_CODEC_ID_AAC);
    //    obs_data_set_string(settings, "audio_settings", NULL);
    //
    //    obs_data_set_int(settings, "scale_width", BASE_WIDTH); // 影响编码参数
    //    obs_data_set_int(settings, "scale_height", BASE_HEIGHT); // 最终视频编码的宽高

    //    obs_output_set_mixer(m_fileOutput, 1); //混流器

    //设置音频、视频的数据源？
    //    obs_output_set_media(m_fileOutput, obs_get_video(), obs_get_audio());
    //    obs_output_update(m_fileOutput, settings);
    // output
    OBSDataAutoRelease videoSettings = obs_data_create();
    OBSDataAutoRelease audioSettings = obs_data_create();

    obs_data_set_string(videoSettings, "preset", "veryfast");

    obs_data_set_string(videoSettings, "rate_control", "CBR");
    obs_data_set_int(videoSettings, "bitrate", 2500);
    obs_data_set_string(audioSettings, "rate_control", "CBR");
    obs_data_set_int(audioSettings, "bitrate", 160);
    obs_service_apply_encoder_settings(m_service, videoSettings, audioSettings);
    obs_encoder_update(videoStreaming, videoSettings);
    obs_encoder_update(audioStreaming, audioSettings);
    obs_encoder_set_video(videoStreaming, obs_get_video());
    obs_encoder_set_audio(audioStreaming, obs_get_audio());
    //    if (!m_fileOutput)
    //    {
    //        //高级输出 ffmpeg
    //        m_fileOutput = obs_output_create("ffmpeg_muxer", "simple_ffmpeg_output", nullptr, nullptr);
    //        if (!m_fileOutput)
    //        {
    //            throw "Failed to create ffmpeg custom service";
    //        }
    //    }
    if (!m_output)
    {
        //高级输出 ffmpeg
        m_output = obs_output_create("whip_output", "simple_stream", nullptr, nullptr);

        //        m_output = obs_service_create("whip_custom", "default_service", settings, nullptr);
        if (!m_output)
        {
            throw "Failed to create whip custom service";
        }
    }

    obs_output_set_video_encoder(m_output, videoStreaming);
    obs_output_set_audio_encoder(m_output, audioStreaming, 0);
    obs_output_set_service(m_output, m_service);

    OBSDataAutoRelease outputsetings = obs_data_create();
    obs_data_set_string(outputsetings, "bind_ip", "default");
    obs_data_set_string(outputsetings, "ip_family", "IPv4+IPv6");
#ifdef _WIN32
    obs_data_set_bool(settings, "new_socket_loop_enabled", enableNewSocketLoop);
    obs_data_set_bool(settings, "low_latency_mode_enabled", enableLowLatencyMode);
#endif
    obs_data_set_bool(outputsetings, "dyn_bitrate", false);
    obs_output_update(m_output, outputsetings);

    obs_output_set_delay(m_output, 0, OBS_OUTPUT_DELAY_PRESERVE);

    obs_output_set_reconnect_settings(m_output, 25, 2);

    return 0;
}

void ObsCtl::loadCameraId(obs_source_t* src)
{
    /*get properties*/
    obs_properties_t* ppts = obs_source_properties(src);
    obs_property_t* property = obs_properties_first(ppts);
    // bool hasNoProperties = !property;
    while (property)
    {
        const char* name = obs_property_name(property);
        // obs_property_type type = obs_property_get_type(property);
        if (strcmp(name, "device_id") == 0)
        {
            size_t count = obs_property_list_item_count(property);
            for (size_t i = 0; i < count; i++)
            {
                const char* str = obs_property_list_item_string(property, i);
                cout << "camera:" << str << endl;
                m_cameraID.push_back(str);
            }
        }

        obs_property_next(&property);
    }
}

bool ObsCtl::searchVideoTarget()
{
    m_recTargets.clear();

    const char* rec_type_name = "device_id";

    while (m_property)
    {
        const char* name = obs_property_name(m_property);
        cout << "rec_type_name:" << rec_type_name << ", name: " << name << endl;
        if (strcmp(name, rec_type_name) == 0)
        {
            size_t count = obs_property_list_item_count(m_property);
            const char* string = nullptr;

            for (size_t i = 0; i < count; i++)
            {
                const char* item_name = obs_property_list_item_name(m_property, i);
                string = item_name;
                const char* str = obs_property_list_item_string(m_property, i);
                cout << "list -> " << str << " : " << string << endl;

                m_recTargets.push_back(string);
            }
        }

        obs_property_next(&m_property);
    }
    return 0;
}

bool ObsCtl::updateVideo()
{
    return true;
    //    for (auto item : m_recTargets)
    //    {
    //        if (item == std::string(target)) //
    //        {
    //            obs_data_set_string(m_settingSource, "device_id", target.c_str()); // 设置源
    //
    //            obs_source_update(m_captureSource, m_settingSource);
    //            break;
    //        }
    //    }
    //
    //    obs_data_release(m_settingSource);

    return 0;
}
bool ObsCtl::updateAudio()
{
    //    int ret = 0;
    //    switch (rec_audio_type)
    //    {
    //    case REC_AUDIO_MIC_SYS: // 录制麦克风+系统声音
    //        recMicAudio();
    //        recSystemAudio();
    //        break;
    //    case REC_AUDIO_MIC_ONLY: // 录制麦克风
    //        recMicAudio();
    //        break;
    //    case REC_AUDIO_SYS_ONLY: // 录制系统声音
    //        recSystemAudio();
    //        break;
    //    default:
    //        break;
    //    }

    return true;
}

bool ObsCtl::start()
{
    if (!obs_output_start(m_output))
    {
        const char* error = obs_output_get_last_error(m_output);
        cout << "StartRec err:" << error << endl;
        return -1;
    }
    //    if (!obs_output_start(m_fileOutput))
    //    {
    //        const char* error = obs_output_get_last_error(m_fileOutput);
    //        cout << "StartRec err:" << error << endl;
    //        return -1;
    //    }
    return 0;
}

bool ObsCtl::resetAudio()
{
    struct obs_audio_info2 ai;
    ai.samples_per_sec = 48000; // 采样率
    ai.speakers = SPEAKERS_STEREO;
    int ret = obs_reset_audio2(&ai);
    cout << " obs_reset_audio:" << ret << endl;
    return ret;
}

bool ObsCtl::resetVideo()
{
    struct obs_video_info ovi;
    memset(&ovi, 0, sizeof(struct obs_video_info));
    ovi.fps_num = 30;
    ovi.fps_den = 1;
    ovi.graphics_module = "libobs-opengl.so.1";
    ovi.base_width = 1920; // 视频源的分辨率区域大小
    ovi.base_height = 1080;
    ovi.output_width = BASE_WIDTH;
    ovi.output_height = BASE_HEIGHT;
    ovi.output_format = VIDEO_FORMAT_NV12; // 视频输出格式YUV420P
    ovi.colorspace = VIDEO_CS_709;
    ovi.range = VIDEO_RANGE_PARTIAL;

    ovi.adapter = 0;
    ovi.gpu_conversion = true;
    ovi.scale_type = OBS_SCALE_BICUBIC;

    int ret = obs_reset_video(&ovi);
    cout << "obs_reset_video: " << ret << endl;
    obs_set_video_levels(300, 1000);
    return ret;
}
