// Licensed GNU LGPL v2.1 or later: http://www.gnu.org/licenses/lgpl-2.1.html

#ifndef SPECTMORPH_MORPH_LINEAR_HH
#define SPECTMORPH_MORPH_LINEAR_HH

#include "smmorphoperator.hh"
#include "smmodulationlist.hh"

#include <string>

namespace SpectMorph
{

class MorphLinear : public MorphOperator
{
public:
  struct Config : public MorphOperatorConfig
  {
    MorphOperatorPtr left_op;
    MorphOperatorPtr right_op;
    std::string      left_path;
    std::string      right_path;

    ModulationData   morphing_mod;
    bool             db_linear;
  };
  static constexpr auto P_MORPHING = "morphing";
protected:
  Config         m_config;
  std::string    load_left, load_right;
  std::string    m_left_smset;
  std::string    m_right_smset;

public:
  MorphLinear (MorphPlan *morph_plan);
  ~MorphLinear();

  // inherited from MorphOperator
  const char        *type() override;
  int                insert_order() override;
  bool               save (OutFile& out_file) override;
  bool               load (InFile&  in_file) override;
  void               post_load (OpNameMap& op_name_map) override;
  OutputType         output_type() override;
  MorphOperatorConfig *clone_config() override;

  std::vector<MorphOperator *> dependencies() override;

  MorphOperator *left_op();
  void set_left_op (MorphOperator *op);

  std::string left_smset();
  void set_left_smset (const std::string& smset);

  MorphOperator *right_op();
  void set_right_op (MorphOperator *op);

  std::string right_smset();
  void set_right_smset (const std::string& smset);

  void set_morphing (double new_morphing);

  bool db_linear();
  void set_db_linear (bool new_db_linear);

/* slots: */
  void on_operator_removed (MorphOperator *op);
};

}

#endif
