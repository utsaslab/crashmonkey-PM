#include "BaseTestCase.h"

namespace tests {

using std::string;

using wrapper::CmFsOps;
using wrapper::DefaultFsFns;
using wrapper::RecordCmFsOps;
using wrapper::PassthroughCmFsOps;

int BaseTestCase::init_values(string mount_dir, long filesys_size) {
  mnt_dir_ = mount_dir;
  filesys_size_ = filesys_size;
  return 0;
}

int BaseTestCase::Run(const int change_fd, const int checkpoint) {
  DefaultFsFns default_fns;
  RecordCmFsOps cm(&default_fns);
  PassthroughCmFsOps pcm(&default_fns);
  if (checkpoint == 0) {
    cm_ = &cm;
  } else {
    cm_ = &pcm;
  }

  int res_1 = run(checkpoint);
  if (res_1 < 0) {
    return res_1;
  }

  if (checkpoint == 0) {
    int res_2 = cm.Serialize(change_fd);
    if (res_2 < 0) {
      return res_2;
    }
  }
  return res_1;
}

}  // namespace tests
