#include "docking_station_simulation.hpp"

///////////////////////////////////////////
/*
  docking_station_simulation.cpp
  2021/09/16    Airi Yokochi
*/
///////////////////////////////////////////

/////////////////////////////
/*  コンストラクタ         */
/////////////////////////////

Docking_Station_Simulation::Docking_Station_Simulation(ros::NodeHandle nh){
  ROS_INFO("Docking_Station_Simulation::Docking_Station_Simulation ->start");
  // get rosparam (debug)
  publish_debug_topics_flag = true;
  nh.getParam("publish_debug_topics", publish_debug_topics_flag);
  // get rosparam (link_name)
  robot_link_name = "base";
  station_link_name = "station";
  nh.getParam("robot_link_name", robot_link_name);
  nh.getParam("station_link_name", station_link_name);
  // get rosparam (docking param)
  robot_connector_offset = 0.18;
  station_connector_offset = 0.22;
  docking_area_radious = 0.03;
  docking_area_angle = 0.5;
  voltage = 30.0;
  current = 5.0;
  nh.getParam("robot_connector_offset", robot_connector_offset);
  nh.getParam("station_connector_offset", station_connector_offset);
  nh.getParam("docking_area_radious", docking_area_radious);
  nh.getParam("docking_area_angle", docking_area_angle);
  nh.getParam("voltage", voltage);
  nh.getParam("current", current);
  //publisher
  output_volatage_pub = nh.advertise<std_msgs::Float32>("output_voltage", 10);
  output_current_pub = nh.advertise<std_msgs::Float32>("output_current", 10);
  if(publish_debug_topics_flag){
    robot_pose_pub = nh.advertise<geometry_msgs::PoseStamped>("debug/robot_pose",5);
    station_pose_pub = nh.advertise<geometry_msgs::PoseStamped>("debug/station_pose",5);
    robot_connector_pose_pub = nh.advertise<geometry_msgs::PoseStamped>("debug/robot_connector_pose",5);
    station_connector_pose_pub = nh.advertise<geometry_msgs::PoseStamped>("debug/station_connector_pose",5);
    target_pose_pub = nh.advertise<geometry_msgs::PoseStamped>("debug/target_pose",5);
    docking_flag_pub = nh.advertise<std_msgs::Bool>("debug/docking_flag", 5);
  }
  gazebo_robot_odom = *(ros::topic::waitForMessage<nav_msgs::Odometry>("robot_odom", nh));
  gazebo_station_odom = *(ros::topic::waitForMessage<nav_msgs::Odometry>("station_odom", nh));  
  //subscriber
  robot_odom_sub = nh.subscribe("robot_odom", 10, &Docking_Station_Simulation::robotPoseSubscriberCallback, this);
  station_odom_sub  = nh.subscribe("station_odom", 10, &Docking_Station_Simulation::stationPoseSubscriberCallback, this);
  ROS_INFO("Docking_Station_Simulation::Docking_Station_Simulation ->done");
}

Docking_Station_Simulation::~Docking_Station_Simulation() {
}

// 充電状態によってロボットのコネクタ部分の電圧と電流をPublishする関数
void Docking_Station_Simulation::publishDataLoop(){
  bool docking_flag = validateDocking(getConnectorDevitation());
  std_msgs::Float32 voltage_message;
  std_msgs::Float32 current_message;
  if(docking_flag){
    voltage_message.data = voltage;
    current_message.data = current;
  }else{
    voltage_message.data = 0.0;
    current_message.data = -3.0;
  }
  output_current_pub.publish(current_message);
  output_volatage_pub.publish(voltage_message);
}

// robotのコネクタからみたstationのコネクタのPoseStampedを返す関数
geometry_msgs::PoseStamped Docking_Station_Simulation::getConnectorDevitation(){
  geometry_msgs::Quaternion quaternion_world2robot;
  quaternion_world2robot.x = gazebo_robot_odom.pose.pose.orientation.x;
  quaternion_world2robot.y = gazebo_robot_odom.pose.pose.orientation.y;
  quaternion_world2robot.z = gazebo_robot_odom.pose.pose.orientation.z;
  quaternion_world2robot.w = gazebo_robot_odom.pose.pose.orientation.w;

  float yaw_world2robot = quaternion2yaw(quaternion_world2robot);  
  Eigen::Matrix2f rotation_matrix_world2robot_vec2 = yaw2matrix2(yaw_world2robot);
  Eigen::Matrix2f rotation_matrix_world2robot_vec2_inv = yaw2matrix2(-yaw_world2robot);

  Eigen::Vector2f world2robot_vec2;
  Eigen::Vector2f world2station_vec2;
  world2robot_vec2(0) = gazebo_robot_odom.pose.pose.position.x;
  world2robot_vec2(1) = gazebo_robot_odom.pose.pose.position.y;
  world2station_vec2(0) = gazebo_station_odom.pose.pose.position.x;
  world2station_vec2(1) = gazebo_station_odom.pose.pose.position.y;
  
  Eigen::Vector2f robot2station_vec2;
  robot2station_vec2(0) = world2station_vec2(0) - world2robot_vec2(0);
  robot2station_vec2(1) = world2station_vec2(1) - world2robot_vec2(1);

  Eigen::Vector2f rotated_robot2station2;
  rotated_robot2station2 = rotation_matrix_world2robot_vec2_inv * robot2station_vec2;

  geometry_msgs::Quaternion quaternion_world2station;
  quaternion_world2station.x = gazebo_station_odom.pose.pose.orientation.x;
  quaternion_world2station.y = gazebo_station_odom.pose.pose.orientation.y;
  quaternion_world2station.z = gazebo_station_odom.pose.pose.orientation.z;
  quaternion_world2station.w = gazebo_station_odom.pose.pose.orientation.w;

  float yaw_world2station = quaternion2yaw(quaternion_world2station);  
  Eigen::Matrix2f rotation_matrix_world2station2;
  rotation_matrix_world2station2 = yaw2matrix2(yaw_world2station);

  float yaw_robot2station = yaw_world2station - yaw_world2robot;
  geometry_msgs::Quaternion quaternion_robot2station = yaw2quaternion(yaw_robot2station);
  //robotのPose(表示用)
  geometry_msgs::PoseStamped robot2robot;
  //robotからみたstationのPose
  geometry_msgs::PoseStamped robot2station;

  Eigen::Vector2f robot2connector_vec;         //robotからみたrobotコネクタ
  robot2connector_vec(0) = robot_connector_offset;
  robot2connector_vec(1) = 0;

  Eigen::Vector2f world2robot_connector_vec;    //worldからみたrobotコネクタ
  world2robot_connector_vec = world2robot_vec2 + rotation_matrix_world2robot_vec2 * robot2connector_vec;

  Eigen::Vector2f station2connector_vec;        //stationからみたstationコネクタ
  station2connector_vec(0) = station_connector_offset;
  station2connector_vec(1) = 0;

  Eigen::Vector2f world2station_connector_vec;    //worldからみたstationコネクタ
  world2station_connector_vec = world2station_vec2 + rotation_matrix_world2station2 * station2connector_vec;

  Eigen::Vector2f robot_con2station_con_vec;      //robotのコネクタからみたstationのコネクタ
  robot_con2station_con_vec = world2station_connector_vec - world2robot_connector_vec;

  Eigen::Vector2f rotated_robot_con2station_con_vec;
  rotated_robot_con2station_con_vec = rotation_matrix_world2robot_vec2_inv * robot_con2station_con_vec;

  geometry_msgs::Quaternion quaternion_robot2target;
  quaternion_robot2target = yaw2quaternion(yaw_robot2station + M_PI);

  //targetのPose
  geometry_msgs::PoseStamped robot_con2station_con;
  robot_con2station_con.header.frame_id = gazebo_robot_odom.child_frame_id;
  robot_con2station_con.pose.position.x = rotated_robot_con2station_con_vec(0);
  robot_con2station_con.pose.position.y = rotated_robot_con2station_con_vec(1);
  robot_con2station_con.pose.position.z = 0;
  robot_con2station_con.pose.orientation.x = quaternion_robot2target.x;
  robot_con2station_con.pose.orientation.y = quaternion_robot2target.y;
  robot_con2station_con.pose.orientation.z = quaternion_robot2target.z;
  robot_con2station_con.pose.orientation.w = quaternion_robot2target.w;
  if(publish_debug_topics_flag){
    //robotから見たstationの位置をPublish
    robot2station.header.frame_id = gazebo_robot_odom.child_frame_id;
    robot2station.header.stamp = gazebo_robot_odom.header.stamp;
    robot2station.pose.position.x = rotated_robot2station2(0);
    robot2station.pose.position.y = rotated_robot2station2(1);
    robot2station.pose.position.z = 0;
    robot2station.pose.orientation.x = quaternion_robot2station.x;
    robot2station.pose.orientation.y = quaternion_robot2station.y;
    robot2station.pose.orientation.z = quaternion_robot2station.z;
    robot2station.pose.orientation.w = quaternion_robot2station.w;
    //debug用にロボットの位置を表示
    robot2robot.header = gazebo_robot_odom.header;
    robot2robot.pose = gazebo_robot_odom.pose.pose;
    robot2robot.header.frame_id = gazebo_robot_odom.child_frame_id;
    robot2robot.pose.position.x = 0;
    robot2robot.pose.position.y = 0;
    robot2robot.pose.position.z = 0;
    robot2robot.pose.orientation.x = 0;
    robot2robot.pose.orientation.y = 0;
    robot2robot.pose.orientation.z = 0;
    robot2robot.pose.orientation.w = 1;

    //robotから見たrobot connectorの位置をPublish
    geometry_msgs::PoseStamped robot2robot_con;
    robot2robot_con.header.frame_id = gazebo_robot_odom.child_frame_id;
    robot2robot_con.header.stamp = gazebo_robot_odom.header.stamp;
    robot2robot_con.pose.position.x = robot2connector_vec(0);
    robot2robot_con.pose.position.y = robot2connector_vec(1);
    robot2robot_con.pose.position.z = 0;
    robot2robot_con.pose.orientation.x = 0;
    robot2robot_con.pose.orientation.y = 0;
    robot2robot_con.pose.orientation.z = 0;
    robot2robot_con.pose.orientation.w = 1;
    
    //robotから見たstation connectorの位置をPublish
    Eigen::Matrix2f rotation_matrix_robot2station = yaw2matrix2(yaw_robot2station); //robotから見たstationの回転行列
    Eigen::Vector2f rotated_station2con_vec = rotation_matrix_robot2station * station2connector_vec;  //コネクタ分回転する
    Eigen::Vector2f robot2station_con_vec = rotated_robot2station2 + rotated_station2con_vec;   //robotからみたstaionのコネクタ
    geometry_msgs::PoseStamped robot2station_con;
    robot2station_con.header.frame_id = gazebo_robot_odom.child_frame_id;
    robot2station_con.header.stamp = gazebo_robot_odom.header.stamp;
    robot2station_con.pose.position.x = robot2station_con_vec(0);
    robot2station_con.pose.position.y = robot2station_con_vec(1);
    robot2station_con.pose.position.z = 0;
    robot2station_con.pose.orientation.x = quaternion_robot2station.x;
    robot2station_con.pose.orientation.y = quaternion_robot2station.y;
    robot2station_con.pose.orientation.z = quaternion_robot2station.z;
    robot2station_con.pose.orientation.w = quaternion_robot2station.w;

    robot_pose_pub.publish(robot2robot);
    station_pose_pub.publish(robot2station);
    robot_connector_pose_pub.publish(robot2robot_con);
    station_connector_pose_pub.publish(robot2station_con);
    target_pose_pub.publish(robot_con2station_con);
  }

  return robot_con2station_con;
}

bool Docking_Station_Simulation::validateDocking(geometry_msgs::PoseStamped robot2station_connectors){
  // robotのコネクタからstationのコネクタの距離を計算する
  float translation_diff = sqrt(pow(robot2station_connectors.pose.position.x,2) + pow(robot2station_connectors.pose.position.y, 2));

  // robotのコネクタとstationのコネクタの角度を計算する  
  geometry_msgs::Quaternion anguler_diff_quaternion;
  anguler_diff_quaternion.x = robot2station_connectors.pose.orientation.x;
  anguler_diff_quaternion.y = robot2station_connectors.pose.orientation.y;
  anguler_diff_quaternion.z = robot2station_connectors.pose.orientation.z;
  anguler_diff_quaternion.w = robot2station_connectors.pose.orientation.w;
  float anguler_diff = fabs(quaternion2yaw(anguler_diff_quaternion));
  anguler_diff = fabs( M_PI - anguler_diff);

  bool docking_flag = false;

  // ROS_INFO("diff trans: %f(<%f), diff and: %f(<%f)", translation_diff, docking_area_radious, anguler_diff, docking_area_angle);


  // 距離と角度がしきい値以内ならばdocking_fragをtrueにする
  if( translation_diff < docking_area_radious && anguler_diff < docking_area_angle){
    docking_flag = true;
  }
  if(publish_debug_topics_flag){
    std_msgs::Bool docking_flag_msgs;
    docking_flag_msgs.data = docking_flag;
    //publish
    docking_flag_pub.publish(docking_flag_msgs);
  }

  return docking_flag;
}

// Quaternionからyawに変換する
float Docking_Station_Simulation::quaternion2yaw(geometry_msgs::Quaternion quoternion){
  float x = quoternion.x;
  float y = quoternion.y;
  float z = quoternion.z;
  float w = quoternion.w;
  float yaw_ret = atan2(2.0 * (w * z + x * y), w * w + x * x - y * y - z * z);
  return yaw_ret;
}

// yawから回転行列に変換する
Eigen::Matrix2f Docking_Station_Simulation::yaw2matrix2(float yaw){
  Eigen::Matrix2f rot;
  float cos_f = cos(yaw);
  float sin_f = sin(yaw);
  float rot_ret[2][2] = {{cos_f, -sin_f}, {sin_f, cos_f}};
  rot(0,0) = cos_f;
  rot(0,1) = -sin_f;
  rot(1,0) = sin_f;
  rot(1,1) = cos_f;
  return rot;
}

// yawからQuaternionに変換する
geometry_msgs::Quaternion Docking_Station_Simulation::yaw2quaternion(float yaw){
  geometry_msgs::Quaternion ret_quaternion;
  ret_quaternion.x = 0;
  ret_quaternion.y = 0;
  ret_quaternion.z = sin(yaw/2.0);
  ret_quaternion.w = cos(yaw/2.0);
  return ret_quaternion;
}

void Docking_Station_Simulation::robotPoseSubscriberCallback(const nav_msgs::Odometry::ConstPtr &odom_data){
  gazebo_robot_odom = *odom_data;
}

void Docking_Station_Simulation::stationPoseSubscriberCallback(const nav_msgs::Odometry::ConstPtr &odom_data){
  gazebo_station_odom = *odom_data;
}

int main(int argc, char **argv){
  ros::init(argc, argv, "docking_statoion_simulation");

  ros::NodeHandle nh("~");

  Docking_Station_Simulation docking_station_simulation(nh);

  ros::Rate loop_rate(20);

  while(ros::ok()){
    docking_station_simulation.publishDataLoop();
    ros::spinOnce();
    loop_rate.sleep();
  }
  return 0;
}
