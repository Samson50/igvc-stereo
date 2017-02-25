///////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2016, STEREOLABS.
//
// All rights reserved.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
///////////////////////////////////////////////////////////////////////////


/************************************************************************************
 ** This sample demonstrates how to use PCL (Point Cloud Library) with the ZED SDK **
 ************************************************************************************/

#include <stdio.h>
#include <string.h>
#include <ctime>
#include <chrono>
#include <thread>
#include <mutex>

#include <ros/ros.h>
#include <opencv2/opencv.hpp>

// 2016 Pipeline includes
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/calib3d/calib3d.hpp>
#include <image_transport/image_transport.h>
#include <cv_bridge/cv_bridge.h>
#include <sensor_msgs/image_encodings.h>
#include "whiteline_filter.h"
#include "rbflag_filter.h"

#include <zed/Camera.hpp>
#include <zed/utils/GlobalDefine.hpp>

#ifdef _WIN32
#undef max
#undef min
#endif

#include <sensor_msgs/PointCloud2.h>
#include <pcl/common/common_headers.h>
#include <pcl_conversions/pcl_conversions.h>

#include <cuda.h>
#include <cuda_runtime_api.h>

using namespace sl::zed;
using namespace std;


Camera* zed;
SENSING_MODE dm_type = STANDARD;


int check_red(float prgb) {
    uint32_t rgb = *reinterpret_cast<int*>(&prgb);
    uint8_t r = (rgb >> 16) & 0x0000ff;
    uint8_t g = (rgb >> 8) & 0x0000ff;
    uint8_t b = (rgb) & 0x0000ff;

    float ratio_t = (float)(b + g)/((float)r+1);
    float ratio_c = (float) (b)/((float) g);

    if (ratio_t < 1.6 && ratio_c < 1.3 && ratio_c > 0.7) return 1;
    return 0;

}

int check_blue(float prgb) {
    uint32_t rgb = *reinterpret_cast<int*>(&prgb);
    uint8_t r = (rgb >> 16) & 0x0000ff;
    uint8_t g = (rgb >> 8) & 0x0000ff;
    uint8_t b = (rgb) & 0x0000ff;

    float ratio_t = (float)(r + g)/((float)b+1);
    float ratio_c = (float)(r)/((float)g);

    if (ratio_t < 1.6 && ratio_c < 1.3 && ratio_c > 0.7) return 1;
    return 0;

}

int check_white(float prgb) {
    uint32_t rgb = *reinterpret_cast<int*>(&prgb);
    uint8_t r = (rgb >> 16) & 0x0000ff;
    uint8_t g = (rgb >> 8) & 0x0000ff;
    uint8_t b = (rgb) & 0x0000ff;

    float ratio_r = (float)(b + g)/((float)r+1);
    float ratio_g = (float)(b + r)/((float)g+1);
    float ratio_b = (float)(r + g)/((float)b+1);

    if (ratio_r < 2.2 && ratio_r > 1.8 && ratio_g < 2.2 && ratio_g > 1.8 && ratio_b < 2.2 && ratio_b > 1.8) return 1;
    return 0;

}


int main(int argc, char** argv) {
    /*
    Initialize ZED Camera
    */
    printf("Initializing ZED\n");
    zed = new Camera(VGA);

    sl::zed::InitParams params;
    params.mode = PERFORMANCE;
    params.unit = METER;
    params.coordinate = RIGHT_HANDED;
    params.verbose = true;
    ERRCODE err = zed->init(params);
    cout << errcode2str(err) << endl;
    if (err != SUCCESS) {
        delete zed;
        return 1;
    }
    int width = zed->getImageSize().width;
    int height = zed->getImageSize().height;
    int size = width*height;

    printf("Initializing ROS\n");
    /*
    Set up ROS variables
    - node handler
    - sensor messages
    - topics
    - publishers
    - spin rate
    */

    ros::init(argc, argv, "zed_ros_node");
    WhitelineFilter wl_filter;
    RBflagFilter rb_filter;
    ros::NodeHandle nh;
    sensor_msgs::PointCloud2 output_red;
    sensor_msgs::PointCloud2 output_blue;
    sensor_msgs::PointCloud2 output_white;
    string red_cloud_topic = "point_cloud/red_cloud";
    string blue_cloud_topic = "point_cloud/blue_cloud";
    string white_cloud_topic = "point_cloud/white_cloud";
    std::string point_cloud_frame_id = "/zed_current_frame";
    ros::Publisher pub_red_cloud = nh.advertise<sensor_msgs::PointCloud2> (red_cloud_topic, 2);
    ros::Publisher pub_blue_cloud = nh.advertise<sensor_msgs::PointCloud2> (blue_cloud_topic, 2);
    ros::Publisher pub_white_cloud = nh.advertise<sensor_msgs::PointCloud2> (white_cloud_topic, 2);
    ros::Rate loop_rate(30);

    printf("Allocating Data\n");
    /*
    Initialize remaining variables
    - Point clouds
    - cv::Mat image type
    */
    pcl::PointCloud<pcl::PointXYZRGBA> red_cloud;
    pcl::PointCloud<pcl::PointXYZRGBA> blue_cloud;
    pcl::PointCloud<pcl::PointXYZRGBA> white_cloud;
    int point_step;
    int row_step;
    Mat gpu_cloud;
    float* cpu_cloud;
    float color;
    int index4 = 0;
    pcl::PointXYZRGBA point;
    red_cloud.clear();
    blue_cloud.clear();
    white_cloud.clear();
    cv::Mat image(height, width, CV_8UC4, 1);

    printf("Entering main loop\n");
    while (nh.ok()) {

        if (!zed->grab(dm_type)) {
		index4 = 0;
		//Get image from ZED using the gpu buffer
		gpu_cloud = zed->retrieveMeasure_gpu(MEASURE::XYZBGRA);
		//Get size values for retrieved image
		point_step = gpu_cloud.channels * gpu_cloud.getDataSize();
		row_step = point_step * width;
		//Create a cpu buffer for the image
		cpu_cloud = (float*) malloc(row_step * height);
		//Copy gpu buffer into cpu buffer
		cudaError_t err = cudaMemcpy2D(
			cpu_cloud, row_step, gpu_cloud.data, gpu_cloud.getWidthByte(),
			row_step, height, cudaMemcpyDeviceToHost
		);
		//Filter the image for white lines and red/blue flags
		//cv::cvtColor(slMat2cvMat(zed->retrieveImage(sl::zed::SIDE::LEFT)), image, CV_RGBA2RGB);
		cv::Mat cv_filteredImage = wl_filter.findLines(image);
		cv::Mat r_filteredImage = rb_filter.findRed(image);
		cv::Mat b_filteredImage = rb_filter.findBlu(image);

        	//Iterate through points in cloud
        	for (int i = 0; i < size; i++) {
			//Get coresponding points for each cloud
			cv::Vec3b wl_point = cv_filteredImage.at<cv::Vec3b>((index4/4)/width,(index4/4)%width);
			cv::Vec3b r_point = r_filteredImage.at<cv::Vec3b>((index4/4)/width,(index4/4)%width);
			cv::Vec3b b_point = b_filteredImage.at<cv::Vec3b>((index4/4)/width,(index4/4)%width);

			//Check for bad data
			if (cpu_cloud[index4 + 2] > 0) {
				index4 += 4;
	        		continue;
	        	}
			//Check if point exists in red image
			else if (r_point[0] == 255 && r_point[1] == 255) {	//check_red(cpu_cloud[index4+3])) {
				point.y = -cpu_cloud[index4++];
				point.z = cpu_cloud[index4++];
	        		point.x = -cpu_cloud[index4++];
	        		point.rgb = cpu_cloud[index4++];
				red_cloud.push_back(point);
			}
			//Check if point exists in blue image
			else if (b_point[0] == 255 && b_point[1] == 255) {	//check_blue(cpu_cloud[index4+3])) {
				point.y = -cpu_cloud[index4++];
	        	point.z = cpu_cloud[index4++];
	        	point.x = -cpu_cloud[index4++];
	        	point.rgb = cpu_cloud[index4++];
				blue_cloud.push_back(point);
			}
			//Check if point exists in white-line image
			else if (wl_point[0] == 255 && wl_point[1] == 255) {
				point.y = -cpu_cloud[index4++];
		        	point.z = cpu_cloud[index4++];
		        	point.x = -cpu_cloud[index4++];
		        	point.rgb = cpu_cloud[index4++];
				white_cloud.push_back(point);
			}
			else {
				index4 += 4;
			}

		}
		//Publish Red Flag Point Cloud
		pcl::toROSMsg(red_cloud, output_red); // Convert the point cloud to a ROS message
		output_red.header.frame_id = point_cloud_frame_id; // Set the header values of the ROS message
		output_red.header.stamp = ros::Time::now();
		output_red.is_bigendian = false;
		output_red.is_dense = false;
		pub_red_cloud.publish(output_red);
		red_cloud.clear();
        	//Publish Blue Flag Point Cloud
		pcl::toROSMsg(blue_cloud, output_blue);
        	output_blue.header.frame_id = point_cloud_frame_id; // Set the header values of the ROS message
       		output_blue.header.stamp = ros::Time::now();
       		output_blue.is_bigendian = false;
       		output_blue.is_dense = false;
       		pub_blue_cloud.publish(output_blue);
		blue_cloud.clear();
        	//Publish White Line Point Cloud
		pcl::toROSMsg(white_cloud, output_white); // Convert the point cloud to a ROS message
       		output_white.header.frame_id = point_cloud_frame_id; // Set the header values of the ROS message
       		output_white.header.stamp = ros::Time::now();
       		output_white.is_bigendian = false;
       		output_white.is_dense = false;
       		pub_white_cloud.publish(output_white);
		white_cloud.clear();
		free(cpu_cloud);
		//Spin once updates dynamic_reconfigure values
		ros::spinOnce();
		loop_rate.sleep();
	    }
    }
    delete zed;
    return 0;
}
