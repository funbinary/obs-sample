#ifndef MAINWIDGET_H
#define MAINWIDGET_H

#include "obsimp.h"
#include <QTimer>
#include <QWidget>
QT_BEGIN_NAMESPACE
namespace Ui {
class MainWidget;
}
QT_END_NAMESPACE

class MainWidget : public QWidget {
  Q_OBJECT

public:
  MainWidget(QWidget *parent = nullptr);
  ~MainWidget();

  int InitUi();

  void switchRecVideoType(int index);
  void switchRecAudioType(int index);
  void onTimer(); // 计时
private slots:
  void on_startBtn_clicked();

  void on_stopBtn_clicked();

  void on_videoComboBox_currentIndexChanged(int index);

  void on_audioComboBox_currentIndexChanged(int index);

private:
  Ui::MainWidget *ui;
  std::unique_ptr<OBSImp> obs_;
  // 录制时间
  int rec_seconds_ = 0;
  QTimer *rec_timer_;

  bool is_rec_ = false; //是否为录制状态

  REC_VIDEO_TYPE rec_video_type_ = REC_VIDEO_DESKTOP;
  REC_AUDIO_TYPE rec_audio_type_ = REC_AUDIO_MIC_SYS;
};

#endif // MAINWIDGET_H
