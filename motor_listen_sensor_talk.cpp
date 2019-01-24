#include "ros/ros.h"
#include "std_msgs/Int16.h"
#include "std_msgs/String.h"

#include <sstream>


std_msgs::Int16 gmsg;

void chatterCallback(const std_msgs::Int16::ConstPtr& msg)
{

  gmsg.data=msg->data;
  ROS_INFO("I heard: [%d]", msg->data);

}

int main(int argc, char **argv)
{

  ros::init(argc, argv, "motor_listen_sensor_talk");
  ros::NodeHandle r;
  ros::Publisher encoder_pub = r.advertise<std_msgs::Int16>("encoder", 1000);
  ros::Publisher magnet_pub = r.advertise<std_msgs::Int16>("magnet", 1000);
  ros::Subscriber sub = r.subscribe("desire_position", 1000, chatterCallback);

  std_msgs::Int16 msg;
  std_msgs::Int16 msg2;
  ros::Rate loop_rate(10);
  ros::spinOnce();

  //ROS_INFO("I heard: [%s]", gmsg.data.c_str());
  //ros::spin();
  while (ros::ok())
  {
    std::stringstream ss;
    std::stringstream ss2;
    ss << "bye ";
    ss2 << "s";
    msg.data = 4;
    msg2.data = 5;
    ROS_INFO("I heard2: [%d]", gmsg.data);
    ROS_INFO("%d", msg.data);
    ROS_INFO("%d", msg2.data);
    encoder_pub.publish(msg);
    magnet_pub.publish(msg2);
    ros::spinOnce();
	loop_rate.sleep();
  }

  ros::spin();
  return 0;
}
