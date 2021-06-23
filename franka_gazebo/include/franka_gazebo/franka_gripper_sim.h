#pragma once

#include <actionlib/server/simple_action_server.h>
#include <control_msgs/GripperCommandAction.h>
#include <control_toolbox/pid.h>
#include <controller_interface/controller.h>
#include <franka_gripper/GraspAction.h>
#include <franka_gripper/GraspEpsilon.h>
#include <franka_gripper/HomingAction.h>
#include <franka_gripper/MoveAction.h>
#include <franka_gripper/StopAction.h>
#include <franka_hw/trigger_rate.h>
#include <hardware_interface/joint_command_interface.h>
#include <hardware_interface/robot_hw.h>
#include <realtime_tools/realtime_publisher.h>
#include <ros/time.h>
#include <sensor_msgs/JointState.h>
#include <functional>
#include <mutex>

namespace franka_gazebo {

const double kMaxFingerWidth = 0.08;

/**
 * Simulate the franka_gripper_node.
 *
 * Internally this done via ROS control. This controller assumes there are two finger joints in the
 * URDF which can be effort (force) controlled. It simulates the behavior of the real franka_gripper
 * by offering the same actions:
 *
 * - homing:  Execute a homing motion, i.e.e open and close the gripper fully. This is only
 *            simulated, though, and has no effect on the other actions.
 * - move:    Move the gripper with a desired velocity to a certain width.
 * - grasp:   Close the gripper until it stops because of a contact. If then the gripper width is
 *            within a user specified range a certain force is applied
 * - stop:    Stop any previous motion, or the excertion of forces on currently grasped objects
 * - gripper_action: A standard gripper action recognized by MoveIt!
 *
 * NOTE: The `grasp` action has a bug, that it will not succeed nor abort if the target width
 *       lets the fingers open. This is because of missing the joint limits interface which
 *       lets the finger oscillate at their limits.
 */
class FrankaGripperSim
    : public controller_interface::Controller<hardware_interface::EffortJointInterface> {
 public:
  enum State {
    IDLE,      ///< Gripper is not actively controlled, but tracks the other finger to simulate a
               ///< mimicked joint
    HOLDING,   ///< Gripper is holding position and tracking zero velocity while mainting a desired
               ///< force
    MOVING,    ///< Gripper is tracking a desired position and velocity
    GRASPING,  ///< Gripper is tracking a desired position and velocity. On contact it switches to
               ///< `HOLDING` if inside the epsilon of the desired grasping width otherwise back to
               ///< `IDLE`
    HOMING     ///< Gripper opens fully and then closes again.
  };

  bool init(hardware_interface::EffortJointInterface* hw, ros::NodeHandle& nh) override;
  void starting(const ros::Time&) override;
  void update(const ros::Time& now, const ros::Duration& period) override;

 private:
  State state_ = State::IDLE;

  franka_hw::TriggerRate rate_trigger_{30.0};
  control_toolbox::Pid pid1_;
  control_toolbox::Pid pid2_;
  realtime_tools::RealtimePublisher<sensor_msgs::JointState> pub_;
  hardware_interface::JointHandle finger1_;
  hardware_interface::JointHandle finger2_;

  std::mutex mutex_;

  // Configurable by action goals
  double width_desired_;
  double speed_desired_;
  double force_desired_;
  franka_gripper::GraspEpsilon tolerance_;

  // Configurable by parameters
  int speed_samples_;
  double speed_threshold_;
  double speed_default_;
  double tolerance_move_;
  double tolerance_gripper_action_;

  std::unique_ptr<actionlib::SimpleActionServer<franka_gripper::StopAction>> action_stop_;
  std::unique_ptr<actionlib::SimpleActionServer<franka_gripper::HomingAction>> action_homing_;
  std::unique_ptr<actionlib::SimpleActionServer<franka_gripper::MoveAction>> action_move_;
  std::unique_ptr<actionlib::SimpleActionServer<franka_gripper::GraspAction>> action_grasp_;
  std::unique_ptr<actionlib::SimpleActionServer<control_msgs::GripperCommandAction>> action_gc_;

  double control(hardware_interface::JointHandle& joint,
                 control_toolbox::Pid&,
                 double q_d,
                 double dq_d,
                 double f_d,
                 const ros::Duration& period);

  void interrupt(const std::string& message, const State& except);

  void waitUntil(const State& state);
};
}  // namespace franka_gazebo
