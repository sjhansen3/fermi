#include <boost/function.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/assert.hpp>
#include <math.h>
#include <moveit/robot_state/conversions.h>
#include <moveit/trajectory_processing/iterative_time_parameterization.h>
#include <moveit_cartesian_plan_plugin/generate_cartesian_path.hpp>

#include <moveit/planning_interface/planning_interface.h>
#include <moveit/planning_scene/planning_scene.h>
#include <moveit/kinematic_constraints/utils.h>
#include <moveit_msgs/DisplayTrajectory.h>
#include <moveit_msgs/PlanningScene.h>

#include <ros/ros.h>
#include <geometry_msgs/PoseArray.h>
#include <tf2_eigen/tf2_eigen.h>

#include <pluginlib/class_loader.h>

// MoveIt!
#include <moveit/robot_model_loader/robot_model_loader.h>
#include <moveit/planning_interface/planning_interface.h>
#include <moveit/planning_scene/planning_scene.h>
#include <moveit/planning_scene_monitor/planning_scene_monitor.h>
#include <moveit/kinematic_constraints/utils.h>
#include <moveit_msgs/DisplayTrajectory.h>
#include <moveit_msgs/PlanningScene.h>
#include <moveit_visual_tools/moveit_visual_tools.h>


GenerateCartesianPath::GenerateCartesianPath(QObject *parent)
{
    //to add initializations
    init();

}
GenerateCartesianPath::~GenerateCartesianPath()
{
  /*! The destructor resets the moveit_group_ and the kinematic_state of the robot.
  */
  moveit_group_.reset();
  kinematic_state_.reset();
  robot_model_loader.reset();
  kmodel_.reset();
  delete tfListener;
}

void GenerateCartesianPath::init()
{
  /*! Initialize the MoveIt parameters:
        - MoveIt group
        - Kinematic State is the current kinematic congiguration of the Robot
        - Robot model which handles getting the Robot Model
        - Joint Model group which are necessary for checking if Way-Point is outside the IK Solution
        .
  */
  selected_plan_group = 0;
  robot_model_loader = RobotModelLoaderPtr(new robot_model_loader::RobotModelLoader("robot_description"));
  kmodel_ = robot_model_loader->getModel();

  end_eff_joint_groups = kmodel_->getEndEffectors();


  ROS_INFO_STREAM("size of the end effectors is: "<<end_eff_joint_groups.size());
  tfListener = new tf2_ros::TransformListener(tfBuffer);

  if (end_eff_joint_groups.empty())
  {
    std::vector< std::string > group_names_tmp_;
    const moveit::core::JointModelGroup *  end_eff_joint_groups_tmp_;
    group_names_tmp_ = kmodel_->getJointModelGroupNames();

    for(int i=0;i<group_names_tmp_.size();i++)
    {

    end_eff_joint_groups_tmp_ = kmodel_->getJointModelGroup(group_names_tmp_.at(i));
    if(end_eff_joint_groups_tmp_->isChain())
    {
      group_names.push_back(group_names_tmp_.at(i));
    }
    else
    {
      ROS_WARN_STREAM("The group:" << end_eff_joint_groups_tmp_->getName() <<" is not a Chain. Depreciate it!!");
    }
    }
  }
  else
  {
  for(int i=0;i<end_eff_joint_groups.size();i++)
  {
    if(end_eff_joint_groups.at(i)->isChain())
    {
    const std::string& parent_group_name = end_eff_joint_groups.at(i)->getName();
    group_names.push_back(parent_group_name);

    ROS_INFO_STREAM("Group name:"<< group_names.at(i));
    }
    else
    {
        ROS_INFO_STREAM("This group is not a chain. Find the parent of the group");
        const std::pair< std::string, std::string > & parent_group_name = end_eff_joint_groups.at(i)->getEndEffectorParentGroup();
        group_names.push_back(parent_group_name.first);
    }
  }
}

  ROS_INFO_STREAM("Group name:"<< group_names[selected_plan_group]);

  moveit_group_ = MoveGroupPtr(new moveit::planning_interface::MoveGroupInterface(group_names[selected_plan_group]));
  kinematic_state_ = moveit::core::RobotStatePtr(new robot_state::RobotState(kmodel_));
  kinematic_state_->setToDefaultValues();

  joint_model_group_ = kmodel_->getJointModelGroup(group_names[selected_plan_group]);
}

void GenerateCartesianPath::setCartParams(double plan_time_,double cart_step_size_, double cart_jump_thresh_, bool moveit_replan_,bool avoid_collisions_, std::string robot_model_frame_)
{
  /*! Set the necessary parameters for the MoveIt and the Cartesian Path Planning.
      These parameters correspond to the ones that the user has entered or the default ones before the execution of the Cartesian Path Planner.
  */
  ROS_INFO_STREAM("MoveIt and Cartesian Path parameters from UI:\n MoveIt Plan Time:"<<plan_time_
                  <<"\n Cartesian Path Step Size:"<<cart_step_size_
                  <<"\n Jump Threshold:"<<cart_jump_thresh_
                  <<"\n Replanning:"<<moveit_replan_
                  <<"\n Avoid Collisions:"<<avoid_collisions_
                  <<"\n Robot model frame: "<<robot_model_frame_);

  PLAN_TIME_        = plan_time_;
  MOVEIT_REPLAN_    = moveit_replan_;
  CART_STEP_SIZE_   = cart_step_size_;
  CART_JUMP_THRESH_ = cart_jump_thresh_;
  AVOID_COLLISIONS_ = avoid_collisions_;
  ROBOT_MODEL_FRAME_ = robot_model_frame_;
}

void GenerateCartesianPath::moveToConfig(std::vector<double> config, bool plan_only) 
{
    Q_EMIT cartesianPathExecuteStarted();

    ROS_INFO("starting plan to config");

    moveit::planning_interface::MoveGroupInterface::Plan my_plan;
    moveit_group_->setPlanningTime(PLAN_TIME_);
    moveit_group_->allowReplanning (MOVEIT_REPLAN_);

    moveit_group_->setJointValueTarget(config);

    bool success = (moveit_group_->plan(my_plan) == moveit::planning_interface::MoveItErrorCode::SUCCESS);
    ROS_INFO("Visualizing plan 2 (joint space goal) %s", success ? "" : "FAILED");
    
    if (!plan_only && success){
      moveit_group_->execute(my_plan);
    }

    Q_EMIT cartesianPathExecuteFinished();

}

void GenerateCartesianPath::moveToPose(std::vector<geometry_msgs::Pose> waypoints)
{
    /*!

    */
    Q_EMIT cartesianPathExecuteStarted();

    moveit_group_->setPlanningTime(PLAN_TIME_);
    moveit_group_->allowReplanning (MOVEIT_REPLAN_);
    moveit_group_->setPoseReferenceFrame("base_link");

    //TODO convert the poses here to the base link from the robot_model_frame

    moveit::planning_interface::MoveGroupInterface::Plan plan;

    moveit_msgs::RobotTrajectory trajectory_;
    double fraction = moveit_group_->computeCartesianPath(waypoints,CART_STEP_SIZE_,CART_JUMP_THRESH_,trajectory_,AVOID_COLLISIONS_);
    robot_trajectory::RobotTrajectory rt(kmodel_, group_names[selected_plan_group]);

    rt.setRobotTrajectoryMsg(*kinematic_state_, trajectory_);

    ROS_INFO_STREAM("Frame which moveit requests plans in (it transforms to this frame before sending plan): " << moveit_group_->getPoseReferenceFrame());
    ROS_INFO_STREAM("Frame which poses are represented in " << ROBOT_MODEL_FRAME_);

    // Third create a IterativeParabolicTimeParameterization object
    trajectory_processing::IterativeParabolicTimeParameterization iptp;
    bool success = iptp.computeTimeStamps(rt);
    ROS_INFO("Computed time stamp %s",success?"SUCCEDED":"FAILED");


    // Get RobotTrajectory_msg from RobotTrajectory
    rt.getRobotTrajectoryMsg(trajectory_);
    // Finally plan and execute the trajectory
    plan.trajectory_ = trajectory_;
    ROS_INFO("Visualizing plan (cartesian path) (%.2f%% acheived)",fraction * 100.0);
    Q_EMIT cartesianPathCompleted(fraction);

    moveit_group_->execute(plan);

    kinematic_state_ = moveit_group_->getCurrentState();

    Q_EMIT cartesianPathExecuteFinished();

}

void GenerateCartesianPath::cartesianPathHandler(std::vector<geometry_msgs::Pose> waypoints)
{
  /*! Since the execution of the Cartesian path is time consuming and can lead to locking up of the Plugin and the RViz enviroment the function for executing the Cartesian Path Plan has been placed in a separtate thread.
      This prevents the RViz and the Plugin to lock.
  */
  ROS_INFO("Starting concurrent process for Cartesian Path");
  QFuture<void> future = QtConcurrent::run(this, &GenerateCartesianPath::moveToPose, waypoints);
}

void GenerateCartesianPath::freespacePathHandler(std::vector<double> config, bool plan_only)
{
  /*! Since the execution of the Cartesian path is time consuming and can lead to locking up of the Plugin and the RViz enviroment the function for executing the Cartesian Path Plan has been placed in a separtate thread.
      This prevents the RViz and the Plugin to lock.
  */
  ROS_INFO("Starting concurrent process for Freespace Planning");
  QFuture<void> future = QtConcurrent::run(this, &GenerateCartesianPath::moveToConfig, config, plan_only);
}



void GenerateCartesianPath::checkWayPointValidity(const geometry_msgs::Pose& waypoint, const int point_number)
{
      /*! This function is called every time the user updates the pose of the Way-Point and checks if the Way-Point is within the valid IK solution for the Robot.
          In the case when a point is outside the valid IK solution this function send a signal to the RViz enviroment to update the color of the Way-Point.
      */
    try {
      if (ROBOT_MODEL_FRAME_.empty()){
        ROS_ERROR_STREAM("the robot model frame is empty for point number " << point_number);
        Q_EMIT wayPointOutOfIK(point_number,0);
        return;
      }
      ROS_INFO_STREAM("The frame ik is in is map the frame we are currently in is " << ROBOT_MODEL_FRAME_ << " transforming");
      geometry_msgs::TransformStamped transformStamped;
      try{
        transformStamped = tfBuffer.lookupTransform("mobile_base_link", ROBOT_MODEL_FRAME_, ros::Time(0));
      }
      catch (tf2::TransformException &ex) {
        ROS_ERROR("transform error in check waypoint validity");
        ROS_ERROR("%s",ex.what());
        Q_EMIT wayPointOutOfIK(point_number,0);
        return;
      }
      geometry_msgs::Pose waypoint_pose_copy;
      waypoint_pose_copy.position = waypoint.position;
      waypoint_pose_copy.orientation = waypoint.orientation;

      const geometry_msgs::Transform constTransform = transformStamped.transform;
      ROS_INFO_STREAM("transform used to transform into planning frame of base_link: " << transformStamped);
      tf::Transform transform_old_new;
      tf::transformMsgToTF(constTransform, transform_old_new);

      tf::Transform waypoint_tf;
      tf::poseMsgToTF (waypoint_pose_copy, waypoint_tf);
      waypoint_tf = transform_old_new*waypoint_tf;
      tf::poseTFToMsg (waypoint_tf, waypoint_pose_copy);

      ROS_INFO_STREAM("the frame the solver expects is  " << joint_model_group_->getSolverInstance()->getTipFrame());
      ROS_INFO_STREAM("the frame of the is robot model" << kinematic_state_->getRobotModel()->getModelFrame().c_str());
      // kinematic_state_->getRobotModel()->printModelInfo(std::cout);

      bool found_ik = kinematic_state_->setFromIK(joint_model_group_, waypoint_pose_copy, 1, 0.05);

         if(found_ik)
        {
           Q_EMIT wayPointOutOfIK(point_number,0);
        }
        else
        {
          Q_EMIT wayPointOutOfIK(point_number,1);
        }
}
    catch (...) {
      ROS_ERROR("Something happened in checkWayPointValidity, steve doesnt know what");
    }
}

void GenerateCartesianPath::initRvizDone()
{
  /*! Once the initialization of the RViz is has finished, this function sends the pose of the robot end-effector and the name of the base frame to the RViz enviroment.
      The RViz enviroment sets the User Interactive Marker pose and Add New Way-Point RQT Layout default values based on the end-effector starting position.
      The transformation frame of the InteractiveMarker is set based on the robot PoseReferenceFrame.
  */
  ROS_INFO("RViz is done now we need to emit the signal");

  if(moveit_group_->getEndEffectorLink().empty())
  {
    ROS_INFO("End effector link is empty");
    const std::vector< std::string > &  joint_names = joint_model_group_->getLinkModelNames();
    for(int i=0;i<joint_names.size();i++)
    {
      ROS_INFO_STREAM("Link " << i << " name: "<< joint_names.at(i));
    }
    const Eigen::Affine3d &end_effector_state = kinematic_state_->getGlobalLinkTransform(joint_names.at(0));
    //tf::Transform end_effector;
    tf::transformEigenToTF(end_effector_state, end_effector);
    Q_EMIT getRobotModelFrame_signal(ROBOT_MODEL_FRAME_,end_effector);
  }
  else
  {

    ROS_INFO("End effector link is not empty");
    const Eigen::Affine3d &end_effector_state = kinematic_state_->getGlobalLinkTransform(moveit_group_->getEndEffectorLink());
    //tf::Transform end_effector;
    tf::transformEigenToTF(end_effector_state, end_effector);

    Q_EMIT getRobotModelFrame_signal(ROBOT_MODEL_FRAME_,end_effector);
  }

    Q_EMIT sendCartPlanGroup(group_names);


}
void GenerateCartesianPath::moveToHome()
{

  geometry_msgs::Pose home_pose;
  tf::poseTFToMsg(end_effector,home_pose);

  std::vector<geometry_msgs::Pose> waypoints;
  waypoints.push_back(home_pose);

  cartesianPathHandler(waypoints);

}

void GenerateCartesianPath::getSelectedGroupIndex(int index)
{
  selected_plan_group = index;

  ROS_INFO_STREAM("selected name is:"<<group_names[selected_plan_group]);
  moveit_group_.reset();
  kinematic_state_.reset();
  moveit_group_ = MoveGroupPtr(new moveit::planning_interface::MoveGroupInterface(group_names[selected_plan_group]));

  kinematic_state_ = moveit::core::RobotStatePtr(new robot_state::RobotState(kmodel_));
  kinematic_state_->setToDefaultValues();

  joint_model_group_ = kmodel_->getJointModelGroup(group_names[selected_plan_group]);

  ROS_INFO("End effector link is not empty");
  const Eigen::Affine3d &end_effector_state = kinematic_state_->getGlobalLinkTransform(moveit_group_->getEndEffectorLink());
  tf::transformEigenToTF(end_effector_state, end_effector);

  Q_EMIT getRobotModelFrame_signal(ROBOT_MODEL_FRAME_, end_effector);

}
