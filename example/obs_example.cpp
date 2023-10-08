#include <filesystem>
#include <iostream>
#include <string>

#include "obs.h"
using namespace std;

bool resetAudio()
{
    struct obs_audio_info ai;
    ai.samples_per_sec = 48000; // 采样率
    ai.speakers = SPEAKERS_STEREO;

    return obs_reset_audio(&ai);
}

#define VIDEO_FPS 25

#define BASE_WIDTH 1920
#define BASE_HEIGHT 1080

bool resetVideo()
{
    struct obs_video_info ovi;
    memset(&ovi, 0, sizeof(struct obs_video_info));
    ovi.fps_num = VIDEO_FPS;
    ovi.fps_den = 1;

    ovi.graphics_module = "libobs-d3d11.dll";
    //    ovi.base_width = 1920;
    //    ovi.base_height = 1080;
    ovi.base_width = BASE_WIDTH; // 视频源的分辨率区域大小
    ovi.base_height = BASE_HEIGHT;
    //    ovi.output_width = 1920;
    //    ovi.output_height = 1080;
    ovi.output_width = BASE_WIDTH;
    ovi.output_height = BASE_HEIGHT;
    ovi.output_format = VIDEO_FORMAT_I420; // 视频输出格式YUV420P
    ovi.colorspace = VIDEO_CS_709;
    ovi.range = VIDEO_RANGE_PARTIAL;
    ovi.adapter = 0;
    ovi.gpu_conversion = true;
    ovi.scale_type = OBS_SCALE_BICUBIC;

    int ret = obs_reset_video(&ovi);
    return ret;
}

int main()
{
    string cfg_path = "cfg_dir"; // 文件所在目录

    if (!obs_initialized())
    {
        //初始化obs
        if (!obs_startup("zh-CN", cfg_path.c_str(), NULL))
        {
            cout << "初始化obs失败" << endl;
            return -1;
        }
        obs_add_module_path("/mnt/d/workspace/project/obs-sample/output/plugins",
                            "/mnt/d/workspace/project/obs-sample/output/data/%module%");
        obs_load_all_modules();
    }

    if (!resetAudio())
    {
        cout << "初始化音频失败" << endl;
        return -1;
    }
    if (!resetVideo())
    {
        cout << "初始化视频失败" << endl;
        return -1;
    }
    cout << "end" << endl;
}
