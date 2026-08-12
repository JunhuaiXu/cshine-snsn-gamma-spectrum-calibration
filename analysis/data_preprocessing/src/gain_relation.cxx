// Provenance: DP-S100,
// DataPreprocessing/step1-2D/20240308_SnSn_GOAL_ALLCOIN.C.

#include "gain_relation.h"

#include "output_path_support.h"
#include "t_2d_fit.h"

#include <TAxis.h>
#include <TCanvas.h>
#include <TChain.h>
#include <TF1.h>
#include <TFile.h>
#include <TFitResult.h>
#include <TFitResultPtr.h>
#include <TGraph.h>
#include <TH1.h>
#include <TPad.h>
#include <TSystem.h>
#include <TTree.h>

#include <cmath>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>

namespace cshine_gamma {
namespace {

bool PathExists(const std::string& path) {
  return !path.empty() && !gSystem->AccessPathName(path.c_str(), kFileExists);
}

void RequireNewOutputPath(const std::string& path,
                          const char* label,
                          bool overwrite,
                          bool optional = false) {
  if (path.empty()) {
    if (optional) {
      return;
    }
    throw std::invalid_argument(std::string(label) + " path must not be empty");
  }
  if (!overwrite && PathExists(path)) {
    throw std::runtime_error(std::string(label) + " already exists: " + path);
  }
}

void RequireDistinctOutputPaths(const std::vector<std::string>& paths) {
  for (std::size_t left = 0; left < paths.size(); ++left) {
    if (paths[left].empty()) {
      continue;
    }
    for (std::size_t right = left + 1; right < paths.size(); ++right) {
      if (!paths[right].empty() && paths[left] == paths[right]) {
        throw std::invalid_argument("output paths must be distinct: " +
                                    paths[left]);
      }
    }
  }
}

void RequireReadableFiles(const std::vector<std::string>& files) {
  for (std::vector<std::string>::const_iterator file = files.begin();
       file != files.end(); ++file) {
    if (gSystem->AccessPathName(file->c_str(), kReadPermission)) {
      throw std::runtime_error("missing or unreadable gain-relation input: " +
                               *file);
    }
  }
}

void RequireInputSchemas(const std::vector<std::string>& files,
                         const GainRelationDefinition& definition) {
  for (std::vector<std::string>::const_iterator file = files.begin();
       file != files.end(); ++file) {
    std::unique_ptr<TFile> input(TFile::Open(file->c_str(), "READ"));
    if (!input || input->IsZombie()) {
      throw std::runtime_error("cannot open gain-relation input: " + *file);
    }
    TTree* tree = nullptr;
    input->GetObject(definition.tree_name.c_str(), tree);
    if (tree == nullptr) {
      throw std::runtime_error("missing tree '" + definition.tree_name +
                               "' in " + *file);
    }
    for (unsigned int channel = 0; channel < definition.channel_count;
         ++channel) {
      const std::string low_branch = LowGainBranchName(channel);
      const std::string high_branch = GainHighGainBranchName(channel);
      if (tree->GetBranch(low_branch.c_str()) == nullptr) {
        throw std::runtime_error("missing branch " + low_branch + " in " +
                                 *file);
      }
      if (tree->GetBranch(high_branch.c_str()) == nullptr) {
        throw std::runtime_error("missing branch " + high_branch + " in " +
                                 *file);
      }
    }
  }
}

void AddFiles(TChain& chain, const std::vector<std::string>& files) {
  for (std::vector<std::string>::const_iterator file = files.begin();
       file != files.end(); ++file) {
    if (chain.AddFile(file->c_str()) != 1) {
      throw std::runtime_error("failed to add gain-relation input: " + *file);
    }
  }
}

std::string FitFunctionName(unsigned int channel) {
  std::ostringstream name;
  name << "f_" << channel;
  return name.str();
}

std::string GraphTitle(unsigned int channel) {
  std::ostringstream title;
  title << "ADC[" << channel << "]:ADC[" << channel + 16
        << "] { 150<ADC[" << channel << "]<600";
  return title.str();
}

std::string ChannelTitle(unsigned int channel) {
  std::ostringstream title;
  title << "CH " << channel << " E-XE";
  return title.str();
}

}  // namespace

GainRelationDefinition Central0308GainRelationDefinition() {
  GainRelationDefinition definition;
  definition.name = "central_0308";
  definition.tree_name = "tree";
  definition.input_prefix = "20240308_SnSn_GOAL_ALLCOIN";
  definition.first_file = 0;
  definition.last_file = 104;
  definition.channel_count = 15;
  definition.low_gain_min_exclusive = 150.0;
  definition.low_gain_max_exclusive = 600.0;
  definition.fit_high_gain_min = 1000.0;
  definition.fit_high_gain_max = 3500.0;
  definition.function_high_gain_min = 0.0;
  definition.function_high_gain_max = 4096.0;
  definition.slope_min = 0.099;
  definition.slope_max = 0.101;
  definition.output_object_name = "f_data";
  definition.output_object_title = "20240308_SnSn_GOAL_ALLCOIN.root";
  definition.canvas_name = "c";
  definition.canvas_title =
      "Gamma-E-XE-20240308_SnSn_GOAL_ALLCOIN-0-104";
  definition.canvas_width = 1920;
  definition.canvas_height = 1500;
  definition.canvas_columns = 4;
  definition.canvas_rows = 4;
  return definition;
}

void ValidateGainRelationDefinition(const GainRelationDefinition& definition) {
  if (definition.name.empty() || definition.tree_name.empty() ||
      definition.input_prefix.empty()) {
    throw std::invalid_argument(
        "definition name, tree name, and input prefix are required");
  }
  if (definition.first_file > definition.last_file) {
    throw std::invalid_argument("invalid gain-relation input file range");
  }
  if (definition.channel_count != 15) {
    throw std::invalid_argument("gain-relation channel count must be 15");
  }
  if (!(definition.low_gain_min_exclusive <
        definition.low_gain_max_exclusive) ||
      !(definition.fit_high_gain_min < definition.fit_high_gain_max) ||
      !(definition.function_high_gain_min <
        definition.function_high_gain_max) ||
      !(definition.slope_min < definition.slope_max)) {
    throw std::invalid_argument("invalid gain-relation fit limits");
  }
  if (definition.fit_high_gain_min < definition.function_high_gain_min ||
      definition.fit_high_gain_max > definition.function_high_gain_max) {
    throw std::invalid_argument(
        "fit range must lie inside the function range");
  }
  if (definition.output_object_name.empty() ||
      definition.output_object_title.empty() ||
      definition.canvas_name.empty() || definition.canvas_title.empty()) {
    throw std::invalid_argument("ROOT output object names are required");
  }
  if (definition.canvas_width <= 0 || definition.canvas_height <= 0 ||
      definition.canvas_columns <= 0 || definition.canvas_rows <= 0 ||
      static_cast<unsigned int>(definition.canvas_columns *
                                definition.canvas_rows) <
          definition.channel_count) {
    throw std::invalid_argument("invalid gain-relation canvas layout");
  }
}

std::string FormatGainInputFilename(const std::string& input_directory,
                                    const std::string& input_prefix,
                                    int file_index) {
  std::ostringstream path;
  path << input_directory;
  if (!input_directory.empty() &&
      input_directory[input_directory.size() - 1] != '/') {
    path << '/';
  }
  path << 'a' << input_prefix << '.' << std::setfill('0') << std::setw(4)
       << file_index << ".root";
  return path.str();
}

std::vector<std::string> BuildGainInputFileList(
    const std::string& input_directory,
    const GainRelationDefinition& definition) {
  ValidateGainRelationDefinition(definition);
  std::vector<std::string> files;
  files.reserve(static_cast<std::size_t>(definition.last_file -
                                         definition.first_file + 1));
  for (int file_index = definition.first_file;
       file_index <= definition.last_file; ++file_index) {
    files.push_back(FormatGainInputFilename(
        input_directory, definition.input_prefix, file_index));
  }
  return files;
}

std::string LowGainBranchName(unsigned int channel) {
  std::ostringstream name;
  name << "GAMMA" << channel + 1 << "_LOW_E";
  return name.str();
}

std::string GainHighGainBranchName(unsigned int channel) {
  std::ostringstream name;
  name << "GAMMA" << channel + 1 << "_HIGH_E";
  return name.str();
}

std::string GainSelection(const GainRelationDefinition& definition,
                          unsigned int channel) {
  const std::string branch = LowGainBranchName(channel);
  std::ostringstream selection;
  selection << branch << '<' << definition.low_gain_max_exclusive << "&&"
            << branch << '>' << definition.low_gain_min_exclusive;
  return selection.str();
}

int HistoricalGainCanvasPad(unsigned int channel) {
  if (channel >= 15) {
    throw std::out_of_range("historical gain canvas channel must be 0..14");
  }
  return channel == 12 ? 1 : 16 - static_cast<int>(channel);
}

GainRelationSummary FitGainRelation(
    const GainRelationDefinition& definition,
    const std::string& input_directory,
    const std::string& output_root_file,
    const std::string& parameter_file,
    const std::string& report_file,
    const std::string& canvas_pdf_file,
    const std::string& canvas_png_file,
    bool overwrite) {
  ValidateGainRelationDefinition(definition);
  RequireDistinctOutputPaths({output_root_file,
                              parameter_file,
                              report_file,
                              canvas_pdf_file,
                              canvas_png_file});
  RequireNewOutputPath(output_root_file, "ROOT output", overwrite);
  RequireNewOutputPath(parameter_file, "parameter output", overwrite);
  RequireNewOutputPath(report_file, "run report", overwrite);
  RequireNewOutputPath(canvas_pdf_file, "canvas PDF", overwrite, true);
  RequireNewOutputPath(canvas_png_file, "canvas PNG", overwrite, true);

  GainRelationSummary summary;
  summary.input_files = BuildGainInputFileList(input_directory, definition);
  RequireReadableFiles(summary.input_files);
  RequireInputSchemas(summary.input_files, definition);

  detail::EnsureOutputParentDirectories({output_root_file,
                                         parameter_file,
                                         report_file,
                                         canvas_pdf_file,
                                         canvas_png_file});

  TChain chain(definition.tree_name.c_str(), "");
  AddFiles(chain, summary.input_files);
  summary.tree_entries = chain.GetEntries();
  if (summary.tree_entries <= 0 || chain.LoadTree(0) < 0) {
    throw std::runtime_error("gain-relation input chain is empty");
  }
  chain.SetEstimate(summary.tree_entries + 1);

  std::unique_ptr<TCanvas> canvas(new TCanvas(definition.canvas_name.c_str(),
                                               definition.canvas_title.c_str(),
                                               definition.canvas_width,
                                               definition.canvas_height));
  canvas->Divide(definition.canvas_columns, definition.canvas_rows);
  std::unique_ptr<t_2d_fit> parameters(
      new t_2d_fit(definition.output_object_name.c_str(),
                   definition.output_object_title.c_str()));

  summary.channels.reserve(definition.channel_count);
  for (unsigned int channel = 0; channel < definition.channel_count;
       ++channel) {
    const std::string low_branch = LowGainBranchName(channel);
    const std::string high_branch = GainHighGainBranchName(channel);
    const std::string expression = low_branch + ":" + high_branch;
    const std::string selection = GainSelection(definition, channel);
    const Long64_t selected =
        chain.Draw(expression.c_str(), selection.c_str(), "goff");
    if (selected < 2) {
      throw std::runtime_error("fewer than two selected gain-relation points "
                               "for channel " +
                               std::to_string(channel));
    }

    const double* low_values = chain.GetV1();
    const double* high_values = chain.GetV2();
    Long64_t fit_range_points = 0;
    for (Long64_t point = 0; point < selected; ++point) {
      if (high_values[point] >= definition.fit_high_gain_min &&
          high_values[point] <= definition.fit_high_gain_max) {
        ++fit_range_points;
      }
    }
    if (fit_range_points < 2) {
      throw std::runtime_error("fewer than two gain-relation points in the "
                               "fit range for channel " +
                               std::to_string(channel));
    }

    TGraph* graph = new TGraph(selected, high_values, low_values);
    graph->SetName("Graph");
    graph->SetTitle(GraphTitle(channel).c_str());

    const int canvas_pad = HistoricalGainCanvasPad(channel);
    TPad* pad = static_cast<TPad*>(canvas->cd(canvas_pad));
    graph->Draw("AP");
    graph->GetHistogram()->SetTitle(ChannelTitle(channel).c_str());
    graph->GetXaxis()->SetTitle("ADC XE CH ");
    graph->GetXaxis()->SetLabelSize(0.08);
    graph->GetXaxis()->SetTitleSize(0.1);
    graph->GetXaxis()->SetTitleOffset(-0.5);
    graph->GetXaxis()->SetNdivisions(505);
    graph->GetYaxis()->SetTitle("ADC E CH ");
    graph->GetYaxis()->SetLabelSize(0.08);
    graph->GetYaxis()->SetTitleSize(0.1);
    graph->GetYaxis()->SetTitleOffset(-0.5);

    TF1* fit = new TF1(FitFunctionName(channel).c_str(),
                       "[a0]+[a1]*x",
                       definition.function_high_gain_min,
                       definition.function_high_gain_max);
    fit->SetParLimits(1, definition.slope_min, definition.slope_max);
    const TFitResultPtr fit_result = graph->Fit(
        fit, "QS", "", definition.fit_high_gain_min,
        definition.fit_high_gain_max);
    pad->Modified();
    pad->Update();

    GainChannelFitSummary channel_summary;
    channel_summary.channel = channel;
    channel_summary.canvas_pad = canvas_pad;
    channel_summary.selected_points = selected;
    channel_summary.fit_range_points = fit_range_points;
    channel_summary.fit_status = static_cast<int>(fit_result);
    channel_summary.chi_square = fit->GetChisquare();
    channel_summary.degrees_of_freedom = fit->GetNDF();
    channel_summary.intercept = fit->GetParameter(0);
    channel_summary.intercept_error = fit->GetParError(0);
    channel_summary.slope = fit->GetParameter(1);
    channel_summary.slope_error = fit->GetParError(1);
    summary.channels.push_back(channel_summary);

    parameters->SetPoint(channel,
                         channel_summary.intercept,
                         channel_summary.intercept_error,
                         channel_summary.slope,
                         channel_summary.slope_error);
  }

  canvas->Modified();
  canvas->Update();

  TFile output(output_root_file.c_str(), overwrite ? "RECREATE" : "CREATE");
  if (output.IsZombie()) {
    throw std::runtime_error("cannot create ROOT output: " + output_root_file);
  }
  output.WriteTObject(parameters.get(), definition.output_object_name.c_str());
  output.WriteTObject(canvas.get(), definition.canvas_name.c_str());
  output.Close();

  WriteGainParameterFile(parameter_file, summary, overwrite);
  WriteGainRelationReport(report_file,
                          definition,
                          summary,
                          output_root_file,
                          parameter_file,
                          canvas_pdf_file,
                          canvas_png_file,
                          overwrite);
  if (!canvas_pdf_file.empty()) {
    canvas->SaveAs(canvas_pdf_file.c_str());
  }
  if (!canvas_png_file.empty()) {
    canvas->SaveAs(canvas_png_file.c_str());
  }
  return summary;
}

void WriteGainParameterFile(const std::string& parameter_file,
                            const GainRelationSummary& summary,
                            bool overwrite) {
  RequireNewOutputPath(parameter_file, "parameter output", overwrite);
  std::ofstream output(parameter_file.c_str(),
                       overwrite ? std::ios::trunc : std::ios::out);
  if (!output) {
    throw std::runtime_error("cannot create gain parameter output: " +
                             parameter_file);
  }
  output << std::setprecision(6);
  output << "double f_2d_fit[][2][2] = {\n";
  for (std::size_t channel = 0; channel < summary.channels.size(); ++channel) {
    const GainChannelFitSummary& fit = summary.channels[channel];
    output << "{{" << fit.intercept << ", " << fit.intercept_error << "}, {"
           << fit.slope << ", " << fit.slope_error << "}}";
    if (channel + 1 != summary.channels.size()) {
      output << ',';
    }
    output << '\n';
  }
  output << "};\n";
}

void WriteGainRelationReport(const std::string& report_file,
                             const GainRelationDefinition& definition,
                             const GainRelationSummary& summary,
                             const std::string& output_root_file,
                             const std::string& parameter_file,
                             const std::string& canvas_pdf_file,
                             const std::string& canvas_png_file,
                             bool overwrite) {
  RequireNewOutputPath(report_file, "run report", overwrite);
  std::ofstream report(report_file.c_str(),
                       overwrite ? std::ios::trunc : std::ios::out);
  if (!report) {
    throw std::runtime_error("cannot create gain-relation report: " +
                             report_file);
  }
  report << std::setprecision(17);
  report << "record\tfield_1\tfield_2\tfield_3\tfield_4\tfield_5\tfield_6\tfield_7\tfield_8\tfield_9\tfield_10\n";
  report << "config\tname\t" << definition.name << "\t\t\t\t\t\t\t\t\n";
  report << "config\ttree_name\t" << definition.tree_name
         << "\t\t\t\t\t\t\t\t\n";
  report << "config\tinput_prefix\t" << definition.input_prefix
         << "\t\t\t\t\t\t\t\t\n";
  report << "config\tfile_range\t" << definition.first_file << '\t'
         << definition.last_file << "\t\t\t\t\t\t\t\n";
  report << "config\tlow_gain_selection_exclusive\t"
         << definition.low_gain_min_exclusive << '\t'
         << definition.low_gain_max_exclusive << "\t\t\t\t\t\t\t\n";
  report << "config\thigh_gain_fit_range\t"
         << definition.fit_high_gain_min << '\t'
         << definition.fit_high_gain_max << "\t\t\t\t\t\t\t\n";
  report << "config\tslope_limits\t" << definition.slope_min << '\t'
         << definition.slope_max << "\t\t\t\t\t\t\t\n";
  report << "config\toutput_root\t" << output_root_file
         << "\t\t\t\t\t\t\t\t\n";
  report << "config\tparameter_file\t" << parameter_file
         << "\t\t\t\t\t\t\t\t\n";
  report << "config\tcanvas_pdf\t" << canvas_pdf_file
         << "\t\t\t\t\t\t\t\t\n";
  report << "config\tcanvas_png\t" << canvas_png_file
         << "\t\t\t\t\t\t\t\t\n";
  report << "entries\ttree\t" << summary.tree_entries
         << "\t\t\t\t\t\t\t\t\n";
  for (std::size_t index = 0; index < summary.input_files.size(); ++index) {
    report << "input\t" << index << '\t' << summary.input_files[index]
           << "\t\t\t\t\t\t\t\t\n";
  }
  for (std::vector<GainChannelFitSummary>::const_iterator channel =
           summary.channels.begin();
       channel != summary.channels.end(); ++channel) {
    report << "channel\t" << channel->channel << '\t'
           << channel->canvas_pad << '\t' << channel->selected_points << '\t'
           << channel->fit_range_points << '\t' << channel->fit_status << '\t'
           << channel->chi_square << '\t' << channel->degrees_of_freedom
           << '\t' << channel->intercept << '\t'
           << channel->intercept_error << '\t' << channel->slope << '\t'
           << channel->slope_error << '\n';
  }
}

}  // namespace cshine_gamma
