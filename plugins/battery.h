#ifndef FLATLAND_PLUGINS_BATTERY_H
#define FLATLAND_PLUGINS_BATTERY_H

#include <flatland_server/model_plugin.h>
#include <flatland_server/timekeeper.h>
#include <flatland_plugins/update_timer.h>
#include <sensor_msgs/msg/battery_state.hpp>
#include <visualization_msgs/msg/marker.hpp>
#include <rclcpp/rclcpp.hpp>

namespace flatland_plugins {

class Battery : public flatland_server::ModelPlugin {
 public:
  void OnInitialize(const YAML::Node &config) override;
  void BeforePhysicsStep(const flatland_server::Timekeeper &timekeeper) override;

 private:
  flatland_server::Body *body_;
  UpdateTimer update_timer_;
  rclcpp::Publisher<sensor_msgs::msg::BatteryState>::SharedPtr pub_;
  rclcpp::Publisher<visualization_msgs::msg::Marker>::SharedPtr marker_pub_;

  double capacity_ah_;
  double voltage_full_;
  double voltage_empty_;
  double base_current_;
  double linear_current_coeff_;
  double angular_current_coeff_;

  double charge_ah_;
  bool depleted_;
};

}  // namespace flatland_plugins

#endif
