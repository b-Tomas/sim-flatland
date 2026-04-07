FROM osrf/ros:jazzy-desktop

ENV DEBIAN_FRONTEND=noninteractive
SHELL ["/bin/bash", "-c"]

# System + ROS apt dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    ros-jazzy-navigation2 \
    ros-jazzy-nav2-bringup \
    ros-jazzy-slam-toolbox \
    cmake \
    libopencv-dev \
    liblua5.1-0-dev \
    libboost-filesystem-dev \
    libboost-date-time-dev \
    libboost-system-dev \
    python3-colcon-common-extensions \
    git \
    wget \
    qt6-wayland \
    qtwayland5 \
    && rm -rf /var/lib/apt/lists/*

# Build Box2D 2.3.1 from source (Ubuntu 24.04's libbox2d-dev lacks CMake config)
RUN cd /tmp && \
    git clone --branch v2.3.1 --depth 1 https://github.com/erincatto/box2d.git && \
    cd box2d/Box2D/Build && \
    cmake -DBOX2D_BUILD_EXAMPLES=OFF -DBOX2D_BUILD_TESTS=OFF \
      -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
      -DCMAKE_INSTALL_PREFIX=/usr/local .. && \
    make -j"$(nproc)" && make install && \
    rm -rf /tmp/box2d

# Clone flatland ros2 branch
RUN mkdir -p /ros2_ws/src && \
    cd /ros2_ws/src && \
    git clone --branch ros2 --depth 1 https://github.com/avidbots/flatland.git

# Patch flatland source for ROS2 Jazzy / Ubuntu 24.04 compatibility
RUN cd /ros2_ws/src/flatland && \
    # Fix missing <cstdint> include (C++17 no longer implicitly includes it)
    sed -i '1i #include <cstdint>' flatland_server/include/flatland_server/collision_filter_registry.h && \
    # Fix missing <array> include in types.h
    sed -i '/#include <Box2D\/Box2D.h>/a #include <array>' flatland_server/include/flatland_server/types.h && \
    # Fix tf2_geometry_msgs header path (.h -> .hpp for ROS2)
    find . -name '*.cpp' -o -name '*.h' | xargs sed -i \
      's|tf2_geometry_msgs/tf2_geometry_msgs\.h|tf2_geometry_msgs/tf2_geometry_msgs.hpp|g' && \
    # Fix deprecated OpenCV constant (CV_LOAD_IMAGE_GRAYSCALE -> cv::IMREAD_GRAYSCALE)
    find . -name '*.cpp' | xargs sed -i \
      's|CV_LOAD_IMAGE_GRAYSCALE|cv::IMREAD_GRAYSCALE|g' && \
    # Move ament_package() to end of file (must come after all install() calls)
    sed -i '/^ament_package()/d' flatland_server/CMakeLists.txt && \
    echo 'ament_package()' >> flatland_server/CMakeLists.txt && \
    # Remove broken target export (ament_export_libraries already handles it)
    sed -i '/ament_export_interfaces\|ament_export_targets/d' flatland_server/CMakeLists.txt && \
    sed -i 's|EXPORT export_flatland_lib||g' flatland_server/CMakeLists.txt && \
    # Fix declare_parameter() calls that need default values in Jazzy
    sed -i \
      's|declare_parameter("world_path")|declare_parameter<std::string>("world_path", "")|; \
       s|declare_parameter("update_rate")|declare_parameter<double>("update_rate", 200.0)|; \
       s|declare_parameter("step_size")|declare_parameter<double>("step_size", 0.005)|; \
       s|declare_parameter("show_viz")|declare_parameter<double>("show_viz", 0.0)|; \
       s|declare_parameter("viz_pub_rate")|declare_parameter<double>("viz_pub_rate", 30.0)|' \
      flatland_server/src/flatland_server_node.cpp && \
    # Add missing dependencies to flatland_plugins CMakeLists.txt
    sed -i '/find_package(flatland_server REQUIRED)/a \
find_package(sensor_msgs REQUIRED)\nfind_package(visualization_msgs REQUIRED)\nfind_package(interactive_markers REQUIRED)\nfind_package(OpenCV REQUIRED)' \
      flatland_plugins/CMakeLists.txt && \
    sed -i 's|ament_target_dependencies(flatland_plugins_lib rclcpp flatland_server|ament_target_dependencies(flatland_plugins_lib rclcpp flatland_server sensor_msgs visualization_msgs interactive_markers|' \
      flatland_plugins/CMakeLists.txt && \
    sed -i '/target_link_libraries(flatland_plugins_lib/a \  ${OpenCV_LIBRARIES}' \
      flatland_plugins/CMakeLists.txt && \
    sed -i '/\${Eigen3_INCLUDE_DIRS}/a \  ${OpenCV_INCLUDE_DIRS}' \
      flatland_plugins/CMakeLists.txt && \
    # Fix rclcpp::Duration(int) -> rclcpp::Duration::from_seconds() in Jazzy
    find . -name '*.cpp' | xargs sed -i \
      's|rclcpp::Duration(\([0-9]*\))|rclcpp::Duration::from_seconds(\1)|g' && \
    # Remove -Wpedantic from plugins to avoid format string warnings being fatal
    sed -i 's|-Wpedantic|-Wpedantic -Wno-error=format|' flatland_plugins/CMakeLists.txt && \
    # Also fix ament_export_interfaces in plugins
    sed -i 's|ament_export_interfaces|ament_export_targets|' flatland_plugins/CMakeLists.txt && \
    # Fix pluginlib registration: base package must be "flatland_server" not "flatland_plugins_lib"
    sed -i 's|pluginlib_export_plugin_description_file(flatland_plugins_lib|pluginlib_export_plugin_description_file(flatland_server|' \
      flatland_plugins/CMakeLists.txt && \
    # Move ament_package() to end of file in plugins (must come after pluginlib_export)
    sed -i '/^ament_package()/d' flatland_plugins/CMakeLists.txt && \
    echo 'ament_package()' >> flatland_plugins/CMakeLists.txt && \
    # Fix plugin_description.xml library path for Jazzy pluginlib
    sed -i 's|path="lib/libflatland_plugins_lib"|path="flatland_plugins_lib"|' \
      flatland_plugins/plugin_description.xml && \
    # Fix null pointer crash: initialize twist_msg_ to avoid segfault before first message
    sed -i 's|geometry_msgs::msg::Twist::SharedPtr twist_msg_;|geometry_msgs::msg::Twist::SharedPtr twist_msg_ = std::make_shared<geometry_msgs::msg::Twist>();|' \
      flatland_plugins/include/flatland_plugins/diff_drive.h

# Install rosdep dependencies and build flatland
# Skip flatland_viz (rviz2 alone handles visualization)
RUN cd /ros2_ws && \
    . /opt/ros/jazzy/setup.sh && \
    (rosdep install --from-paths src --ignore-src -r -y || true) && \
    colcon build --symlink-install \
      --cmake-args -DCMAKE_BUILD_TYPE=Release \
      --packages-select flatland_msgs flatland_server flatland_plugins

# Copy project files
COPY launch/ /ros2_ws/src/flatland_nav2_bringup/launch/
COPY config/ /ros2_ws/src/flatland_nav2_bringup/config/
COPY maps/ /ros2_ws/maps/
COPY worlds/ /ros2_ws/worlds/
COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

WORKDIR /ros2_ws

# Display environment for X11/Wayland forwarding
ENV DISPLAY=:0
ENV QT_X11_NO_MITSHM=1

ENTRYPOINT ["/entrypoint.sh"]
CMD ["--with-rviz"]
