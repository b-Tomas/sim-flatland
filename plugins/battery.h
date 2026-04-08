#ifndef FLATLAND_PLUGINS_BATTERY_H
#define FLATLAND_PLUGINS_BATTERY_H

#include <flatland_server/model_plugin.h>
#include <flatland_server/timekeeper.h>
#include <flatland_plugins/update_timer.h>
#include <sensor_msgs/msg/battery_state.hpp>
#include <visualization_msgs/msg/marker.hpp>
#include <visualization_msgs/msg/marker_array.hpp>
#include <std_srvs/srv/set_bool.hpp>
#include <std_srvs/srv/trigger.hpp>
#include <rclcpp/rclcpp.hpp>
#include <vector>

namespace flatland_plugins {

struct ChargingZone {
  double x, y, radius;
  std::string name;
};

class Battery : public flatland_server::ModelPlugin {
 public:
  void OnInitialize(const YAML::Node &config) override;
  void BeforePhysicsStep(const flatland_server::Timekeeper &timekeeper) override;

 private:
  bool IsInChargingZone(double rx, double ry) const;

  flatland_server::Body *body_;
  UpdateTimer update_timer_;
  rclcpp::Publisher<sensor_msgs::msg::BatteryState>::SharedPtr pub_;
  rclcpp::Publisher<visualization_msgs::msg::Marker>::SharedPtr marker_pub_;
  rclcpp::Publisher<visualization_msgs::msg::MarkerArray>::SharedPtr zone_marker_pub_;

  rclcpp::Service<std_srvs::srv::SetBool>::SharedPtr set_charging_srv_;
  rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr reset_battery_srv_;

  double capacity_ah_;
  double voltage_full_;
  double voltage_empty_;
  double base_current_;
  double linear_current_coeff_;
  double angular_current_coeff_;
  double charge_current_;

  double charge_ah_;
  bool depleted_;
  bool charging_override_;  // manual charging via service
  bool in_charging_zone_;   // auto charging via zone

  std::vector<ChargingZone> charging_zones_;
  bool zones_published_;
};

}  // namespace flatland_plugins

#endif
