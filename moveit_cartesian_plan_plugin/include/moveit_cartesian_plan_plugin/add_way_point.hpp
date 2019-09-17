#ifndef add_way_point_H_
#define add_way_point_H_
#ifndef Q_MOC_RUN  // See: https://bugreports.qt-project.org/browse/QTBUG-22829
#include <stdio.h>
#include <iostream>
#include <string.h>
#include <math.h>
#include <algorithm>
#include <vector>
#include <iterator>

#include <rviz/panel.h>
#include <ros/ros.h>
#include <interactive_markers/interactive_marker_server.h>
#include <interactive_markers/menu_handler.h>
#include <rviz/default_plugin/interactive_markers/interactive_marker.h>
#include <tf/LinearMath/Vector3.h>
#include <tf/LinearMath/Scalar.h>
#include <tf/transform_datatypes.h>
#include <tf2_ros/transform_listener.h>
#include <geometry_msgs/Pose.h>
#include <geometry_msgs/PointStamped.h>
#include <std_msgs/ColorRGBA.h>
#include <tf/tf.h>
#include <tf/transform_listener.h>
#include <tf/message_filter.h>
#include <message_filters/subscriber.h>
#include <peanut_cotyledon/CleanPath.h>
#include <peanut_cotyledon/SetCleanPath.h>
#include <peanut_cotyledon/SetObjects.h>
#include <peanut_cotyledon/Object.h>
#include <trajectory_msgs/JointTrajectory.h>

#include <rviz/properties/bool_property.h>
#include <rviz/properties/string_property.h>
#include <moveit_cartesian_plan_plugin/widgets/path_planning_widget.hpp>

#include <moveit_cartesian_plan_plugin/generate_cartesian_path.hpp>

#include <QWidget>
#include <QCursor>
#include <QObject>
#include <QKeyEvent>
#include <QHBoxLayout>
#include <QTimer>
#include <QtConcurrent/QtConcurrent>
// #include <QtConcurrentRun>
#include <QFuture>

#endif


namespace rviz
{
class VectorProperty;
class VisualizationManager;
class ViewportMouseEvent;

class StringProperty;
class BoolProperty;
}

using namespace visualization_msgs;
namespace moveit_cartesian_plan_plugin
{
/*!
 *  \brief     Class for handling the User Interactions with the RViz enviroment.
 *  \details   The AddWayPoint Class handles all the Visualization in the RViz enviroment.
 	 		   This Class inherits from the rviz::Panel superclass.
 *  \author    Risto Kojcev
 */
class AddWayPoint: public rviz::Panel
{
Q_OBJECT
public:
	//! A Constructor for the RViz Panel.
	AddWayPoint(QWidget* parent = 0);
	//! Virtual Destructor for the RViz Panel.
	virtual ~AddWayPoint();
    //! RViz panel initialization
	virtual void onInitialize();

	//! Fucntion for all the interactive marker interactions
	virtual void processFeedback( const visualization_msgs::InteractiveMarkerFeedbackConstPtr &feedback );
	virtual void processFeedbackInter( const visualization_msgs::InteractiveMarkerFeedbackConstPtr &feedback );

    //! Make a new Interactive Marker Way-Point
	virtual void makeArrow(const tf::Transform& point_pos,int count_arrow);
	//! User Interaction Arrow Marker
	virtual void makeInteractiveMarker();
	ros::ServiceClient set_clean_path_proxy_;
	ros::ServiceClient get_objects_proxy_;
	ros::NodeHandle nh_;

private:
	//! Function for creating a way-point marker
	Marker makeWayPoint( InteractiveMarker &msg );
	//! Function to create the InteractionArrow Marker
	Marker makeInterArrow( InteractiveMarker &msg );
	Marker makeMeshResourceMarker(std::string path, geometry_msgs::Pose object_pose);
	//! Create controls for each different marker. Here we have control for the defaulot starting control ArrowMarkers(the cartesian way points)
	void makeArrowControlDefault(InteractiveMarker &msg );
    //! 6DOF control for the Ingteractive Markers
	void makeArrowControlDetails(InteractiveMarker &msg, bool is_frame_fixed );
	void insert(std::vector<tf::Transform>::iterator insert_it, std::vector<tf::Transform> pose_vector);
	
	//! The box control can be used as a pointer to a certain 3D location and when clicked it will add a arrow to that location.
	InteractiveMarkerControl& makeInteractiveMarkerControl( InteractiveMarker &msg_box );
    //! Function to handle the entries made from the Way-Points interactive markers Menu.
	virtual void changeMarkerControlAndPose(std::string marker_name, std::string control_mode);

    //! Define a server for the Interactive Markers.
    boost::shared_ptr<interactive_markers::InteractiveMarkerServer> server;
	interactive_markers::MenuHandler menu_handler;
	interactive_markers::MenuHandler menu_handler_inter;

    //! Vector for storing all the User Entered Way-Points.
	std::vector<tf::Transform> waypoints_pos;
	//! The position of the User handlable Interactive Marker.
	tf::Transform box_pos;

	std::vector<double> config;

    //! Variable for storing the count of the Way-Points.
	int count;
    //! Target Frame for the Transformation.
    std::string target_frame_;

	tf2_ros::Buffer tfBuffer;
    tf2_ros::TransformListener* tfListener;
	bool points_attached_to_object = true;

protected Q_SLOTS:
	//! rviz::Panel virtual functions for loading Panel Configuration.
	virtual void load(const rviz::Config& config);
	//! rviz::Panel virtual functions for saving Panel Configuration.
	virtual void save(rviz::Config config) const;
public Q_SLOTS:
	//! Slot for handling the event of way-point deletion.
	virtual void pointDeleted(std::string marker_name);
	void cacheConfig(std::vector<double> config);
	
	//! Slot for handling the add way-point event from the RQT UI.
	void addPointFromUI( const tf::Transform point_pos);
	//! Slot for handling when the user updates the position of the Interactive Markers.
	void pointPoseUpdated(const tf::Transform& point_pos, const char* marker_name);
	//! add slot for playing a path from a minimum to maximum index
	void parseWayPointsGoto(int min_index, int max_index);
	//! Slot for parsing the Way-Points before sending them to the MoveIt class.
	void parseWayPoints();
	void saveToolPath();
	//! Save all the Way-Points to a yaml file.
	void saveWayPointsObject(std::string floor_name, std::string area_name, int object_id, std::string task_name, peanut_cotyledon::CleanPath clean_path);
	bool getObjectWithID(std::string floor_name, std::string area_name, int object_id, peanut_cotyledon::Object& desired_obj);

	//! clear all the 3d interaction point boxes
	void clearAllInteractiveBoxes();

	//! Clear all the Way-Points
	void clearAllPointsRViz();
	// Modify marker control 
	void modifyMarkerControl(std::string mesh_name, geometry_msgs::Pose object_pose);
	void transformPointsViz(std::string frame);
	//! Slot for handling the even when a way-point is out of the IK solution of the loaded robot.
	void wayPointOutOfIK_slot(int point_number,int out, std::vector<geometry_msgs::Pose> out_of_bounds_poses);
	//! Get the name of the Transformation frame of the Robot.
	void getRobotModelFrame_slot(const std::string robot_model_frame,const tf::Transform end_effector);

Q_SIGNALS:
	//! Signal for notifying that RViz is done with initialization.
	void initRviz();
	//! Signal for notifying that a way-point was deleted in the RViz enviroment.
	void pointDeleteRviz(int marker_name_nr);
	//! Signal for notifying that a way-point has been added from the RViz enviroment.
	void addPointRViz(const tf::Transform& point_pos, const int count);
	//! Signal that the way-point position has been updated by the user from the RViz enviroment.
	void pointPoseUpdatedRViz(const tf::Transform& point_pos, const char* marker_name);
	//! Signal for sending all the Way-Points.
	void wayPoints_signal(std::vector<geometry_msgs::Pose> waypoints);
	//! Signal to check if the way-point is in valid IK solution.
	void onUpdatePosCheckIkValidity(const std::vector<tf::Transform> waypoints_pos, const int point_number);


protected:
	//! The class that GUI RQT User Interactions.
    QWidget *widget_;
    //! The Object for the MoveIt components.
    QObject *path_generate;
private:
	//! Define constants for color, arrow size, etc.
	std_msgs::ColorRGBA WAY_POINT_COLOR;
	std_msgs::ColorRGBA WAY_POINT_COLOR_OUTSIDE_IK;
	std_msgs::ColorRGBA ARROW_INTER_COLOR;

	geometry_msgs::Vector3 WAY_POINT_SCALE_CONTROL;
	geometry_msgs::Vector3 ARROW_INTER_SCALE_CONTROL;
	geometry_msgs::Vector3 MESH_SCALE_CONTROL;
	geometry_msgs::Pose parent_home_;

	float INTERACTIVE_MARKER_SCALE;
	float ARROW_INTERACTIVE_SCALE;
};
} //end of namespace moveit_cartesian_plan_plugin

#endif //add_way_point_H_
