#ifndef TIRO_SPEECH_SRC_SCOPED_CHDIR_H_
#define TIRO_SPEECH_SRC_SCOPED_CHDIR_H_

#include <filesystem>

class ScopedChdir {
 public:
  explicit ScopedChdir(const std::filesystem::path& path)
      : oldpwd_{std::filesystem::current_path()} {
    std::filesystem::current_path(path);
  }

  ~ScopedChdir() { std::filesystem::current_path(oldpwd_); }

  ScopedChdir(const ScopedChdir&) = delete;
  ScopedChdir(ScopedChdir&&) = delete;
  const ScopedChdir& operator=(const ScopedChdir&) = delete;
  ScopedChdir&& operator=(ScopedChdir&&) = delete;

 private:
  const std::filesystem::path oldpwd_;
};

#endif  // TIRO_SPEECH_SRC_SCOPED_CHDIR_H_
