import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, EmitEvent, RegisterEventHandler
from launch.events import matches_action
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import LifecycleNode
from launch_ros.event_handlers import OnStateTransition
from launch_ros.events.lifecycle import ChangeState
from lifecycle_msgs.msg import Transition


def generate_launch_description():
    package_share = get_package_share_directory('cuda_tracker')
    default_params_file = os.path.join(package_share, 'config', 'tracker_params.yaml')

    params_file = LaunchConfiguration('params_file')

    tracker_node = LifecycleNode(
        package='cuda_tracker',
        executable='cuda_tracker_node',
        name='tracker_node',
        namespace='',
        output='screen',
        parameters=[params_file],
    )

    configure_on_start = EmitEvent(
        event=ChangeState(
            lifecycle_node_matcher=matches_action(tracker_node),
            transition_id=Transition.TRANSITION_CONFIGURE,
        )
    )

    activate_on_inactive = RegisterEventHandler(
        OnStateTransition(
            target_lifecycle_node=tracker_node,
            goal_state='inactive',
            entities=[
                EmitEvent(
                    event=ChangeState(
                        lifecycle_node_matcher=matches_action(tracker_node),
                        transition_id=Transition.TRANSITION_ACTIVATE,
                    )
                )
            ],
        )
    )

    return LaunchDescription([
        # The only argument: every other setting lives in the YAML, so a different
        # engine or input source is a copy of that file.
        DeclareLaunchArgument(
            'params_file',
            default_value=default_params_file,
            description='Full path to the parameter YAML file.',
        ),
        activate_on_inactive,
        tracker_node,
        configure_on_start,
    ])
