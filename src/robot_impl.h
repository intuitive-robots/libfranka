// Copyright (c) 2017 Franka Emika GmbH
// Use of this source code is governed by the Apache-2.0 license, see LICENSE
#pragma once

#include <chrono>
#include <memory>
#include <type_traits>

#include <franka/model.h>
#include <franka/robot.h>
#include <research_interface/robot/rbk_types.h>
#include <research_interface/robot/service_traits.h>
#include <research_interface/robot/service_types.h>

#include "logger.h"
#include "network.h"
#include "robot_control.h"

namespace franka {

RobotState convertRobotState(const research_interface::robot::RobotState& robot_state) noexcept;

class Robot::Impl : public RobotControl {
 public:
  explicit Impl(std::unique_ptr<Network> network,
                size_t log_size,
                RealtimeConfig realtime_config = RealtimeConfig::kEnforce);

  RobotState update(
      const research_interface::robot::MotionGeneratorCommand* motion_command = nullptr,
      const research_interface::robot::ControllerCommand* control_command = nullptr) override;

  void throwOnMotionError(const RobotState& robot_state, uint32_t motion_id) override;

  RobotState readOnce();

  ServerVersion serverVersion() const noexcept;
  RealtimeConfig realtimeConfig() const noexcept override;

  uint32_t startMotion(
      research_interface::robot::Move::ControllerMode controller_mode,
      research_interface::robot::Move::MotionGeneratorMode motion_generator_mode,
      const research_interface::robot::Move::Deviation& maximum_path_deviation,
      const research_interface::robot::Move::Deviation& maximum_goal_pose_deviation) override;
  void cancelMotion(uint32_t motion_id) override;
  void finishMotion(uint32_t motion_id,
                    const research_interface::robot::MotionGeneratorCommand* motion_command,
                    const research_interface::robot::ControllerCommand* control_command) override;

  template <typename T, typename... TArgs>
  uint32_t executeCommand(TArgs... /* args */);

  Model loadModel() const;

 protected:
  bool motionGeneratorRunning() const noexcept;
  bool controllerRunning() const noexcept;

 private:
  template <typename T>
  void handleCommandResponse(const typename T::Response& response) const;
  template <typename T>
  void handleBaseCommandResponse(const typename T::Response& response,
                                 std::true_type /* is_setter */) const;
  template <typename T>
  void handleBaseCommandResponse(const typename T::Response& response,
                                 std::false_type /* is_setter */) const;

  research_interface::robot::RobotCommand sendRobotCommand(
      const research_interface::robot::MotionGeneratorCommand* motion_command,
      const research_interface::robot::ControllerCommand* control_command) const;
  research_interface::robot::RobotState receiveRobotState();
  void updateState(const research_interface::robot::RobotState& robot_state);

  std::unique_ptr<Network> network_;

  Logger logger_;

  const RealtimeConfig realtime_config_;
  uint16_t ri_version_;

  research_interface::robot::MotionGeneratorMode motion_generator_mode_;
  research_interface::robot::ControllerMode controller_mode_;
  uint64_t message_id_;
};

template <typename T>
void Robot::Impl::handleCommandResponse(const typename T::Response& response) const {
  using is_setter =
      std::is_base_of<research_interface::robot::SetterCommandBase<T, T::kCommand>, T>;
  handleBaseCommandResponse<T>(response, typename is_setter::type());
}

template <typename T>
void Robot::Impl::handleBaseCommandResponse(const typename T::Response& response,
                                            std::true_type /* is_setter */) const {
  using namespace std::string_literals;  // NOLINT (google-build-using-namespace)

  switch (response.status) {
    case T::Status::kSuccess:
      break;
    case T::Status::kCommandNotPossibleRejected:
      throw CommandException("libfranka: "s + research_interface::robot::CommandTraits<T>::kName +
                             " command rejected: command not possible in the current mode!");
    case T::Status::kInvalidArgumentRejected:
      throw CommandException("libfranka: "s + research_interface::robot::CommandTraits<T>::kName +
                             " command rejected: invalid argument!");
    default:
      throw ProtocolException("libfranka: Unexpected response while handling "s +
                              research_interface::robot::CommandTraits<T>::kName + " command!");
  }
}

template <typename T>
void Robot::Impl::handleBaseCommandResponse(const typename T::Response& response,
                                            std::false_type /* is_setter */) const {
  using namespace std::string_literals;  // NOLINT (google-build-using-namespace)

  switch (response.status) {
    case T::Status::kSuccess:
      break;
    case T::Status::kCommandNotPossibleRejected:
      throw CommandException("libfranka: "s + research_interface::robot::CommandTraits<T>::kName +
                             " command rejected: command not possible in the current mode!");
    default:
      throw ProtocolException("libfranka: Unexpected response while handling "s +
                              research_interface::robot::CommandTraits<T>::kName + " command!");
  }
}

template <>
inline void Robot::Impl::handleCommandResponse<research_interface::robot::Move>(
    const research_interface::robot::Move::Response& response) const {
  using namespace std::string_literals;  // NOLINT (google-build-using-namespace)

  switch (response.status) {
    case research_interface::robot::Move::Status::kSuccess:
      break;
    case research_interface::robot::Move::Status::kMotionStarted:
      if (motionGeneratorRunning()) {
        throw ProtocolException(
            "libfranka: "s +
            research_interface::robot::CommandTraits<research_interface::robot::Move>::kName +
            " received unexpected motion started message.");
      }
      break;
    case research_interface::robot::Move::Status::kEmergencyAborted:
      throw CommandException(
          "libfranka: "s +
          research_interface::robot::CommandTraits<research_interface::robot::Move>::kName +
          " command aborted: User Stop pressed!");
    case research_interface::robot::Move::Status::kReflexAborted:
      throw CommandException(
          "libfranka: "s +
          research_interface::robot::CommandTraits<research_interface::robot::Move>::kName +
          " command aborted: motion aborted by reflex!");
    case research_interface::robot::Move::Status::kInputErrorAborted:
      throw CommandException(
          "libfranka: "s +
          research_interface::robot::CommandTraits<research_interface::robot::Move>::kName +
          " command aborted: invalid input provided!");
    case research_interface::robot::Move::Status::kCommandNotPossibleRejected:
      throw CommandException(
          "libfranka: "s +
          research_interface::robot::CommandTraits<research_interface::robot::Move>::kName +
          " command rejected: command not possible in the current mode!");
    case research_interface::robot::Move::Status::kStartAtSingularPoseRejected:
      throw CommandException(
          "libfranka: "s +
          research_interface::robot::CommandTraits<research_interface::robot::Move>::kName +
          " command rejected: cannot start at singular pose!");
    case research_interface::robot::Move::Status::kOutOfRangeRejected:
      throw CommandException(
          "libfranka: "s +
          research_interface::robot::CommandTraits<research_interface::robot::Move>::kName +
          " command rejected: maximum path deviation out of range!");
    case research_interface::robot::Move::Status::kPreempted:
      throw CommandException(
          "libfranka: "s +
          research_interface::robot::CommandTraits<research_interface::robot::Move>::kName +
          " command preempted!");
    default:
      throw ProtocolException(
          "libfranka: Unexpected response while handling "s +
          research_interface::robot::CommandTraits<research_interface::robot::Move>::kName +
          " command!");
  }
}

template <typename T, typename... TArgs>
uint32_t Robot::Impl::executeCommand(TArgs... args) {
  uint32_t command_id = network_->tcpSendRequest<T>(args...);
  typename T::Response response = network_->tcpBlockingReceiveResponse<T>(command_id);
  handleCommandResponse<T>(response);
  return command_id;
}

template <>
inline uint32_t Robot::Impl::executeCommand<research_interface::robot::GetCartesianLimit,
                                            int32_t,
                                            VirtualWallCuboid*>(
    int32_t id,
    VirtualWallCuboid* virtual_wall_cuboid) {
  using research_interface::robot::GetCartesianLimit;
  uint32_t command_id = network_->tcpSendRequest<GetCartesianLimit>(id);
  GetCartesianLimit::Response response =
      network_->tcpBlockingReceiveResponse<GetCartesianLimit>(command_id);

  virtual_wall_cuboid->p_frame = response.object_frame;
  virtual_wall_cuboid->p_max = response.object_p_max;
  virtual_wall_cuboid->p_min = response.object_p_min;
  virtual_wall_cuboid->active = response.object_activation;
  virtual_wall_cuboid->id = id;

  handleCommandResponse<GetCartesianLimit>(response);
  return command_id;
}

}  // namespace franka
