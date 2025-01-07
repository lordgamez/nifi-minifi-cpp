#pragma once

#include <string>
#include <vector>
#include <mutex>

namespace org::apache::nifi::minifi::core {

struct Bulletin {
  uint64_t id;
  std::string timestamp; // "Tue Dec 10 08:40:26 CET 2024",
  std::string node_address; // empty
  std::string level;
  std::string category; // "Log Message",
  std::string message;
  std::string group_id; // process group id
  std::string group_name; // process group name
  std::string group_path; // empty
  std::string source_id;
  std::string source_name;
  std::string flow_file_uuid; // empty
};

class BulletinStore {
 public:
  void addBulletin(Bulletin& bulletin) {
    std::lock_guard<std::mutex> lock(mutex_);
    bulletin.id = id_counter++;
    bulletins_.push_back(bulletin);
  }

  std::vector<Bulletin> getBulletins() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return bulletins_;
  }

 private:
  mutable std::mutex mutex_;
  uint64_t id_counter = 1;
  std::vector<Bulletin> bulletins_;
};

}  // namespace org::apache::nifi::minifi::core
