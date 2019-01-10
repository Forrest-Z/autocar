#include "control/lon_controller.h"
#include "ros/ros.h"
namespace control {

LonController::LonController(): name_("Pid-Pid LonController"){

}

void LonController::Init(const LonControllerConf *control_conf){
      ts_ = control_conf->ts;
      station_pid_controller_.Init(control_conf->station_pid_conf);
      speed_pid_controller_.Init(control_conf->speed_pid_conf);
}
void LonController::ComputeControlCommand(const car_msgs::trajectory &planning_published_trajectory,
                                          const car_msgs::vehicle_state &vehicle_state,
                                          car_msgs::control_cmd &control_cmd,
                                          car_msgs::lon_debug &debug){
      auto target_tracking_trajectory = planning_published_trajectory;

      trajectory_analyzer_ = std::move(TrajectoryAnalyzer(&target_tracking_trajectory));

      ComputeLongitudinalErrors(
                              vehicle_state.x, 
                              vehicle_state.y, 
                              vehicle_state.yaw,
                              vehicle_state.linear_velocity, 
                              trajectory_analyzer_,
                              debug);

      debug.station_out = station_pid_controller_.Control(debug.station_error,ts_);

      double speed_err = debug.station_out + debug.speed_error;
      //double speed_err = 5 - vehicle_state.linear_velocity;
      
      debug.speed_out = speed_pid_controller_.Control(speed_err,ts_);
      double speed_cmd_out = debug.speed_out;// + debug.preview_acceleration_reference;

      //double speed_cmd_out = 0;
      if(speed_cmd_out > 0.0){
            control_cmd.throttle = speed_cmd_out;
            control_cmd.brake = 0.0;
      }else if(speed_cmd_out < 0.0){
            control_cmd.throttle = 0.0;
            control_cmd.brake = -speed_cmd_out;
      }
      else{
            control_cmd.throttle = 0.0;
            control_cmd.brake = 0.0;
      }
}


void LonController::ComputeLongitudinalErrors(
                                                const double x, 
                                                const double y, 
                                                const double yaw,
                                                const double linear_velocity, 
                                                const TrajectoryAnalyzer &trajectory_analyzer,
                                                car_msgs::lon_debug &debug) {
  // the decomposed vehicle motion onto Frenet frame
  // s: longitudinal accumulated distance along reference path_point
  // s_dot: longitudinal velocity along reference path_point
  // d: lateral distance w.r.t. reference path_point
  // d_dot: lateral distance change rate, i.e. dd/dt
  double s_matched = 0.0;
  double s_dot_matched = 0.0;
  double d_matched = 0.0;
  double d_dot_matched = 0.0;

  auto matched_point = trajectory_analyzer.QueryMatchedPathPoint(x,y);

  trajectory_analyzer.ToTrajectoryFrame(x,y,yaw,linear_velocity, matched_point,
                                          &s_matched, &s_dot_matched, &d_matched, &d_dot_matched);

  double current_control_time = ros::Time::now().toSec();
//TODO:
      car_msgs::trajectory_point reference_point =
       trajectory_analyzer.QueryNearestPointByAbsoluteTime(current_control_time);

  debug.station_error = reference_point.s - s_matched;
  debug.speed_error = reference_point.speed - s_dot_matched;
  debug.station_reference = reference_point.s;
  debug.speed_reference = reference_point.speed;
  debug.preview_acceleration_reference = reference_point.accel;
  debug.current_station = s_matched;
}
void LonController::UpdateParam(const PidConf &station_pid_conf,const PidConf &speed_pid_conf){
      station_pid_controller_.SetPID(station_pid_conf);
      speed_pid_controller_.SetPID(speed_pid_conf);
}

bool LonController::Reset(){
      
}
}//namespace control




















// /******************************************************************************
//  * Copyright 2017 The Apollo Authors. All Rights Reserved.
//  *
//  * Licensed under the Apache License, Version 2.0 (the "License");
//  * you may not use this file except in compliance with the License.
//  * You may obtain a copy of the License at
//  *
//  * http://www.apache.org/licenses/LICENSE-2.0
//  *
//  * Unless required by applicable law or agreed to in writing, software
//  * distributed under the License is distributed on an "AS IS" BASIS,
//  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  * See the License for the specific language governing permissions and
//  * limitations under the License.
//  *****************************************************************************/
// #include "modules/control/controller/lon_controller.h"

// #include <cstdio>
// #include <utility>

// #include "modules/common/configs/vehicle_config_helper.h"
// #include "modules/common/log.h"
// #include "modules/common/math/math_utils.h"
// #include "modules/common/time/time.h"
// #include "modules/common/util/string_util.h"
// #include "modules/control/common/control_gflags.h"
// #include "modules/localization/common/localization_gflags.h"

// namespace apollo {
// namespace control {

// using apollo::common::ErrorCode;
// using apollo::common::Status;
// using apollo::common::path_pointPoint;
// using apollo::common::car_msgs::vehicle_stateProvider;
// using apollo::common::time::Clock;

// constexpr double GRA_ACC = 9.8;

// LonController::LonController()
//     : name_(ControlConf_ControllerType_Name(ControlConf::LON_CONTROLLER)) {
//   if (FLAGS_enable_csv_debug) {
//     time_t rawtime;
//     char name_buffer[80];
//     std::time(&rawtime);
//     strftime(name_buffer, 80, "/tmp/speed_log__%F_%H%M%S.csv",
//              localtime(&rawtime));
//     speed_log_file_ = fopen(name_buffer, "w");
//     if (speed_log_file_ == nullptr) {
//       AERROR << "Fail to open file:" << name_buffer;
//       FLAGS_enable_csv_debug = false;
//     }
//     if (speed_log_file_ != nullptr) {
//       fprintf(speed_log_file_,
//               "station_reference,"
//               "station_error,"
//               "station_error_limited,"
//               "preview_station_error,"
//               "speed_reference,"
//               "speed_error,"
//               "speed_error_limited,"
//               "preview_speed_reference,"
//               "preview_speed_error,"
//               "preview_acceleration_reference,"
//               "acceleration_cmd_closeloop,"
//               "acceleration_cmd,"
//               "acceleration_lookup,"
//               "speed_lookup,"
//               "calibration_value,"
//               "throttle_cmd,"
//               "brake_cmd,"
//               "is_full_stop,"
//               "\r\n");

//       fflush(speed_log_file_);
//     }
//     AINFO << name_ << " used.";
//   }
// }

// void LonController::CloseLogFile() {
//   if (FLAGS_enable_csv_debug) {
//     if (speed_log_file_ != nullptr) {
//       fclose(speed_log_file_);
//       speed_log_file_ = nullptr;
//     }
//   }
// }
// void LonController::Stop() { CloseLogFile(); }

// LonController::~LonController() { CloseLogFile(); }

// Status LonController::Init(const ControlConf *control_conf) {
//   control_conf_ = control_conf;
//   if (control_conf_ == nullptr) {
//     controller_initialized_ = false;
//     AERROR << "get_longitudinal_param() nullptr";
//     return Status(ErrorCode::CONTROL_INIT_ERROR,
//                   "Failed to load LonController conf");
//   }
//   const LonControllerConf &lon_controller_conf =
//       control_conf_->lon_controller_conf();

//   station_pid_controller_.Init(lon_controller_conf.station_pid_conf());
//   speed_pid_controller_.Init(lon_controller_conf.low_speed_pid_conf());

//   vehicle_param_.CopyFrom(
//       common::VehicleConfigHelper::instance()->GetConfig().vehicle_param());

//   SetDigitalFilterPitchAngle(lon_controller_conf);

//   LoadControlCalibrationTable(lon_controller_conf);
//   controller_initialized_ = true;

//   return Status::OK();
// }

// void LonController::SetDigitalFilterPitchAngle(
//     const LonControllerConf &lon_controller_conf) {
//   double cutoff_freq =
//       lon_controller_conf.pitch_angle_filter_conf().cutoff_freq();
//   double ts = lon_controller_conf.ts();
//   SetDigitalFilter(ts, cutoff_freq, &digital_filter_pitch_angle_);
// }

// void LonController::LoadControlCalibrationTable(
//     const LonControllerConf &lon_controller_conf) {
//   const auto &control_table = lon_controller_conf.calibration_table();
//   AINFO << "Control calibration table loaded";
//   AINFO << "Control calibration table size is "
//         << control_table.calibration_size();
//   Interpolation2D::DataType xyz;
//   for (const auto &calibration : control_table.calibration()) {
//     xyz.push_back(std::make_tuple(calibration.speed(),
//                                   calibration.acceleration(),
//                                   calibration.command()));
//   }
//   control_interpolation_.reset(new Interpolation2D);
//   CHECK(control_interpolation_->Init(xyz))
//       << "Fail to load control calibration table";
// }

// Status LonController::ComputeControlCommand(
//     const localization::LocalizationEstimate *localization,
//     const canbus::Chassis *chassis,
//     const planning::ADCpath_point *planning_published_path_point,
//     control::ControlCommand *cmd) {
//   localization_ = localization;
//   chassis_ = chassis;

//   path_point_message_ = planning_published_path_point;
//   if (!control_interpolation_) {
//     AERROR << "Fail to initialize calibration table.";
//     return Status(ErrorCode::CONTROL_COMPUTE_ERROR,
//                   "Fail to initialize calibration table.");
//   }

//   if (path_point_analyzer_ == nullptr ||
//       path_point_analyzer_->seq_num() !=
//           path_point_message_->header().sequence_num()) {
//     path_point_analyzer_.reset(new path_pointAnalyzer(path_point_message_));
//   }
//   const LonControllerConf &lon_controller_conf =
//       control_conf_->lon_controller_conf();

//   auto debug = cmd->mutable_debug()->mutable_simple_lon_debug();
//   debug.Clear();

//   double brake_cmd = 0.0;
//   double throttle_cmd = 0.0;
//   double ts = lon_controller_conf.ts();
//   double preview_time = lon_controller_conf.preview_window() * ts;

//   if (preview_time < 0.0) {
//     const auto error_msg = common::util::StrCat(
//         "Preview time set as: ", preview_time, " less than 0");
//     AERROR << error_msg;
//     return Status(ErrorCode::CONTROL_COMPUTE_ERROR, error_msg);
//   }
//   ComputeLongitudinalErrors(path_point_analyzer_.get(), preview_time, debug);

//   double station_error_limit = lon_controller_conf.station_error_limit();
//   double station_error_limited = 0.0;
//   if (FLAGS_enable_speed_station_preview) {
//     station_error_limited =
//         common::math::Clamp(debug.preview_station_error(),
//                             -station_error_limit, station_error_limit);
//   } else {
//     station_error_limited = common::math::Clamp(
//         debug.station_error(), -station_error_limit, station_error_limit);
//   }
//   double speed_offset =
//       station_pid_controller_.Control(station_error_limited, ts);

//   double speed_controller_input = 0.0;
//   double speed_controller_input_limit =
//       lon_controller_conf.speed_controller_input_limit();
//   double speed_controller_input_limited = 0.0;
//   if (FLAGS_enable_speed_station_preview) {
//     speed_controller_input = speed_offset + debug.preview_speed_error();
//   } else {
//     speed_controller_input = speed_offset + debug.speed_error();
//   }
//   speed_controller_input_limited =
//       common::math::Clamp(speed_controller_input, -speed_controller_input_limit,
//                           speed_controller_input_limit);

//   double acceleration_cmd_closeloop = 0.0;
//   if (VehicleStateProvider::instance()->linear_velocityelocity() <=
//       lon_controller_conf.switch_speed()) {
//     speed_pid_controller_.SetPID(lon_controller_conf.low_speed_pid_conf());
//     acceleration_cmd_closeloop =
//         speed_pid_controller_.Control(speed_controller_input_limited, ts);
//   } else {
//     speed_pid_controller_.SetPID(lon_controller_conf.high_speed_pid_conf());
//     acceleration_cmd_closeloop =
//         speed_pid_controller_.Control(speed_controller_input_limited, ts);
//   }

//   double slope_offset_compenstaion = digital_filter_pitch_angle_.Filter(
//       GRA_ACC * std::sin(VehicleStateProvider::instance()->pitch()));

//   if (isnan(slope_offset_compenstaion)) {
//       slope_offset_compenstaion = 0;
//   }

//   debug.set_slope_offset_compensation(slope_offset_compenstaion);

//   double acceleration_cmd =
//       acceleration_cmd_closeloop + debug.preview_acceleration_reference() +
//       FLAGS_enable_slope_offset * debug.slope_offset_compensation();
//   debug.set_is_full_stop(false);
//   GetPathRemain(debug);

//   if ((path_point_message_->path_point_type() ==
//    apollo::planning::ADCpath_point::NORMAL) &&
//       ((std::fabs(debug.preview_acceleration_reference()) <=
//            FLAGS_max_acceleration_when_stopped &&
//        std::fabs(debug.preview_speed_reference()) <=
//            vehicle_param_.max_abs_speed_when_stopped()) ||
//       (debug.path_remain() < 0.3))) {
//     acceleration_cmd = lon_controller_conf.standstill_acceleration();
//     AINFO << "Stop location reached";
//     debug.set_is_full_stop(true);
//   }

//   double throttle_deadzone = lon_controller_conf.throttle_deadzone();
//   double brake_deadzone = lon_controller_conf.brake_deadzone();
//   double calibration_value = 0.0;
//   if (FLAGS_use_preview_speed_for_table) {
//     calibration_value = control_interpolation_->Interpolate(
//         std::make_pair(debug.preview_speed_reference(), acceleration_cmd));
//   } else {
//     calibration_value = control_interpolation_->Interpolate(
//         std::make_pair(chassis_->speed_mps(), acceleration_cmd));
//   }

//   if (calibration_value >= 0) {
//     throttle_cmd = calibration_value > throttle_deadzone ? calibration_value
//                                                          : throttle_deadzone;
//     brake_cmd = 0.0;
//   } else {
//     throttle_cmd = 0.0;
//     brake_cmd = -calibration_value > brake_deadzone ? -calibration_value
//                                                     : brake_deadzone;
//   }

//   debug.set_station_error_limited(station_error_limited);
//   debug.set_speed_controller_input_limited(speed_controller_input_limited);
//   debug.set_acceleration_cmd(acceleration_cmd);
//   debug.set_throttle_cmd(throttle_cmd);
//   debug.set_brake_cmd(brake_cmd);
//   debug.set_acceleration_lookup(acceleration_cmd);
//   debug.set_speed_lookup(chassis_->speed_mps());
//   debug.set_calibration_value(calibration_value);
//   debug.set_acceleration_cmd_closeloop(acceleration_cmd_closeloop);

//   if (FLAGS_enable_csv_debug && speed_log_file_ != nullptr) {
//     fprintf(speed_log_file_,
//             "%.6f, %.6f, %.6f, %.6f, %.6f, %.6f, %.6f, %.6f, %.6f, %.6f,"
//             "%.6f, %.6f, %.6f, %.6f, %.6f, %.6f, %.6f, %d,\r\n",
//             debug.station_reference(), debug.station_error(),
//             station_error_limited, debug.preview_station_error(),
//             debug.speed_reference(), debug.speed_error(),
//             speed_controller_input_limited, debug.preview_speed_reference(),
//             debug.preview_speed_error(),
//             debug.preview_acceleration_reference(), acceleration_cmd_closeloop,
//             acceleration_cmd, debug.acceleration_lookup(),
//             debug.speed_lookup(), calibration_value, throttle_cmd, brake_cmd,
//             debug.is_full_stop());
//   }

//   cmd->set_throttle(throttle_cmd);
//   cmd->set_brake(brake_cmd);

//   if (std::fabs(VehicleStateProvider::instance()->linear_velocityelocity()) <=
//           vehicle_param_.max_abs_speed_when_stopped() ||
//       chassis->gear_location() == path_point_message_->gear() ||
//       chassis->gear_location() == canbus::Chassis::GEAR_NEUTRAL) {
//     cmd->set_gear_location(path_point_message_->gear());
//   } else {
//     cmd->set_gear_location(chassis->gear_location());
//   }

//   return Status::OK();
// }

// Status LonController::Reset() {
//   speed_pid_controller_.Reset();
//   station_pid_controller_.Reset();
//   return Status::OK();
// }

// std::string LonController::Name() const { return name_; }



// void LonController::SetDigitalFilter(double ts, double cutoff_freq,
//                                      common::DigitalFilter *digital_filter) {
//   std::vector<double> denominators;
//   std::vector<double> numerators;
//   common::LpfCoefficients(ts, cutoff_freq, &denominators, &numerators);
//   digital_filter->set_coefficients(denominators, numerators);
// }

// void LonController::GetPathRemain(SimpleLongitudinalDebug *debug) {
//   int stop_index = 0;
//   while (stop_index < path_point_message_->path_point_point_size()) {
//     if (fabs(path_point_message_->path_point_point(stop_index).v()) < 1e-3 &&
//         path_point_message_->path_point_point(stop_index).a() > -0.01 &&
//         path_point_message_->path_point_point(stop_index).a() < 0.0) {
//       break;
//     } else {
//       ++stop_index;
//     }
//   }
//   if (stop_index == path_point_message_->path_point_point_size()) {
//     --stop_index;
//     if (fabs(path_point_message_->path_point_point(stop_index).v()) < 0.1) {
//       AINFO << "the last point is selected as parking point";
//     } else {
//       AINFO << "the last point found in path and speed > speed_deadzone";
//       debug.set_path_remain(10000);
//     }
//   }
//   debug.set_path_remain(
//       path_point_message_->path_point_point(stop_index).path_point().s() -
//       debug.current_station());
// }

// }  // namespace control
// }  // namespace apollo
