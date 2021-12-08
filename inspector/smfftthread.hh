// Licensed GNU LGPL v2.1 or later: http://www.gnu.org/licenses/lgpl-2.1.html

#ifndef SPECTMORPH_FFTTHREAD_HH
#define SPECTMORPH_FFTTHREAD_HH

#include "smcommon.hh"
#include "smpixelarray.hh"
#include "smwavdata.hh"

#include <QObject>
#include <QMutex>

#include <thread>

namespace SpectMorph
{

class FFTThread : public QObject
{
  Q_OBJECT
public:
  struct Command : public QObject {
    virtual void execute() = 0;
  };

protected:
  QMutex                  command_mutex;
  std::vector<Command *>  commands;
  std::vector<Command *>  command_results;
  double                  command_progress;

  int                     fft_thread_wakeup_pfds[2];
  int                     main_thread_wakeup_pfds[2];

  std::thread             thread;

public:
  FFTThread();
  ~FFTThread();

  void set_command_progress (double progress);
  bool command_is_obsolete();

  void run();
  void compute_image (const WavData& wav_data, const AnalysisParams& params);
  bool get_result (PixelArray& image);
  double get_progress();

  static FFTThread *the();

public slots:
  void on_result_available();

signals:
  void result_available();
};

class AnalysisCommand : public FFTThread::Command
{
  Q_OBJECT
public:
  FFTThread              *fft_thread;
  WavData                 wav_data;
  std::vector<FFTResult>  results;
  PixelArray              image;
  AnalysisParams          analysis_params;

  AnalysisCommand (const WavData& wav_data, const AnalysisParams& analysis_params, FFTThread *fft_thread);
  ~AnalysisCommand();
  void execute();
  void execute_cwt();

public slots:
  void set_progress (double progress);
};

}

#endif
