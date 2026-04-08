#include <flatland_plugins/battery.h>
#include <flatland_server/yaml_reader.h>
#include <pluginlib/class_list_macros.hpp>
#include <cmath>

using namespace flatland_server;

namespace flatland_plugins {

void Battery::OnInitialize(const YAML::Node &config) {
  YamlReader reader(node_, config);

  std::string body_name = reader.Get<std::string>("body");
  std::string topic = reader.Get<std::string>("topic", "battery_state");
  double pub_rate = reader.Get<double>("pub_rate", 1.0);

  capacity_ah_ = reader.Get<double>("capacity_ah", 5.0);
  voltage_full_ = reader.Get<double>("voltage_full", 12.6);
  voltage_empty_ = reader.Get<double>("voltage_empty", 10.0);
  base_current_ = reader.Get<double>("base_current", 0.5);
  linear_current_coeff_ = reader.Get<double>("linear_current_coeff", 2.0);
  angular_current_coeff_ = reader.Get<double>("angular_current_coeff", 0.5);
  double initial_charge = reader.Get<double>("initial_charge", 1.0);

  reader.EnsureAccessedAllKeys();

  body_ = GetModel()->GetBody(body_name);
  if (body_ == nullptr) {
    throw YAMLException("Body with name \"" + body_name + "\" does not exist");
  }

  charge_ah_ = capacity_ah_ * initial_charge;
  depleted_ = false;

  update_timer_.SetRate(pub_rate);

  pub_ = node_->create_publisher<sensor_msgs::msg::BatteryState>(
      GetModel()->NameSpaceTopic(topic), 1);
  marker_pub_ = node_->create_publisher<visualization_msgs::msg::Marker>(
      GetModel()->NameSpaceTopic("battery_marker"), 1);

  RCLCPP_INFO(node_->get_logger(),
              "Battery plugin: capacity=%.1fAh voltage=%.1f-%.1fV "
              "base_current=%.2fA lin_coeff=%.2f ang_coeff=%.2f",
              capacity_ah_, voltage_empty_, voltage_full_,
              base_current_, linear_current_coeff_, angular_current_coeff_);
}

void Battery::BeforePhysicsStep(const Timekeeper &timekeeper) {
  double dt = timekeeper.GetStepSize();

  if (!depleted_) {
    b2Body *physics = body_->physics_body_;
    b2Vec2 linear_vel = physics->GetLinearVelocityFromLocalPoint(b2Vec2(0, 0));
    float angular_vel = physics->GetAngularVelocity();

    double speed = std::sqrt(linear_vel.x * linear_vel.x +
                             linear_vel.y * linear_vel.y);
    double current_draw = base_current_ +
                          linear_current_coeff_ * speed +
                          angular_current_coeff_ * std::fabs(angular_vel);

    charge_ah_ -= current_draw * dt / 3600.0;

    if (charge_ah_ <= 0.0) {
      charge_ah_ = 0.0;
      depleted_ = true;
      RCLCPP_WARN(node_->get_logger(), "Battery depleted! Robot stopped.");
    }
  }

  // Stop robot when depleted
  if (depleted_) {
    b2Body *physics = body_->physics_body_;
    physics->SetLinearVelocity(b2Vec2(0, 0));
    physics->SetAngularVelocity(0);
  }

  // Publish at configured rate
  if (update_timer_.CheckUpdate(timekeeper)) {
    double percentage = charge_ah_ / capacity_ah_;
    double voltage = voltage_empty_ +
                     (voltage_full_ - voltage_empty_) * percentage;

    // Compute instantaneous current for the message
    double current = base_current_;
    if (!depleted_) {
      b2Body *physics = body_->physics_body_;
      b2Vec2 vel = physics->GetLinearVelocityFromLocalPoint(b2Vec2(0, 0));
      double speed = std::sqrt(vel.x * vel.x + vel.y * vel.y);
      current += linear_current_coeff_ * speed +
                 angular_current_coeff_ * std::fabs(physics->GetAngularVelocity());
    }

    sensor_msgs::msg::BatteryState msg;
    msg.header.stamp = timekeeper.GetSimTime();
    msg.header.frame_id = body_->name_;
    msg.voltage = static_cast<float>(voltage);
    msg.current = static_cast<float>(-current);  // negative = discharging
    msg.charge = static_cast<float>(charge_ah_);
    msg.capacity = static_cast<float>(capacity_ah_);
    msg.design_capacity = static_cast<float>(capacity_ah_);
    msg.percentage = static_cast<float>(percentage);
    msg.power_supply_status = depleted_
        ? sensor_msgs::msg::BatteryState::POWER_SUPPLY_STATUS_NOT_CHARGING
        : sensor_msgs::msg::BatteryState::POWER_SUPPLY_STATUS_DISCHARGING;
    msg.power_supply_health =
        sensor_msgs::msg::BatteryState::POWER_SUPPLY_HEALTH_GOOD;
    msg.power_supply_technology =
        sensor_msgs::msg::BatteryState::POWER_SUPPLY_TECHNOLOGY_LION;
    msg.present = true;

    pub_->publish(msg);

    // Publish text marker above robot showing battery info
    b2Body *physics = body_->physics_body_;
    b2Vec2 pos = physics->GetPosition();

    char text_buf[128];
    std::snprintf(text_buf, sizeof(text_buf),
                  "Batt: %.0f%%  %.1fV  %.2fA",
                  percentage * 100.0, voltage, -current);

    visualization_msgs::msg::Marker marker;
    marker.header.stamp = timekeeper.GetSimTime();
    marker.header.frame_id = "map";
    marker.ns = "battery";
    marker.id = 0;
    marker.type = visualization_msgs::msg::Marker::TEXT_VIEW_FACING;
    marker.action = visualization_msgs::msg::Marker::ADD;
    marker.pose.position.x = pos.x;
    marker.pose.position.y = pos.y;
    marker.pose.position.z = 1.0;
    marker.scale.z = 0.3;
    if (depleted_) {
      marker.color.r = 1.0; marker.color.g = 0.0; marker.color.b = 0.0;
    } else if (percentage < 0.2) {
      marker.color.r = 1.0; marker.color.g = 0.5; marker.color.b = 0.0;
    } else {
      marker.color.r = 0.2; marker.color.g = 1.0; marker.color.b = 0.2;
    }
    marker.color.a = 1.0;
    marker.text = text_buf;
    marker.lifetime = rclcpp::Duration::from_seconds(2.0);
    marker_pub_->publish(marker);
  }
}

}  // namespace flatland_plugins

PLUGINLIB_EXPORT_CLASS(flatland_plugins::Battery,
                       flatland_server::ModelPlugin)
