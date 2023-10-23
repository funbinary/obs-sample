#include "mainwidget.h"

#include <QDateTime>
#include <QDebug>
#include <QMessageBox>

#include "obs.h"
#include "ui_mainwidget.h"

MainWidget::MainWidget(QWidget* parent) : QWidget(parent), ui(new Ui::MainWidget), obs_(new OBSImp())
{
    ui->setupUi(this);

    if (!obs_->InitOBS())
    {
        QMessageBox::information(this, "tips", "obs init failed");
        exit(0);
    }
    InitUi();
    rec_timer_ = new QTimer(this);
    connect(rec_timer_, &QTimer::timeout, this, &MainWidget::onTimer);
}

MainWidget::~MainWidget()
{
    delete ui;
}

void MainWidget::onTimer()
{
    rec_seconds_++;

    int hours = rec_seconds_ / 3600;
    int mins = (rec_seconds_ - hours * 3600) / 60;
    int secs = rec_seconds_ - hours * 3600 - mins * 60;

    char buf[256];
    memset(buf, 0, 256);

    sprintf(buf, "%02d:%02d:%02d", hours, mins, secs);

    ui->timeLable->setText(buf);
}

int MainWidget::InitUi()
{
    const char* version = obs_get_version_string();
    qDebug() << "obs version: " << version;
    //   ui->label->setText(QString("零声教育: %1").arg(version));  // 在界面上打印版本

    QFont font("Microsoft YaHei", 14, 75); //第一个属性是字体（微软雅黑），第二个是大小，第三个是加粗（权重是75）
    ui->timeLable->setFont(font);
    //    ui->timeLable->clear();
    ui->timeLable->setText("00:00:00");
    ui->videoComboBox->setCurrentIndex(0);
    int rec_video_index = ui->videoComboBox->currentIndex();
    switchRecVideoType(rec_video_index);
    on_videoComboBox_currentIndexChanged(rec_video_index);
    return 0;
}

void MainWidget::switchRecVideoType(int index)
{
    switch (index)
    {
    case 0:
        rec_video_type_ = REC_VIDEO_DESKTOP;
        break;
    case 1:
        rec_video_type_ = REC_VIDEO_CAMERA;
        break;
    case 2:
        rec_video_type_ = REC_VIDEO_WINDOWS;
        break;
    case 3:
        rec_video_type_ = REC_VIDEO_NO;
        break;
    default:
        rec_video_type_ = REC_VIDEO_DESKTOP;
        break;
    }
}
void MainWidget::switchRecAudioType(int index)
{
    switch (index)
    {
    case 0:
        rec_audio_type_ = REC_AUDIO_MIC_SYS;
        break;
    case 1:
        rec_audio_type_ = REC_AUDIO_MIC_ONLY;
        break;
    case 2:
        rec_audio_type_ = REC_AUDIO_SYS_ONLY;
    case 3:
        rec_audio_type_ = REC_AUDIO_NO;
    default:
        rec_audio_type_ = REC_AUDIO_MIC_SYS;
        break;
    }
}

void MainWidget::on_startBtn_clicked()
{
    //
    if (!is_rec_)
    { // 没有处于录制状态
        rec_seconds_ = 0;
        // 准备录制工作

        //1 判断录制桌面、摄像头、窗口
        QString item_text = ui->videoSourceIdboBox->currentText();
        const char* item_text_str = item_text.toStdString().c_str();
        // 1
        obs_->UpdateVideoRecItem(item_text_str, rec_video_type_);

        //2. 判断音频录制类型
        obs_->UpdateAudioRecItem(rec_audio_type_);

        // 3.启动录制
        QString timestr = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
        string out_file_name = timestr.toStdString() + ".mp4"; // 暂时先使用相对路径
        if (0 == obs_->StartRec(out_file_name))
        {
            is_rec_ = true;
            rec_timer_->start(1000);
        }
        else
        {
            //            QMessageBox::information(this, u8"提示", u8"obs开启录制失败");
            //            QMessageBox::warning(this, "警告", "obs开启录制失败");
        }
    }
    //    else {
    //        qDebug() << "目前还没有实现暂停功能";
    //    }
}

void MainWidget::on_stopBtn_clicked()
{
    if (is_rec_)
    {
        obs_->StopRec();
        rec_timer_->stop();
        rec_seconds_ = 0;
    }
}

//activated(QString):被选中就触发，并传递一个QString，其内容是构件里的text的值
//activated(int):被选中就触发，并传递一个int，其内容是构件里对应的序号 从0开始
//注意：以上两个信号不论控件是否改变，只要被点击都会触发，这也就是程序启动时会触发一次
//currentlndexchanged:控件改变时才会触发，也有两个传参与上面一样，但如果选择没改变则不会触发信号。
//————————————————
//版权声明：本文为CSDN博主「m0_67391683」的原创文章，遵循CC 4.0 BY-SA版权协议，转载请附上原文出处链接及本声明。
//原文链接：https://blog.csdn.net/m0_67391683/article/details/125241371

void MainWidget::on_videoComboBox_currentIndexChanged(int index)
{
    qDebug() << __FUNCTION__ << " index: " << index;
    switchRecVideoType(index);

    // 更新视频对应的源码ID
    ui->videoSourceIdboBox->clear();
    obs_->AddVideoScensSource(rec_video_type_);
    obs_->SearchRecVideoTargets(rec_video_type_);

    std::vector<std::string> vec = obs_->GetRecTargets();
    for (string item : vec)
    {
        ui->videoSourceIdboBox->addItem(QString::fromStdString(item));
    }
}

void MainWidget::on_audioComboBox_currentIndexChanged(int index)
{
    qDebug() << __FUNCTION__ << " index: " << index;
    switchRecAudioType(index);
}
