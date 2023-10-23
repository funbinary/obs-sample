#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

#include "obsimp.h"
using namespace std;

REC_VIDEO_TYPE rec_video_type_ = REC_VIDEO_CAMERA;
REC_AUDIO_TYPE rec_audio_type_ = REC_AUDIO_MIC_SYS;
REC_PUSH_TYPE rec_push_type = REC_WHIP;
int main()
{
    std::unique_ptr<OBSImp> obsInstance = std::make_unique<OBSImp>();
    obsInstance->InitOBS();
    obsInstance->AddVideoScensSource(rec_video_type_);
    obsInstance->SearchRecVideoTargets(rec_video_type_);

    //    obsInstance->UpdateVideoRecItem("", rec_video_type_);

    //2. 判断音频录制类型
    obsInstance->UpdateAudioRecItem(rec_audio_type_);
    string server_url = "http://192.168.3.247:9060/index/api/whip?app=live&stream=test";
//    string server_url = "rtmp://192.168.3.247/live"; // 自己修改地址
    //    string server_url = "1.mp4"; // 自己修改地址
    int ret = obsInstance->Start(server_url, rec_push_type);

    if (ret < 0)
    {
        cout << "start failed" << endl;
        return -1;
    }
    int count = 0;
    while (count < 1000)
    {
        count++;
        std::this_thread::sleep_for(1s);
    }

    obsInstance->Stop();
    std::this_thread::sleep_for(5s);
}
