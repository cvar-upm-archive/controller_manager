#ifndef TAKE_OFF_BEHAVIOUR_HPP
#define TAKE_OFF_BEHAVIOUR_HPP

#include <as2_core/control_mode_utils/control_mode_utils.hpp>
#include <as2_core/node.hpp>
#include <as2_core/yaml_utils/yaml_utils.hpp>
#include <as2_msgs/action/take_off.hpp>
#include <filesystem>
#include <pluginlib/class_loader.hpp>

#include "as2_core/names/actions.hpp"
#include "as2_core/names/topics.hpp"
#include "controller_plugin_base/controller_base.hpp"

// #define PLUGIN_NAME "controller_plugin_differential_flatness::PDController"
#define PLUGIN_NAME "controller_plugin_differential_flatness::PDController"

class ControllerManager : public as2::Node {
  public:
  ControllerManager() : as2::Node("controller_manager") {
    loader_ = std::make_shared<pluginlib::ClassLoader<controller_plugin_base::ControllerBase>>(
        "controller_plugin_base", "controller_plugin_base::ControllerBase");
    try {
      controller_ = loader_->createSharedInstance(PLUGIN_NAME);
      RCLCPP_INFO(this->get_logger(), "PLUGIN LOADED");

    } catch (pluginlib::PluginlibException& ex) {
      printf("The plugin failed to load for some reason. Error: %s\n", ex.what());
    }

    controller_->initialize(this);
    std::filesystem::path manifest_path = loader_->getPluginManifestPath(PLUGIN_NAME);
    config_available_control_modes(manifest_path.parent_path());
  };

  private:
  void config_available_control_modes(const std::filesystem::path project_path) {
    auto available_input_modes = as2::parse_uint_from_string(
        as2::find_tag_from_project_exports_path<std::string>(project_path, "input_control_modes"));
    auto available_output_modes = as2::parse_uint_from_string(
        as2::find_tag_from_project_exports_path<std::string>(project_path, "output_control_modes"));
    controller_->setInputControlModesAvailables(available_input_modes);
    controller_->setOutputControlModesAvailables(available_output_modes);
  };

  std::shared_ptr<pluginlib::ClassLoader<controller_plugin_base::ControllerBase>> loader_;
  std::shared_ptr<controller_plugin_base::ControllerBase> controller_;
};

#endif  // TAKE_OFF_BEHAVIOUR_HPP
