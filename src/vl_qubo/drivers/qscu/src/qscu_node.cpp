#include "qscu_node.h"

using namespace std;

QSCUNode::QSCUNode(ros::NodeHandle n, string node_name, string device_file)
	:m_node_name(node_name), qscu(device_file, B115200) {

	string qubo_namespace = "/qubo/";

	string embedded_status_topic = qubo_namespace + "embedded_status";
	m_status_pub = n.advertise<ram_msgs::Status>(embedded_status_topic, 1000);


	/**
	 * This stuff is almost exactly the same as it is in the Gazebo node
	 * This stuff is now done in hardware_node.cpp
	 */
	//string roll_topic	= qubo_namespace + "roll";
	//string pitch_topic	= qubo_namespace + "pitch";
	//string yaw_topic	= qubo_namespace + "yaw";
	//string depth_topic	= qubo_namespace + "depth"; //sometimes called heave
	//string surge_topic	= qubo_namespace + "surge"; //"forward" translational motion
	//string sway_topic	= qubo_namespace + "sway";   //"sideways" translational motion

	//m_roll_sub  = n.subscribe(roll_topic  + "_cmd", 1000, &QSCUNode::rollCallback, this);
	//m_pitch_sub = n.subscribe(pitch_topic + "_cmd", 1000, &QSCUNode::pitchCallback, this);
	//m_yaw_sub   = n.subscribe(yaw_topic   + "_cmd", 1000, &QSCUNode::yawCallback, this);
	//m_depth_sub = n.subscribe(depth_topic + "_cmd", 1000, &QSCUNode::depthCallback, this);
	//m_surge_sub = n.subscribe(surge_topic + "_cmd", 1000, &QSCUNode::surgeCallback, this);
	//m_sway_sub  = n.subscribe(sway_topic  + "_cmd", 1000, &QSCUNode::swayCallback, this);

	m_thruster_speeds.resize(8);

	string t_topic;
	
        m_thruster_zero = n.subscribe("/hardware_node/thrusters/0/input", 1000, &QSCUNode::thrusterZeroCallback, this);
	m_thruster_one = n.subscribe("/hardware_node/thrusters/1/input", 1000, &QSCUNode::thrusterOneCallback, this);
	m_thruster_two = n.subscribe("/hardware_node/thrusters/2/input", 1000, &QSCUNode::thrusterTwoCallback, this);
	m_thruster_three = n.subscribe("/hardware_node/thrusters/3/input", 1000, &QSCUNode::thrusterThreeCallback, this);
	m_thruster_four = n.subscribe("/hardware_node/thrusters/4/input", 1000, &QSCUNode::thrusterFourCallback, this);
	m_thruster_five = n.subscribe("/hardware_node/thrusters/5/input", 1000, &QSCUNode::thrusterFiveCallback, this);
	m_thruster_six = n.subscribe("/hardware_node/thrusters/6/input", 1000, &QSCUNode::thrusterSixCallback, this);
	m_thruster_seven = n.subscribe("/hardware_node/thrusters/7/input", 1000, &QSCUNode::thrusterSevenCallback, this);
	
	/**
	 * Creates a Timer object, which will trigger every `Duration` amount of time
	 * to allow us to have a bit more accuracy in the time between updates on Qubobus
	 */
	qubobus_loop = n.createTimer(ros::Duration(0.05), &QSCUNode::QubobusCallback, this);
	qubobus_incoming_loop = n.createTimer(ros::Duration(0.1), &QSCUNode::QubobusIncomingCallback, this);
	// qubobus_status_loop = n.createTimer(ros::Duration(5), &QSCUNode::QubobusStatusCallback, this);
	qubobus_thruster_loop = n.createTimer(ros::Duration(0.1), &QSCUNode::QubobusThrusterCallback, this);

	qubobus_loop.start();
	qubobus_incoming_loop.start();
	// qubobus_status_loop.start();
}

QSCUNode::~QSCUNode(){
	qubobus_loop.stop();
}

void QSCUNode::update(){
	ros::spin();
}

void QSCUNode::QubobusThrusterCallback(const ros::TimerEvent& event){

	// Calculate the values for the thrusters we need to change
  m_thruster_speeds[0] = m_thruster_zero_buffer;
  m_thruster_speeds[1] = m_thruster_one_buffer;
  m_thruster_speeds[2] = m_thruster_two_buffer;
  m_thruster_speeds[3] = m_thruster_three_buffer;

  m_thruster_speeds[4] = m_thruster_four_buffer;
  m_thruster_speeds[5] = m_thruster_five_buffer;
  m_thruster_speeds[6] = m_thruster_six_buffer;
  m_thruster_speeds[7] = m_thruster_seven_buffer;

	// Create the message and add it to the queue
	for (uint8_t i = 0; i < 8; i++) {
		QMsg q_msg;
		q_msg.type = tThrusterSet;
		q_msg.payload = std::make_shared<struct Thruster_Set>( (struct Thruster_Set) {
				.throttle = m_thruster_speeds[i],
					.thruster_id = i,
					});
		q_msg.reply = nullptr;
		m_outgoing.push(q_msg);
	}

	thruster_update = false;

}

void QSCUNode::QubobusCallback(const ros::TimerEvent& event){

	if ( !qscu.isOpen() ) {
		try {
		  qscu.openDevice();
		} catch ( const QSCUException ex ) {
			ROS_ERROR("Unable to connect to the embedded system at the specified location");
			ROS_ERROR("=> %s", ex.what() );
			return;
		}
	}

	try {
		if (m_outgoing.empty()) {
		  qscu.keepAlive();
		} else {
			QMsg msg = m_outgoing.front();
			qscu.sendMessage(&msg.type, msg.payload.get(), msg.reply.get());
			m_incoming.push(msg);
			m_outgoing.pop();
		}
	} catch ( const QSCUException ex ) {
		ROS_ERROR("Error reading the embedded system status");
		ROS_ERROR("=> %s", ex.what() );
		try {
		  qscu.connect();
		} catch ( const QSCUException ex ) {
			ROS_ERROR("Unable to connect to the Tiva");
		}
		return;
	}
}

void QSCUNode::QubobusIncomingCallback(const ros::TimerEvent& event){
	while (!m_incoming.empty()) {
		QMsg msg = m_incoming.front();
		if (msg.type.id == tEmbeddedStatus.id){
			std::shared_ptr<struct Embedded_Status> e_s =
				std::static_pointer_cast<struct Embedded_Status>(msg.reply);
			ROS_ERROR("Uptime: %i, Mem: %f", e_s->uptime, e_s->mem_capacity);
		}

		m_incoming.pop();
	}
}

void QSCUNode::QubobusStatusCallback(const ros::TimerEvent& event){
	QMsg q_msg;
	q_msg.type = tEmbeddedStatus;
	q_msg.payload = nullptr;
	q_msg.reply = std::make_shared<struct Embedded_Status>();
	m_outgoing.push(q_msg);
}

void QSCUNode::thrusterZeroCallback(const std_msgs::Float64::ConstPtr& msg) {
  m_thruster_zero_buffer = (float) msg->data;
}

void QSCUNode::thrusterOneCallback(const std_msgs::Float64::ConstPtr& msg) {
  m_thruster_one_buffer = (float) msg->data;
}

void QSCUNode::thrusterTwoCallback(const std_msgs::Float64::ConstPtr& msg) {
  m_thruster_two_buffer = (float) msg->data;
}

void QSCUNode::thrusterThreeCallback(const std_msgs::Float64::ConstPtr& msg) {
  m_thruster_three_buffer = (float) msg->data;
}

void QSCUNode::thrusterFourCallback(const std_msgs::Float64::ConstPtr& msg) {
  m_thruster_four_buffer = (float) msg->data;
}

void QSCUNode::thrusterFiveCallback(const std_msgs::Float64::ConstPtr& msg) {
  m_thruster_five_buffer = (float) msg->data;
}

void QSCUNode::thrusterSixCallback(const std_msgs::Float64::ConstPtr& msg) {
  m_thruster_six_buffer = (float) msg->data;
}

void QSCUNode::thrusterSevenCallback(const std_msgs::Float64::ConstPtr& msg) {
  m_thruster_seven_buffer = (float) msg->data;
}
