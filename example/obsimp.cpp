#include "obsimp.h"

#include "libavcodec/avcodec.h"

#define INPUT_AUDIO_SOURCE "alsa_input_capture"
#define OUTPUT_AUDIO_SOURCE "alsa_input_capture"

#define VIDEO_ENCODER_ID AV_CODEC_ID_H264
#define VIDEO_ENCODER_NAME "libx264"
#define RECORD_OUTPUT_FORMAT "mp4"
#define RECORD_OUTPUT_FORMAT_MIME "video/mp4"
#define VIDEO_FPS 25
#define AUDIO_BITRATE 128
#define VIDEO_BITRATE 2000
#define BASE_WIDTH 1920
#define BASE_HEIGHT 1080

//目前默认配置了6 个通道
enum SOURCE_CHANNELS
{
    SOURCE_CHANNEL_TRANSITION = 0,
    SOURCE_CHANNEL_AUDIO_OUTPUT, // 1
    SOURCE_CHANNEL_AUDIO_OUTPUT_2, //2
    SOURCE_CHANNEL_AUDIO_INPUT, //3
    SOURCE_CHANNEL_AUDIO_INPUT_2, //4
    SOURCE_CHANNEL_AUDIO_INPUT_3, //5
};

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
    {
        return false;
    }
    obs_property_t* devices = obs_properties_get(props, "device_id");
    if (devices)
    {
        count = obs_property_list_item_count(devices);
    }
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

static bool CreateAACEncoder(OBSEncoder& res, string& id, const char* name, size_t idx)
{
    cout << id << endl;
    const char* id_ = "ffmpeg_aac";
    res = obs_audio_encoder_create(id_, name, nullptr, idx, nullptr);
    if (res)
    {
        obs_encoder_release(res);
        return true;
    }
    return false;
}
OBSImp::OBSImp() {}

OBSImp::~OBSImp() {}

bool OBSImp::InitOBS()
{
    string cfg_path = "/root/.config/obs-studio/plugin_config"; // 文件所在目录

    if (!obs_initialized())
    {
        //初始化obs
        if (!obs_startup("zh-CN", cfg_path.c_str(), NULL))
        {
            throw("初始化obs失败");
        }
	cout << "=========" << endl;
        obs_add_module_path("/workspace/learn/obs/aplugins/tmp2/", "/usr/local/share/obs/obs-plugins/%module%");
        obs_load_all_modules();
    }
    //音频设置
    if (!resetAudio())
    {
        return false;
    }
    //视频设置
    if (resetVideo() != OBS_VIDEO_SUCCESS)
    {
        return false;
    }
    if (!createOutputMode())
    {
        return false;
    }
    return true;
}

int OBSImp::Start(string& url, REC_PUSH_TYPE rec_push_type)
{
    int ret = 0;
    rec_push_type_ = rec_push_type;
    if (REC_LOCAL == rec_push_type_)
    {
        ret = setupLocalRecord(url);
    }
    else if (REC_RTMP == rec_push_type_)
    {
        ret = setupRtmpPush(url);
    }else if (REC_WHIP == rec_push_type_) {
	ret = setupWhipPush(url);
    }
    else
    {
        ret = -1;
    }
    return ret;
}

int OBSImp::Stop()
{
    if (REC_LOCAL == rec_push_type_)
    {
        obs_output_stop(file_output_);
    }
    else if (REC_RTMP == rec_push_type_)
    {
        obs_output_stop(streamingOutput);
    }
    return 0;
}

void OBSImp::get_camera_id(obs_source_t* src)
{
    /*get properties*/
    obs_properties_t* ppts = obs_source_properties(src);
    obs_property_t* property = obs_properties_first(ppts);
    //bool hasNoProperties = !property;
    while (property)
    {
        const char* name = obs_property_name(property);
        //obs_property_type type = obs_property_get_type(property);
        if (strcmp(name, "device_id") == 0)
        {
            size_t count = obs_property_list_item_count(property);
            for (size_t i = 0; i < count; i++)
            {
                const char* str = obs_property_list_item_string(property, i);
                cout << "camera:" << str << endl;
                camera_id_.push_back(str);
            }
        }
        obs_property_next(&property);
    }
    cout << "get_camera_id" << endl;
}

int OBSImp::AddVideoScensSource(REC_VIDEO_TYPE rec_video_type)
{
    // 设置有上面用？
    obs_set_output_source(SOURCE_CHANNEL_TRANSITION, nullptr); // 设置源绑定channel，这里先清空
    obs_set_output_source(SOURCE_CHANNEL_AUDIO_OUTPUT, nullptr);
    obs_set_output_source(SOURCE_CHANNEL_AUDIO_INPUT, nullptr);
    size_t idx = 0;
    const char* id;
    /* automatically add transitions that have no configuration (things
     * such as cut/fade/etc) */
    while (obs_enum_transition_types(idx++, &id))
    {
        const char* name = obs_source_get_display_name(id);
        if (!obs_is_source_configurable(id))
        {
            obs_source_t* tr = obs_source_create_private(id, name, NULL);
            cout << __FUNCTION__ << " tr:" << id << endl;
            if (strcmp(id, "fade_transition") == 0)
            { // 查找淡出过渡
                fade_transition_ = tr;
            }
        }
    }
    if (!fade_transition_)
    {
        return -1;
    }
    //  引用计数机制1， obs_set_output_source->计数为2
    obs_set_output_source(SOURCE_CHANNEL_TRANSITION, fade_transition_); // 将淡出过度和CHANNEL_TRANSITION 绑定
    //    obs_source_release 1
    obs_source_release(fade_transition_); // obs_set_output_source内部已经做了obs_source_addref(source);
    scene_ = obs_scene_create("MyScene"); // 创建场景
    if (!scene_)
    {
        return -2;
    }
    obs_source_t* s = obs_get_output_source(SOURCE_CHANNEL_TRANSITION); // 获取fade_transition
    obs_transition_set(s, obs_scene_get_source(scene_)); // 将fade_transition 和MyScene绑定
    obs_source_release(s);
    //创建源：显示器采集

    capture_source = obs_source_create("v4l2_input", "source1", NULL, nullptr);

    if (capture_source)
    {
        obs_scene_atomic_update(scene_, AddSource, capture_source); // 将capture_source和MyScene绑定
        if (rec_video_type == REC_VIDEO_CAMERA)
        {
            get_camera_id(capture_source);
        }
    }
    else
    {
        return -3;
    }
    // 设置窗口捕获原的窗口或显示器
    setting_source_ = obs_data_create(); // 创建一个新的setting data
    // 获取capture_source的配置数据
    obs_data_t* curSetting = obs_source_get_settings(capture_source);
    // 将capture_source的数据拷贝到setting_source_ 以保留
    obs_data_apply(setting_source_, curSetting); // 保存比如显示源的设置
    obs_data_release(curSetting);
    if (rec_video_type == REC_VIDEO_CAMERA)
    {
        //deviceID,目前是自己获取然后写死的，可写函数获取
        const char* deviceID = "";
        if (!camera_id_.empty())
        {
            deviceID = camera_id_[0].c_str();
        }
        cout << "deviceID:" << deviceID << endl;
        obs_data_set_string(setting_source_, "device_id", "/dev/video2");

        //        obs_data_set_int(setting_source_, "input", 0);
        //        obs_data_set_int(setting_source_, "standard", -1);
        //        obs_data_set_int(setting_source_, "dv_timing", -1);
        //        obs_data_set_int(setting_source_, "resolution", -1);
        //        obs_data_set_int(setting_source_, "framerate", 30);
        //        obs_data_set_int(setting_source_, "color_range", 0);
        //        obs_data_set_bool(setting_source_, "auto_reset", false);
        //        obs_data_set_int(setting_source_, "timeout_frames", 5);
        obs_source_update(capture_source, setting_source_);
    }
    properties = obs_source_properties(capture_source); // 获取属性， obs_properties_t通过链表的方式组织
    property_ = obs_properties_first(properties); // 第一个属性，后续可以根据他遍历后面的属性
    return 0;
}

int OBSImp::SearchRecVideoTargets(REC_VIDEO_TYPE rec_video_type)
{
    vec_rec_targets_.clear();
    const char* rec_type_name = "device_id";

    while (property_)
    {
        const char* name = obs_property_name(property_);
        cout << "rec_type_name:" << rec_type_name << ", name: " << name << endl;
        if (strcmp(name, rec_type_name) == 0)
        {
            size_t count = obs_property_list_item_count(property_);
            const char* string = nullptr;
            for (size_t i = 0; i < count; i++)
            {
                if (rec_video_type == REC_VIDEO_WINDOWS)
                {
                    string = obs_property_list_item_string(property_, i);
                }
                else
                {
                    const char* item_name = obs_property_list_item_name(property_, i);
                    string = item_name;
                }
                const char* str = obs_property_list_item_string(property_, i);
                cout << "list -> " << str << " : " << string << endl;
                vec_rec_targets_.push_back(string);
            }
        }
        obs_property_next(&property_);
    }
    return 0;
}

int OBSImp::UpdateVideoRecItem(const char* target, REC_VIDEO_TYPE rec_video_type)
{
    for (auto item : vec_rec_targets_)
    {
        if (item == std::string(target))
        { //
            if (rec_video_type == REC_VIDEO_DESKTOP)
            {
                obs_data_set_string(setting_source_, "monitor", target); // 设置源
            }
            else if (rec_video_type == REC_VIDEO_CAMERA)
            {
                obs_data_set_string(setting_source_, "dshow", target);
            }
            else if (rec_video_type == REC_VIDEO_WINDOWS)
            {
                obs_data_set_string(setting_source_, "window", target);
            }
            else
            {
                cout << __FUNCTION__ << " failed ..." << endl;
                return -1;
            }
            obs_source_update(capture_source, setting_source_);
            break;
        }
    }
    obs_data_release(setting_source_);
    return 0;
}

int OBSImp::UpdateAudioRecItem(REC_AUDIO_TYPE rec_audio_type)
{
    recMicAudio();
    recSystemAudio();
    return 0;
    int ret = 0;
    switch (rec_audio_type)
    {
    case REC_AUDIO_MIC_SYS: // 录制麦克风+系统声音
        recMicAudio();
        recSystemAudio();
        break;
    case REC_AUDIO_MIC_ONLY: // 录制麦克风
        recMicAudio();
        break;
    case REC_AUDIO_SYS_ONLY: // 录制系统声音
        recSystemAudio();
        break;
    default:
        break;
    }
    return ret;
}

/**
 * @brief 产生时间秒数
 */
time_t getTimeSeconds()
{
    time_t myt = time(NULL);
    return myt;
}
string OBSImp::GetTimeSecondsString()
{
    std::string str = "";
    return str;
}

void OBSImp::recSystemAudio()
{
    bool has_desktop_audio = HasAudioDevices(OUTPUT_AUDIO_SOURCE);
    if (has_desktop_audio)
        ResetAudioDevice(OUTPUT_AUDIO_SOURCE, "default", "Default Desktop Audio", SOURCE_CHANNEL_AUDIO_OUTPUT);
}

void OBSImp::recMicAudio()
{
    bool has_mic_audio = HasAudioDevices(INPUT_AUDIO_SOURCE);
    if (has_mic_audio)
        ResetAudioDevice(INPUT_AUDIO_SOURCE, "default", "Default Mic/Aux", SOURCE_CHANNEL_AUDIO_INPUT);
}

// 设置音频的格式
int OBSImp::resetAudio()
{
    struct obs_audio_info ai;
    ai.samples_per_sec = 44100; // 采样率
    ai.speakers = SPEAKERS_STEREO;
    return obs_reset_audio(&ai);
}

int OBSImp::resetVideo()
{
    struct obs_video_info ovi;
    memset(&ovi, 0, sizeof(struct obs_video_info));
    ovi.fps_num = VIDEO_FPS;
    ovi.fps_den = 1;
    ovi.graphics_module = "libobs-opengl.so.1";
    //    ovi.base_width = 1920;
    //    ovi.base_height = 1080;
    ovi.base_width = BASE_WIDTH; // 视频源的分辨率区域大小
    ovi.base_height = BASE_HEIGHT;
    //    ovi.output_width = 1920;
    //    ovi.output_height = 1080;
    ovi.output_width = 1280;
    ovi.output_height = 720;
    ovi.output_format = VIDEO_FORMAT_I420; // 视频输出格式YUV420P
    ovi.colorspace = VIDEO_CS_709;
    ovi.range = VIDEO_RANGE_PARTIAL;
    ovi.adapter = 0;
    ovi.gpu_conversion = true;
    ovi.scale_type = OBS_SCALE_BICUBIC;
    int ret = obs_reset_video(&ovi);
    cout << "obs_reset_video ret:" << ret << endl;
    return ret;
}

bool OBSImp::createOutputMode()
{
    if (!file_output_)
    {
        //高级输出 ffmpeg
        file_output_ = obs_output_create("ffmpeg_output", "simple_ffmpeg_output", nullptr, nullptr);
        if (!file_output_)
        {
            throw "Failed to create recording FFmpeg output "
                  "(simple output)";
            return false;
        }
    }
    for (int i = 0; i < MAX_AUDIO_MIXES; i++)
    {
        char name[9];
        sprintf(name, "adv_aac%d", i);
        if (!CreateAACEncoder(aac_track_[i], aac_encoder_id_[i], name, i))
        { // 创建aac编码器
            return false;
        }
        obs_encoder_set_audio(aac_track_[i], obs_get_audio()); // 绑定相应的音频编码器
    }
    return true;
}

int OBSImp::setupLocalRecord(string& file_name)
{
    obs_data_t* settings = obs_data_create(); // 输出配置
    obs_data_set_string(settings, "url", file_name.c_str());
    obs_data_set_string(settings, "format_name", RECORD_OUTPUT_FORMAT);
    obs_data_set_string(settings, "format_mime_type", RECORD_OUTPUT_FORMAT_MIME);
    obs_data_set_string(settings, "muxer_settings", "movflags=faststart");
    obs_data_set_int(settings, "gop_size", VIDEO_FPS * 1);
    obs_data_set_string(settings, "video_encoder", VIDEO_ENCODER_NAME);
    obs_data_set_int(settings, "video_encoder_id", VIDEO_ENCODER_ID);
    if (VIDEO_ENCODER_ID == AV_CODEC_ID_H264)
    {
        obs_data_set_string(settings, "video_settings", "profile=main x264-params=crf=22");
    }
    else if (VIDEO_ENCODER_ID == AV_CODEC_ID_FLV1)
    {
        obs_data_set_int(settings, "video_bitrate", VIDEO_BITRATE);
    }
    obs_data_set_int(settings, "audio_bitrate", AUDIO_BITRATE);
    obs_data_set_string(settings, "audio_encoder", "aac");
    obs_data_set_int(settings, "audio_encoder_id", AV_CODEC_ID_AAC);
    obs_data_set_string(settings, "audio_settings", NULL);
    obs_data_set_int(settings, "scale_width", BASE_WIDTH); // 影响编码参数
    obs_data_set_int(settings, "scale_height", BASE_HEIGHT); // 最终视频编码的宽高
    obs_output_set_mixer(file_output_, 1); //混流器
    //设置音频、视频的数据源？
    obs_output_set_media(file_output_, obs_get_video(), obs_get_audio());
    obs_output_update(file_output_, settings);
    obs_data_release(settings);
    if (!obs_output_start(file_output_))
    {
        const char* error = obs_output_get_last_error(file_output_);
        cout << "StartRec err:" << error << endl;
        return -1;
    }
    return 0;
}

int OBSImp::setupWhipPush(string& url)
{
    OBSEncoder vencoder = obs_video_encoder_create("obs_x264", "test_x264", nullptr, nullptr);
    OBSEncoder aencoder = obs_audio_encoder_create("ffmpeg_opus", "test_opus", nullptr, 0, nullptr);
    service = obs_service_create("whip_custom", "simple_stream", nullptr, nullptr);
    cout << __FUNCTION__ << " service " << service << ", url: " << url.c_str() << endl;
    if (!service)
    {
	std::cout << __FUNCTION__ << "obs_service_create failed \n";
	return -1;
    }
    OBSData service_settings = obs_data_create();
    OBSData vencoder_settings = obs_data_create();
    OBSData aencoder_settings = obs_data_create();
    OBSData output_settings = obs_data_create();
    // 创建service
    cout << "创建rtmp service" << endl;
    obs_data_set_string(service_settings, "server", url.c_str());

    obs_data_set_int(vencoder_settings, "bitrate", 3 * 1024); // 单位是kbit
    obs_data_set_string(vencoder_settings, "rate_control", "CBR");
    obs_data_set_string(vencoder_settings, "preset", "veryfast");
    // preset
    obs_data_set_string(vencoder_settings, "preset", "veryfast");
    //        baseline、main、high
    obs_data_set_string(vencoder_settings, "profile", "baseline");
    // 微调
    obs_data_set_string(vencoder_settings, "tune", "zerolatency");
    obs_data_set_string(vencoder_settings, "x264opts", "bframes=0");
    obs_data_set_int(vencoder_settings, "keyint_sec", 1);
    obs_data_set_int(aencoder_settings, "bitrate", 32);
    obs_service_update(service, service_settings);
    obs_service_apply_encoder_settings(service, vencoder_settings, aencoder_settings);
    /* -----------------------------------*/
    /* create output                      */
    auto     output_type = "whip_output";

    cout << "创建whip output" << endl;
    streamingOutput = obs_output_create(output_type, "simple_stream", nullptr, nullptr);
    if (!streamingOutput)
    {
	cout << "obs_output_create whip output failed" << endl;
	return -1;
    }
    //    obs_output_release(streamingOutput);
    //    obs_output_update(streamingOutput, output_settings);
    //    const char *audio_codec = obs_output_get_supported_audio_codecs(streamingOutput);
    //    if (strcmp(audio_codec, "aac") != 0) {
    //        const char *id = FindAudioEncoderFromCodec(audio_codec);
    //        aencoder = obs_audio_encoder_create(id, "test_audio", nullptr,
    //                                            0, nullptr);
    ////        obs_encoder_release(aencoder);
    //    }
    /* -----------------------------------*/
    /* connect encoders/services/outputs  */
    obs_encoder_update(vencoder, vencoder_settings);
    obs_encoder_update(aencoder, aencoder_settings);
    obs_data_release(aencoder_settings);
    obs_data_release(vencoder_settings);
    obs_encoder_set_video(vencoder, obs_get_video());
    obs_encoder_set_audio(aencoder, obs_get_audio());
    obs_output_set_video_encoder(streamingOutput, vencoder);
    obs_output_set_audio_encoder(streamingOutput, aencoder, 0);
    obs_output_set_reconnect_settings(streamingOutput, 5, 1);
    obs_output_set_service(streamingOutput, service);
    // 启动i
    bool ret = obs_output_start(streamingOutput);
    if (!ret)
    {
	cout << "obs_output_start rtmp failed" << endl;
	return -1;
    }
    //    obs_encoder_release(vencoder);// 不能释放
    return 0;
}

int OBSImp::setupRtmpPush(string& url)
{
    OBSEncoder vencoder = obs_video_encoder_create("obs_x264", "test_x264", nullptr, nullptr);
    OBSEncoder aencoder = obs_audio_encoder_create("ffmpeg_aac", "test_aac", nullptr, 0, nullptr);
    service = obs_service_create("rtmp_common", "simple_stream", nullptr, nullptr);
    cout << __FUNCTION__ << " service " << service << ", url: " << url.c_str() << endl;
    if (!service)
    {
        std::cout << __FUNCTION__ << "obs_service_create failed \n";
        return -1;
    }
    OBSData service_settings = obs_data_create();
    OBSData vencoder_settings = obs_data_create();
    OBSData aencoder_settings = obs_data_create();
    OBSData output_settings = obs_data_create();
    // 创建service
    cout << "创建rtmp service" << endl;
    obs_data_set_string(service_settings, "streamType", "rtmp_common");
    obs_data_set_string(service_settings, "service", "Twitch");
    obs_data_set_bool(service_settings, "show_all", 0);
    obs_data_set_string(service_settings, "server", url.c_str());
    obs_data_set_string(service_settings, "key", "test");
    obs_data_set_int(vencoder_settings, "bitrate", 3 * 1024); // 单位是kbit
    obs_data_set_string(vencoder_settings, "rate_control", "CBR");
    obs_data_set_string(vencoder_settings, "preset", "veryfast");
    // preset
    obs_data_set_string(vencoder_settings, "preset", "veryfast");
    //        baseline、main、high
    obs_data_set_string(vencoder_settings, "profile", "baseline");
    // 微调
    obs_data_set_string(vencoder_settings, "tune", "zerolatency");
    obs_data_set_string(vencoder_settings, "x264opts", "bframes=0");
    obs_data_set_int(vencoder_settings, "keyint_sec", 1);
    obs_data_set_int(aencoder_settings, "bitrate", 32);
    obs_service_update(service, service_settings);
    obs_service_apply_encoder_settings(service, vencoder_settings, aencoder_settings);
    /* -----------------------------------*/
    /* create output                      */
    auto     output_type = "rtmp_output";

    cout << "创建rtmp output" << endl;
    streamingOutput = obs_output_create(output_type, "simple_stream", nullptr, nullptr);
    if (!streamingOutput)
    {
        cout << "obs_output_create rtmp output failed" << endl;
        return -1;
    }
    //    obs_output_release(streamingOutput);
    //    obs_output_update(streamingOutput, output_settings);
    //    const char *audio_codec = obs_output_get_supported_audio_codecs(streamingOutput);
    //    if (strcmp(audio_codec, "aac") != 0) {
    //        const char *id = FindAudioEncoderFromCodec(audio_codec);
    //        aencoder = obs_audio_encoder_create(id, "test_audio", nullptr,
    //                                            0, nullptr);
    ////        obs_encoder_release(aencoder);
    //    }
    /* -----------------------------------*/
    /* connect encoders/services/outputs  */
    obs_encoder_update(vencoder, vencoder_settings);
    obs_encoder_update(aencoder, aencoder_settings);
    obs_data_release(aencoder_settings);
    obs_data_release(vencoder_settings);
    obs_encoder_set_video(vencoder, obs_get_video());
    obs_encoder_set_audio(aencoder, obs_get_audio());
    obs_output_set_video_encoder(streamingOutput, vencoder);
    obs_output_set_audio_encoder(streamingOutput, aencoder, 0);
    obs_output_set_reconnect_settings(streamingOutput, 0, 0);
    obs_output_set_service(streamingOutput, service);
    // 启动
    bool ret = obs_output_start(streamingOutput);
    if (!ret)
    {
        cout << "obs_output_start rtmp failed" << endl;
        return -1;
    }
    //    obs_encoder_release(vencoder);// 不能释放
    return 0;
}
