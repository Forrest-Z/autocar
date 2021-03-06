#pragma once

#include <memory>
#include "planning/common/planning_gflags.h"
#include "planning/replay/replay.h"
#include "planning/tasks/task.h"

namespace planning {

class RecordTrajectory : public Task {
 public:
  RecordTrajectory(const common::base::ConfNode& yaml_conf);
  ~RecordTrajectory();

  bool Init();
  bool Run(Frame* frame);
  bool Stop();

 private:
  // conf
  double record_step_ = 0.5;
  int step_count_ = 0;
  car_msgs::trajectory_point last_state_;
  std::unique_ptr<Replay> replay_;
};

}  // namespace planning