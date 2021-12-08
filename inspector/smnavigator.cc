// Licensed GNU LGPL v2.1 or later: http://www.gnu.org/licenses/lgpl-2.1.html

#include "smnavigator.hh"
#include "smmicroconf.hh"
#include "smlivedecoder.hh"
#include "smindex.hh"
#include "smutils.hh"

#include <assert.h>
#include <iostream>

#include <QVBoxLayout>
#include <QHeaderView>
#include <QAbstractTableModel>
#include <QTreeView>
#include <QPushButton>
#include <QMessageBox>

using namespace SpectMorph;

using std::vector;
using std::string;

namespace SpectMorph {

class TreeModel : public QAbstractTableModel
{
  WavSet *wset;
public:
  TreeModel (QWidget *parent, WavSet *wset);

  int
  rowCount (const QModelIndex& index) const
  {
    return index.isValid() ? 0 : wset->waves.size();
  }
  int
  columnCount (const QModelIndex& index) const
  {
    return index.isValid() ? 0 : 4;
  }
  QVariant
  data (const QModelIndex& index, int role) const
  {
    if (index.isValid() && role == Qt::DisplayRole)
      {
        const WavSetWave& wave = wset->waves[index.row()];

        switch (index.column())
          {
            case 0: return wset->waves[index.row()].midi_note;
            case 1: return wave.channel;
            case 2: return string_locale_printf ("%d..%d", wave.velocity_range_min, wave.velocity_range_max).c_str();
            case 3: return wave.path.c_str();
          }
      }
    return QVariant();
  }
  QVariant
  headerData (int section, Qt::Orientation orientation, int role) const
  {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
      {
        switch (section)
          {
            case 0: return "Note";
            case 1: return "Ch";
            case 2: return "Range";
            case 3: return "Path";
          }
      }
    return QVariant();
  }
  void
  update_wset()
  {
    beginResetModel();
    endResetModel();
  }
};

TreeModel::TreeModel (QWidget *parent, WavSet *wset) :
  QAbstractTableModel (parent),
  wset (wset)
{
}

}

Navigator::Navigator (const string& filename)
{
  audio = NULL;
  wset_edit = false;

  Index index;
  index.load_file (filename);

  smset_combobox = new QComboBox();

  smset_dir = index.smset_dir();
  for (vector<string>::const_iterator ii = index.smsets().begin(); ii != index.smsets().end(); ii++)
    smset_combobox->addItem (ii->c_str());
  connect (smset_combobox, SIGNAL (currentIndexChanged (int)), this, SLOT (on_combo_changed()));

  tree_model = new TreeModel (this, &wset);
  tree_view = new QTreeView();
  tree_view->setModel (tree_model);
  tree_view->setRootIsDecorated (false);
  connect (tree_view->selectionModel(), SIGNAL (selectionChanged (const QItemSelection&, const QItemSelection&)),
           this, SLOT (on_selection_changed()));

  on_combo_changed();

  source_button = new QPushButton ("Source/Analysis");
  source_button->setCheckable (true);
  connect (source_button, SIGNAL (clicked()), this, SLOT (on_selection_changed()));

  show_position_button = new QPushButton ("Show Position");
  show_position_button->setCheckable (true);
  connect (show_position_button, SIGNAL (clicked()), this, SLOT (on_show_position_changed()));

  show_analysis_button = new QPushButton ("Show Analysis");
  show_analysis_button->setCheckable (true);
  connect (show_analysis_button, SIGNAL (clicked()), this, SLOT (on_show_analysis_changed()));

  show_frequency_grid_button = new QPushButton ("Show Frequency Grid");
  show_frequency_grid_button->setCheckable (true);
  connect (show_frequency_grid_button, SIGNAL (clicked()), this, SLOT (on_show_frequency_grid_changed()));

  QPushButton *save_button = new QPushButton ("Save");
  connect (save_button, SIGNAL (clicked()), this, SLOT (on_save_clicked()));

  QVBoxLayout *vbox = new QVBoxLayout();
  vbox->addWidget (smset_combobox);
  vbox->addWidget (tree_view);
  vbox->addWidget (source_button);
  vbox->addWidget (show_position_button);
  vbox->addWidget (show_analysis_button);
  vbox->addWidget (show_frequency_grid_button);
  vbox->addWidget (save_button);

  setLayout (vbox);

  m_fft_param_window = new FFTParamWindow();

  player_window = new PlayerWindow (this);
  connect (player_window, SIGNAL (next_sample()), this, SLOT (on_next_sample()));
  connect (player_window, SIGNAL (prev_sample()), this, SLOT (on_prev_sample()));

  sample_window = new SampleWindow (this);
  connect (sample_window, SIGNAL (next_sample()), this, SLOT (on_next_sample()));
  connect (sample_window->sample_view(), SIGNAL (audio_edit()), this, SLOT (on_audio_edit()));

  time_freq_window = new TimeFreqWindow (this);
  spectrum_window = new SpectrumWindow (this);

  spectrum_window->set_spectrum_model (time_freq_window->time_freq_view());

  connect (this, SIGNAL (wav_data_changed()), sample_window, SLOT (on_wav_data_changed()));
}

bool
Navigator::handle_close_event()
{
  if (wset_edit)
    {
      int rc =
        QMessageBox::warning (this, "SpectMorph Inspector",
          string_locale_printf ("You changed instrument <b>'%s'</b>."
                                "<p>"
                                "If you quit now your changes will be lost.",
                                smset_combobox->currentText().toLatin1().data()).c_str(),
                                QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel);
      if (rc == QMessageBox::Cancel)
        return false;
    }

  player_window->close();
  sample_window->close();
  time_freq_window->close();
  spectrum_window->close();
  m_fft_param_window->close();
  return true;
}

void
Navigator::on_combo_changed()
{
  string new_filename = smset_dir + "/" + smset_combobox->currentText().toLatin1().data();
  if (new_filename == wset_filename)  // nothing to do (switch to already loaded instrument)
    return;

  if (wset_edit)
    {
      int rc =
        QMessageBox::warning (this, "SpectMorph Inspector",
          string_locale_printf ("You changed instrument <b>'%s'</b>."
                                "<p>"
                                "If you switch instruments now your changes will be lost.",
                                smset_combobox->currentText().toLatin1().data()).c_str(),
                                QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel);
      if (rc == QMessageBox::Cancel)
        {
          smset_combobox->setCurrentText (wset_active_text.c_str());
          return;
        }
    }
  wset_filename = new_filename;
  wset_edit = false;
  wset_active_text = smset_combobox->currentText().toLatin1().data();
  Error error = wset.load (wset_filename);
  if (error)
    {
      fprintf (stderr, "sminspector: can't open input file: %s: %s\n", wset_filename.c_str(), error.message());
      exit (1);
    }

  audio = NULL;
  wav_data.reset();

  tree_model->update_wset();
  for (int column = 0; column < 4; column++)
    tree_view->resizeColumnToContents (column);

  Q_EMIT title_changed();
  Q_EMIT wav_data_changed();
}

void
Navigator::on_selection_changed()
{
  int row = tree_view->selectionModel()->currentIndex().row();

  size_t i = row;
  assert (i < wset.waves.size());

  int channel = wset.waves[i].channel;

  audio = wset.waves[i].audio;
  assert (wset.waves[i].audio);

  wav_data.reset (new WavData());
  if (spectmorph_signal_active())
    {
      LiveDecoder decoder (&wset);
      decoder.retrigger (channel, audio->fundamental_freq, 127, audio->mix_freq);
      decoded_samples.resize (audio->sample_count);
      decoder.process (decoded_samples.size(), nullptr, &decoded_samples[0]);
      wav_data->load (decoded_samples, 1, audio->mix_freq, 32);
    }
  else
    {
      wav_data->load (audio->original_samples, 1, audio->mix_freq, 32);
    }
  Q_EMIT wav_data_changed();
}

void
Navigator::on_view_sample()
{
  sample_window->show();
}

void
Navigator::on_view_player()
{
  player_window->show();
}

void
Navigator::on_view_time_freq()
{
  time_freq_window->show();
}

void
Navigator::on_view_fft_params()
{
  m_fft_param_window->show();
}

Audio *
Navigator::get_audio()
{
  return audio;
}

const WavData *
Navigator::get_wav_data()
{
  return wav_data.get();
}

bool
Navigator::spectmorph_signal_active()
{
  return source_button->isChecked();
}

FFTParamWindow*
Navigator::fft_param_window()
{
  return m_fft_param_window;
}

void
Navigator::on_show_position_changed()
{
  Q_EMIT show_position_changed();
}

void
Navigator::on_show_analysis_changed()
{
  Q_EMIT show_analysis_changed();
}

void
Navigator::on_show_frequency_grid_changed()
{
  Q_EMIT show_frequency_grid_changed();
}

bool
Navigator::get_show_position()
{
  return show_position_button->isChecked();
}

bool
Navigator::get_show_analysis()
{
  return show_analysis_button->isChecked();
}

bool
Navigator::get_show_frequency_grid()
{
  return show_frequency_grid_button->isChecked();
}

void
Navigator::on_save_clicked()
{
  if (wset_filename != "")
    {
      Error error = wset.save (wset_filename);
      if (error)
        {
          fprintf (stderr, "sminspector: can't write output file: %s: %s\n", wset_filename.c_str(), error.message());
          exit (1);
        }
      wset_edit = false;

      Q_EMIT title_changed();
    }
}

void
Navigator::on_audio_edit()
{
  wset_edit = true;
  Q_EMIT title_changed();
}

void
Navigator::on_next_sample()
{
  QModelIndex index = tree_view->selectionModel()->currentIndex();

  if (index.isValid())
    index = tree_model->index (index.row() + 1, 0);  // select next sample
  else
    index = tree_model->index (0, 0); // select first sample if nothing was selected

  if (index.isValid())
    tree_view->selectionModel()->setCurrentIndex (index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
}

void
Navigator::on_prev_sample()
{
  QModelIndex index = tree_view->selectionModel()->currentIndex();

  if (index.isValid())
    index = tree_model->index (index.row() - 1, 0);  // select previous sample
  else
    index = tree_model->index (0, 0); // select first sample if nothing was selected

  if (index.isValid())
    tree_view->selectionModel()->setCurrentIndex (index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
}

void
Navigator::on_view_spectrum()
{
  spectrum_window->show();
}

string
Navigator::title()
{
  string t = "Navigator - ";
  if (wset_edit)
    t += "*";
  t += smset_combobox->currentText().toLatin1().data();
  return t;
}
