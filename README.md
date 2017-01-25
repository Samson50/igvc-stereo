# USMA IGVC Sterevision

### Installation Instructions
#### Install ZED SDK
1. Get TK1 version of ZED SKD from:
  * https://www.stereolabs.com/developers/release/1.1.1a/
2. cd {Download locaiton}
3. chmod 777 ZED*
4. ./ZED

#### Install ROS
1. sudo sh -c 'echo "deb http://packages.ros.org/ros/ubuntu trusty main" > /etc/apt/sources.list.d/ros-latest.list'
2. sudo apt-key adv --keyserver hkp://ha.pool.sks-keyservers.net --recv-key 0xB01FA116
3. sudo apt-get update
4. sudo apt-get install ros-indigo-ros-base
5. sudo apt-get install python-rosdep
6. sudo rosdep init
7. rosdep update
8. echo "source /opt/ros/indigo/setup.bash" >> ~/.bashrc
9. source ~/.bashrc
10. sudo apt-get install python-rosinstall

#### Install Rvz
1. sudo apt-get install ros-indigo-rviz
2. sudo apt-get install ros-indigo-robot-model
3. echo "unset GTK_IM_MODULE" >> ~/.bashrc
4. source ~/.bashrc

#### Install pcl
1. sudo add-apt-repository ppa:v-launchpad-jochen-sprickerhof-de/pcl
2. sudo apt-get update
3. sudo apt-get install libpcl-all

#### Install pcl-ros
1. sudo apt-get install ros-indigo-pcl-ros

#### Install zed-ros-wrapper
1. cd ~/[CATKIN_WOKRSPACE]/src
2. git clone https://github.com/stereolabs/zed-ros-wrapper 
3. cd ~/[CATKIN_WORKSPACE]
4. catkin_make zed-ros-wrapper
5. source ./devel/setup.bash

### ROS Networking
#### ON JETSON TK1:	
1. echo export ROS_MASTER_URI=http://[REMOTE_PC_IP]:11311 >> ~/.bashrc
2. echo export ROS_HOSTNAME=[JETSON_IP] >> ~/.bashrc
3. echo export ROS_MASTER_URI=http://[REMOTE_PC_IP]:11311 >> ~/[CATKIN_WORKSPACE]/devel/setup.sh
4. echo export ROS_HOSTNAME=[JETSON_IP] >> ~/[CATKIN_WORKSPACE]/devel/setup.sh
5. export ROS_IP=[JETSON_IP]

#### ON REMOTE PC
1. echo export ROS_MASTER_URI=http://localhost:11311 >> ~/.bashrc
2. echo export ROS_HOSTNAME=[PC_IP] >> ~/.bashrc