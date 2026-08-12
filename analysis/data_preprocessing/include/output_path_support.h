#ifndef CSHINE_GAMMA_OUTPUT_PATH_SUPPORT_H
#define CSHINE_GAMMA_OUTPUT_PATH_SUPPORT_H

#include <TSystem.h>

#include <stdexcept>
#include <string>
#include <vector>

namespace cshine_gamma {
namespace detail {

inline std::string ParentDirectory(const std::string& path) {
  const std::string::size_type separator = path.find_last_of("/\\");
  if (separator == std::string::npos) {
    return std::string();
  }
  if (separator == 0) {
    return path.substr(0, 1);
  }
  return path.substr(0, separator);
}

inline void EnsureOutputParentDirectory(const std::string& path) {
  if (path.empty()) {
    return;
  }
  const std::string directory = ParentDirectory(path);
  if (directory.empty() ||
      !gSystem->AccessPathName(directory.c_str(), kFileExists)) {
    return;
  }
  if (gSystem->mkdir(directory.c_str(), true) != 0 &&
      gSystem->AccessPathName(directory.c_str(), kFileExists)) {
    throw std::runtime_error("cannot create output directory: " + directory);
  }
}

inline void EnsureOutputParentDirectories(
    const std::vector<std::string>& paths) {
  for (std::vector<std::string>::const_iterator path = paths.begin();
       path != paths.end(); ++path) {
    EnsureOutputParentDirectory(*path);
  }
}

}  // namespace detail
}  // namespace cshine_gamma

#endif
