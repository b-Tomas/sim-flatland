from setuptools import find_packages, setup

package_name = 'republisher'

setup(
    name=package_name,
    version='0.1.0',
    packages=find_packages(exclude=['test']),
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='OpenRobOps',
    maintainer_email='noreply@openrobops.local',
    description='Republishes selected simulation data as key=value strings on /inorbit/custom_data.',
    license='Apache-2.0',
    entry_points={
        'console_scripts': [
            'republisher = republisher.republisher_node:main',
        ],
    },
)
