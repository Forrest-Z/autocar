#ifndef LON_CONTROLLER_H_
#define LON_CONTROLLER_H_
#include "controller.h"
#include "pid_controller.h"
#include "car_msgs/control_cmd.h"
#include "car_msgs/chassis.h"
#include "car_msgs/localization.h"
#include "car_msgs/trajectory.h"
#include "car_msgs/trajectory_point.h"
#include "car_msgs/param.h"
#include "control/common/trajectory_analyzer.h"

namespace control {

class LonControllerConf{
    public:
        LonControllerConf(){};
        virtual ~LonControllerConf(){};

        double ts;
        PidConf station_pid_conf;
        PidConf speed_pid_conf;
};

class LonController {
    public:
        LonController();
        void Init(const LonControllerConf *control_conf);

        /**
         * @brief compute brake / throttle values based on current vehicle status
         *        and target trajectory_path
         * @param localization vehicle location
         * @param chassis vehicle status e.g., speed, acceleration
         * @param trajectory_path trajectory_path generated by planning
         * @param cmd control command
         * @return Status computation status
         */
        void ComputeControlCommand(
                                const car_msgs::trajectory &planning_published_trajectory,
                                const car_msgs::vehicle_state &vehicle_state,
                                car_msgs::control_cmd &control_cmd,
                                car_msgs::lon_debug &debug);
                                
        void ComputeLongitudinalErrors(
                                        const double x, 
                                        const double y, 
                                        const double theta,
                                        const double linear_velocity, 
                                        const TrajectoryAnalyzer &trajectory_analyzer,
                                        car_msgs::lon_debug &debug);
        /**
         * @brief reset longitudinal controller
         * @return Status reset status
         */
        void UpdateParam(const PidConf &station_pid_conf,const PidConf &speed_pid_conf);
        bool Reset();

        /**
         * @brief stop longitudinal controller
         */
        void Stop();

    protected:
        const std::string name_;
        double ts_;
        TrajectoryAnalyzer trajectory_analyzer_;
        PIDController station_pid_controller_;
        PIDController speed_pid_controller_;
};
}//namespace control
#endif  // LON_CONTROLLER_H_