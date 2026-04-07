import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    IncludeLaunchDescription,
    GroupAction,
)
from launch.conditions import IfCondition
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node, LifecycleNode
from launch_ros.event_handlers import OnStateTransition
from nav2_common.launch import RewrittenYaml


def generate_launch_description():
    # Directories
    bringup_dir = '/ros2_ws/src/flatland_nav2_bringup'
    nav2_bringup_dir = get_package_share_directory('nav2_bringup')

    # Launch arguments
    use_rviz_arg = DeclareLaunchArgument(
        'use_rviz', default_value='true',
        description='Whether to launch rviz2')
    use_sim_time_arg = DeclareLaunchArgument(
        'use_sim_time', default_value='true',
        description='Use simulation time')
    world_path_arg = DeclareLaunchArgument(
        'world_path', default_value='/ros2_ws/worlds/sample.world.yaml',
        description='Path to flatland world file')
    map_path_arg = DeclareLaunchArgument(
        'map_path', default_value='/ros2_ws/maps/sample_map.yaml',
        description='Path to map yaml file')
    nav2_params_arg = DeclareLaunchArgument(
        'nav2_params_file',
        default_value=os.path.join(bringup_dir, 'config', 'nav2_params.yaml'),
        description='Path to Nav2 parameters file')

    use_rviz = LaunchConfiguration('use_rviz')
    use_sim_time = LaunchConfiguration('use_sim_time')
    world_path = LaunchConfiguration('world_path')
    map_path = LaunchConfiguration('map_path')
    nav2_params_file = LaunchConfiguration('nav2_params_file')

    # Rewrite params to inject use_sim_time
    configured_params = RewrittenYaml(
        source_file=nav2_params_file,
        param_rewrites={'use_sim_time': use_sim_time},
        convert_types=True,
    )

    # Flatland server node
    flatland_server = Node(
        package='flatland_server',
        executable='flatland_server',
        name='flatland_server',
        output='screen',
        parameters=[{
            'world_path': world_path,
            'update_rate': 200.0,
            'step_size': 0.005,
            'show_viz': 0.0,
            'viz_pub_rate': 30.0,
            'use_sim_time': False,  # flatland is the time source
        }],
    )

    # Map server (lifecycle node)
    map_server = Node(
        package='nav2_map_server',
        executable='map_server',
        name='map_server',
        output='screen',
        parameters=[configured_params, {'yaml_filename': map_path}],
    )

    # AMCL for localization
    amcl = Node(
        package='nav2_amcl',
        executable='amcl',
        name='amcl',
        output='screen',
        parameters=[configured_params],
    )

    # Nav2 navigation launch (planner, controller, bt_navigator, etc.)
    nav2_navigation = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(nav2_bringup_dir, 'launch', 'navigation_launch.py')
        ),
        launch_arguments={
            'use_sim_time': use_sim_time,
            'params_file': nav2_params_file,
        }.items(),
    )

    # Lifecycle manager for map_server and amcl
    lifecycle_manager_localization = Node(
        package='nav2_lifecycle_manager',
        executable='lifecycle_manager',
        name='lifecycle_manager_localization',
        output='screen',
        parameters=[{
            'use_sim_time': use_sim_time,
            'autostart': True,
            'node_names': ['map_server', 'amcl'],
        }],
    )

    # Rviz2 (conditional)
    rviz_config = os.path.join(bringup_dir, 'config', 'flatland_rviz.rviz')
    rviz_node = Node(
        condition=IfCondition(use_rviz),
        package='rviz2',
        executable='rviz2',
        name='rviz2',
        output='screen',
        arguments=['-d', rviz_config],
        parameters=[{'use_sim_time': use_sim_time}],
    )

    return LaunchDescription([
        use_rviz_arg,
        use_sim_time_arg,
        world_path_arg,
        map_path_arg,
        nav2_params_arg,
        flatland_server,
        map_server,
        amcl,
        lifecycle_manager_localization,
        nav2_navigation,
        rviz_node,
    ])
