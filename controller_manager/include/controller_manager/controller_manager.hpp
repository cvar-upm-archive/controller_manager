/*!*******************************************************************************************
 *  \file       controller_manager.hpp
 *  \brief      Controller manager class definition
 *  \authors    Miguel Fernández Cortizas
 *              Pedro Arias Pérez
 *              David Pérez Saura
 *              Rafael Pérez Seguí
 *
 *  \copyright  Copyright (c) 2022 Universidad Politécnica de Madrid
 *              All Rights Reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ********************************************************************************/


#ifndef CONTROLLER_MANAGER_HPP
#define CONTROLLER_MANAGER_HPP

#include <as2_core/control_mode_utils/control_mode_utils.hpp>
#include <as2_core/node.hpp>
#include <as2_core/yaml_utils/yaml_utils.hpp>
#include <filesystem>
#include <chrono>
#include <pluginlib/class_loader.hpp>
#include <rclcpp/logging.hpp>

#include "controller_plugin_base/controller_base.hpp"
#include "as2_msgs/msg/controller_info.hpp"

class ControllerManager : public as2::Node
{
public:
  ControllerManager() : as2::Node("controller_manager")
  {
    this->declare_parameter<double>("publish_cmd_freq", 100.0);
    this->declare_parameter<double>("publish_info_freq", 10.0);
    try
    {
      this->declare_parameter<std::string>("plugin_name");
    }
    catch(const rclcpp::ParameterTypeException& e)
    {
      RCLCPP_FATAL(this->get_logger(), "Launch argument <plugin_name> not defined or malformed: %s", e.what());
      this->~ControllerManager();
    }
    this->declare_parameter<bool>("use_bypass", true); // DECLARED, READ ON PLUGIN_BASE
    this->declare_parameter<std::filesystem::path>("plugin_config_file", "");  // ONLY DECLARED, USED IN LAUNCH
    this->declare_parameter<std::filesystem::path>("plugin_available_modes_config_file", "");

    this->get_parameter("publish_cmd_freq", cmd_freq_);
    this->get_parameter("publish_info_freq", info_freq_);
    this->get_parameter("plugin_name", plugin_name_);
    plugin_name_ += "::Plugin";
    // this->get_parameter("plugin_config_file", parameter_string_);      
    this->get_parameter("plugin_available_modes_config_file", available_modes_config_file_);
    
    loader_ = std::make_shared<pluginlib::ClassLoader<controller_plugin_base::ControllerBase>>(
        "controller_plugin_base", "controller_plugin_base::ControllerBase");
    try
    {
      controller_ = loader_->createSharedInstance(plugin_name_);
      RCLCPP_INFO(this->get_logger(), "PLUGIN LOADED [%s]", plugin_name_.c_str());
    }
    catch (pluginlib::PluginlibException &ex)
    {
      RCLCPP_ERROR(this->get_logger(), "The plugin failed to load for some reason. Error: %s\n", ex.what());
    }

    controller_->initialize(this);
    if (available_modes_config_file_.empty()) {
      available_modes_config_file_ = loader_->getPluginManifestPath(plugin_name_);
    }
    RCLCPP_DEBUG(this->get_logger(), "MODES FILE LOADED: %s", available_modes_config_file_.parent_path().c_str());

    config_available_control_modes(available_modes_config_file_.parent_path());

    mode_pub_ = this->create_publisher<as2_msgs::msg::ControllerInfo>(
        as2_names::topics::controller::info,
        as2_names::topics::controller::qos_info);

    mode_timer_ = this->create_wall_timer(std::chrono::duration<double>(info_freq_), std::bind(&ControllerManager::mode_timer_callback, this));
  };

  ~ControllerManager() {};
private:
  // TODO: move to plugin base?
  void config_available_control_modes(const std::filesystem::path project_path)
  {
    auto available_input_modes = as2::parse_uint_from_string(
        as2::find_tag_from_project_exports_path<std::string>(project_path, "input_control_modes"));
    RCLCPP_INFO(this->get_logger(), "==========================================================");
    RCLCPP_INFO(this->get_logger(), "AVAILABLE INPUT MODES: ");
    for (auto mode : available_input_modes)
    {
      RCLCPP_INFO(this->get_logger(), "\t -%s", as2::controlModeToString(mode).c_str());
       
    }
    auto available_output_modes = as2::parse_uint_from_string(
        as2::find_tag_from_project_exports_path<std::string>(project_path, "output_control_modes"));
    RCLCPP_INFO(this->get_logger(), "AVAILABLE OUTPUT MODES: ");
    for (auto mode : available_output_modes)
    {
      RCLCPP_INFO(this->get_logger(), "\t -%s", as2::controlModeToString(mode).c_str());
    }

    RCLCPP_INFO(this->get_logger(), "==========================================================");

    controller_->setInputControlModesAvailables(available_input_modes);
    controller_->setOutputControlModesAvailables(available_output_modes);
  };

  void mode_timer_callback()
  {
    as2_msgs::msg::ControllerInfo msg;
    msg.header.stamp = this->now();
    msg.current_control_mode = controller_->getMode();
    mode_pub_->publish(msg);
  };

public:
  double cmd_freq_;

private:
  double info_freq_;
  std::filesystem::path plugin_name_;
  std::filesystem::path available_modes_config_file_;

  std::shared_ptr<pluginlib::ClassLoader<controller_plugin_base::ControllerBase>> loader_;
  std::shared_ptr<controller_plugin_base::ControllerBase> controller_;
  rclcpp::Publisher<as2_msgs::msg::ControllerInfo>::SharedPtr mode_pub_;
  rclcpp::TimerBase::SharedPtr mode_timer_;
};

#endif // CONTROLLER_MANAGER_HPP
