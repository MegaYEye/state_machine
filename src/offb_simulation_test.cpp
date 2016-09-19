 /**
* @file     : offb_simulation_test.cpp
* @brief    : offboard simulation test: demo rewritten -> 4 setpoints flight -> complete state machine.
* @author   : libn
* @Time     : Aug 25, 201610:06:42 PM
*/

#include <ros/ros.h>
#include <geometry_msgs/PoseStamped.h>
#include <geometry_msgs/TwistStamped.h>			/* local velocity setpoint. -libn */
#include <state_machine/CommandBool.h>
#include <state_machine/SetMode.h>
#include <state_machine/State.h>
#include <state_machine/CommandTOL.h>
#include <state_machine/Setpoint.h>
#include <state_machine/DrawingBoard10.h>

void state_machine_func(void);
/* mission state. -libn */
static const int takeoff = 0;
static const int takeoff_hover = 1;
static const int mission_point_A = 2;
static const int mission_point_A_hover_recognize = 3;
static const int mission_search = 4;
static const int mission_scan = 5;
static const int mission_relocate = 6;
static const int mission_operate_move = 7;
static const int mission_operate_hover = 8;
static const int mission_operate_spray = 9;
static const int mission_stop = 10;
static const int mission_fix_failure = 11;
static const int mission_home = 12;
static const int mission_home_hover = 13;
static const int land = 14;
int loop = 1;	/* loop calculator: loop = 1/2/3/4/5. -libn */
// current mission state, initial state is to takeoff
int current_mission_state = takeoff;
ros::Time mission_last_time;	/* timer used in mission. -libn */
bool display_screen_num_recognized = false;	/* to check if the num on display screen is recognized. -libn */
bool relocate_valid = false;	/* to complete relocate mission. -libn */
int mission_failure = 0;

int current_mission_num;	/* mission num: 5 subtask -> 5 current nums.	TODO:change mission num. -libn */

ros::Time mission_timer_t;	/* timer to control the whole mission. -libn */
ros::Time loop_timer_t;	/* timer to control subtask. -libn */

state_machine::State current_state;
state_machine::State last_state;
state_machine::State last_state_display;
void state_cb(const state_machine::State::ConstPtr& msg){
	last_state_display.mode = current_state.mode;
	last_state_display.armed = current_state.armed;
	current_state = *msg;
}

state_machine::Setpoint setpoint_indexed;
void SetpointIndexedCallback(const state_machine::Setpoint::ConstPtr& msg)
{
	setpoint_indexed = *msg;
}

// local position msg callback function
geometry_msgs::PoseStamped current_pos;
void pos_cb(const geometry_msgs::PoseStamped::ConstPtr& msg)
{
    current_pos = *msg;
}

/* 10 drawing board positions. -libn */
state_machine::DrawingBoard10 board10;
void board_pos_cb(const state_machine::DrawingBoard10::ConstPtr& msg)
{
	board10 = *msg;

	ROS_INFO("\nboard_0 position: %d x = %f y = %f z = %f\n"
				"board_1 position: %d x = %f y = %f z = %f\n"
				"board_2 position: %d x = %f y = %f z = %f\n"
				"board_3 position: %d x = %f y = %f z = %f\n"
				"board_4 position: %d x = %f y = %f z = %f\n"
				"board_5 position: %d x = %f y = %f z = %f\n"
				"board_6 position: %d x = %f y = %f z = %f\n"
				"board_7 position: %d x = %f y = %f z = %f\n"
				"board_8 position: %d x = %f y = %f z = %f\n"
				"board_9 position: %d x = %f y = %f z = %f\n",
				board10.drawingboard[0].valid,board10.drawingboard[0].x,board10.drawingboard[0].y,board10.drawingboard[0].z,
				board10.drawingboard[1].valid,board10.drawingboard[1].x,board10.drawingboard[1].y,board10.drawingboard[1].z,
				board10.drawingboard[2].valid,board10.drawingboard[2].x,board10.drawingboard[2].y,board10.drawingboard[2].z,
				board10.drawingboard[3].valid,board10.drawingboard[3].x,board10.drawingboard[3].y,board10.drawingboard[3].z,
				board10.drawingboard[4].valid,board10.drawingboard[4].x,board10.drawingboard[4].y,board10.drawingboard[4].z,
				board10.drawingboard[5].valid,board10.drawingboard[5].x,board10.drawingboard[5].y,board10.drawingboard[5].z,
				board10.drawingboard[6].valid,board10.drawingboard[6].x,board10.drawingboard[6].y,board10.drawingboard[6].z,
				board10.drawingboard[7].valid,board10.drawingboard[7].x,board10.drawingboard[7].y,board10.drawingboard[7].z,
				board10.drawingboard[8].valid,board10.drawingboard[8].x,board10.drawingboard[8].y,board10.drawingboard[8].z,
				board10.drawingboard[9].valid,board10.drawingboard[9].x,board10.drawingboard[9].y,board10.drawingboard[9].z);
}

/* 4 setpoints. -libn */
geometry_msgs::PoseStamped setpoint_A;
geometry_msgs::PoseStamped setpoint_B;
geometry_msgs::PoseStamped setpoint_C;
geometry_msgs::PoseStamped setpoint_D;
geometry_msgs::PoseStamped setpoint_H;	/* home position. -libn */

geometry_msgs::PoseStamped pose_pub;
geometry_msgs::TwistStamped vel_pub;	/* velocity setpoint to be published. -libn */


int main(int argc, char **argv)
{
    ros::init(argc, argv, "offb_node");
    ros::NodeHandle nh;

    ros::Subscriber state_sub = nh.subscribe<state_machine::State>
            ("mavros/state", 10, state_cb);
    ros::Publisher local_pos_pub = nh.advertise<geometry_msgs::PoseStamped>
            ("mavros/setpoint_position/local", 10);

    /* Velocity setpoint. -libn */
    ros::Publisher local_vel_pub = nh.advertise<geometry_msgs::TwistStamped>
                ("/mavros/setpoint_velocity/cmd_vel", 10);

    ros::ServiceClient arming_client = nh.serviceClient<state_machine::CommandBool>
            ("mavros/cmd/arming");
    ros::ServiceClient set_mode_client = nh.serviceClient<state_machine::SetMode>
            ("mavros/set_mode");

    // takeoff and land service
    // ros::ServiceClient takeoff_client = nh.serviceClient<mavros_msgs::CommandTOL>("mavros/cmd/takeoff");
    ros::ServiceClient land_client = nh.serviceClient<state_machine::CommandTOL>("mavros/cmd/land");
    state_machine::CommandTOL landing_cmd;
    landing_cmd.request.min_pitch = 1.0;

	/* receive indexed setpoint. -libn */
	ros::Subscriber setpoint_Indexed_sub = nh.subscribe("Setpoint_Indexed", 100 ,SetpointIndexedCallback);
	/* get pixhawk's local position. -libn */
	ros::Subscriber local_pos_sub = nh.subscribe<geometry_msgs::PoseStamped>("mavros/local_position/pose", 10, pos_cb);

	ros::Subscriber DrawingBoard_Position_sub = nh.subscribe<state_machine::DrawingBoard10>
		            ("DrawingBoard_Position10", 10, board_pos_cb);
	board10.drawingboard.resize(10);		/* MUST! -libn */

    //the setpoint publishing rate MUST be faster than 2Hz
    ros::Rate rate(10.0);

    // wait for FCU connection
    while(ros::ok() && current_state.connected){
        ros::spinOnce();
        rate.sleep();
    }


    pose_pub.pose.position.x = 0;
    pose_pub.pose.position.y = 0;
    pose_pub.pose.position.z = 2;
    pose_pub.pose.orientation.x = 0;			/* orientation expressed using quaternion. -libn */
	pose_pub.pose.orientation.y = 0;			/* w = cos(theta/2), x = nx * sin(theta/2),  y = ny * sin(theta/2), z = nz * sin(theta/2) -libn */
	pose_pub.pose.orientation.z = 0.707;
	pose_pub.pose.orientation.w = 0.707;		/* set yaw* = 90 degree(default in simulation). -libn */

	setpoint_H.pose.position.x = 0.0f;	/* pose(x,y) is not used, just for safe. -libn */
	setpoint_H.pose.position.y = 0.0f;
	setpoint_H.pose.position.z = 3.0f;

    //send a few setpoints before starting
    for(int i = 100; ros::ok() && i > 0; --i){
        local_pos_pub.publish(pose_pub);
        ros::spinOnce();
        rate.sleep();
    }

    current_mission_num = 0;

    ros::Time last_request = ros::Time::now();
    ros::Time last_show_request = ros::Time::now();
    mission_timer_t = ros::Time::now();
    loop_timer_t = ros::Time::now();
    last_state_display = current_state;
    last_state.mode = current_state.mode;
    last_state.armed = current_state.armed;
    ROS_INFO("current_state.mode = %s",current_state.mode.c_str());
    ROS_INFO("armed status: %d",current_state.armed);

    while(ros::ok()){
    	/* mode switch display(Once). -libn */
    	if(current_state.mode == "MANUAL" && last_state.mode != "MANUAL")
    	{
    		last_state.mode = "MANUAL";
    		ROS_INFO("switch to mode: MANUAL");
    	}
    	if(current_state.mode == "POSCTL" && last_state.mode != "POSCTL")
    	{
    		last_state.mode = "POSCTL";
    		ROS_INFO("switch to mode: POSCTL");
    	}
    	if(current_state.mode == "OFFBOARD" && last_state.mode != "OFFBOARD")
    	{
    		last_state.mode = "OFFBOARD";
    		ROS_INFO("switch to mode: OFFBOARD");
    	}
    	if(current_state.armed && !last_state.armed)
    	 		{

    		last_state.armed = current_state.armed;
    		ROS_INFO("UAV armed!");
    	}

		// landing
		if(current_state.armed && current_mission_state == land)	/* set landing mode until uav stops. -libn */
		{
			if(current_state.mode != "AUTO.LAND" && (ros::Time::now() - last_request > ros::Duration(5.0)))
			{
				if(land_client.call(landing_cmd) && landing_cmd.response.success)
				{
					ROS_INFO("AUTO LANDING!");
				}
				last_request = ros::Time::now();
			}
		}

		/* mission state display. -libn */
		if(current_state.mode == "OFFBOARD" && current_state.armed)	/* set message display delay(0.5s). -libn */
		{
			ROS_INFO("now I am in OFFBOARD and armed mode!");	/* state machine! -libn */

			state_machine_func();

//			/* mission timer(5 loops). -libn */
//			if(ros::Time::now() - mission_timer_t > ros::Duration(100.0))
//			{
//				current_mission_state = mission_home;	/* mission timeout. -libn */
//				ROS_INFO("mission time out!");
//			}
//			/* subtask timer(1 loop). -libn */
//			if(current_mission_state >= mission_point_A && ros::Time::now() - loop_timer_t > ros::Duration(10.0))
//			{
//				loop++;
//				ROS_INFO("loop timeout");
//				current_mission_state = mission_point_A;	/* loop timeout, forced to switch to next loop. -libn */
//				/* TODO: mission failure recorded(using switch/case). -libn */
//
//			}

			ROS_INFO("current loop: %d",loop);
			ROS_INFO("current_mission_state: %d",current_mission_state);
			ROS_INFO("position*: %5.3f %5.3f %5.3f",pose_pub.pose.position.x,pose_pub.pose.position.y,pose_pub.pose.position.z);
			ROS_INFO("current position: %5.3f %5.3f %5.3f",current_pos.pose.position.x,current_pos.pose.position.y,current_pos.pose.position.z);
//			ROS_INFO("mission_timer_t = %.3f",mission_timer_t);
//			ROS_INFO("loop_timer_t = %.3f",loop_timer_t);
			ROS_INFO("board_position_received:\n"
					"board0: %d %5.3f %5.3f %5.3f \n"
					"board1: %d %5.3f %5.3f %5.3f \n"
					"board2: %d %5.3f %5.3f %5.3f \n"
					"board3: %d %5.3f %5.3f %5.3f \n"
					"board4: %d %5.3f %5.3f %5.3f \n"
					"board5: %d %5.3f %5.3f %5.3f \n"
					"board6: %d %5.3f %5.3f %5.3f \n"
					"board7: %d %5.3f %5.3f %5.3f \n"
					"board8: %d %5.3f %5.3f %5.3f \n"
					"board9: %d %5.3f %5.3f %5.3f \n",
					board10.drawingboard[0].valid,board10.drawingboard[0].x,board10.drawingboard[0].y,board10.drawingboard[0].z,
					board10.drawingboard[1].valid,board10.drawingboard[1].x,board10.drawingboard[1].y,board10.drawingboard[1].z,
					board10.drawingboard[2].valid,board10.drawingboard[2].x,board10.drawingboard[2].y,board10.drawingboard[2].z,
					board10.drawingboard[3].valid,board10.drawingboard[3].x,board10.drawingboard[3].y,board10.drawingboard[3].z,
					board10.drawingboard[4].valid,board10.drawingboard[4].x,board10.drawingboard[4].y,board10.drawingboard[4].z,
					board10.drawingboard[5].valid,board10.drawingboard[5].x,board10.drawingboard[5].y,board10.drawingboard[5].z,
					board10.drawingboard[6].valid,board10.drawingboard[6].x,board10.drawingboard[6].y,board10.drawingboard[6].z,
					board10.drawingboard[7].valid,board10.drawingboard[7].x,board10.drawingboard[7].y,board10.drawingboard[7].z,
					board10.drawingboard[8].valid,board10.drawingboard[8].x,board10.drawingboard[8].y,board10.drawingboard[8].z,
					board10.drawingboard[9].valid,board10.drawingboard[9].x,board10.drawingboard[9].y,board10.drawingboard[9].z);

		}
		else
		{
			if(current_state.mode != last_state_display.mode || last_state_display.armed != current_state.armed)
			{
				ROS_INFO("current_state.mode = %s",current_state.mode.c_str());
				ROS_INFO("last_state_display.mode = %s",last_state_display.mode.c_str());
				ROS_INFO("armed status: %d\n",current_state.armed);
				last_state_display.armed = current_state.armed;
				last_state_display.mode = current_state.mode;
				ROS_INFO("current position: %5.3f %5.3f %5.3f", current_pos.pose.position.x, 	  	current_pos.pose.position.y, current_pos.pose.position.z);

				ROS_INFO("setpoint_received:\n"
						"setpoint_A:%5.3f %5.3f %5.3f \n"
						"setpoint_B:%5.3f %5.3f %5.3f \n"
						"setpoint_C:%5.3f %5.3f %5.3f \n"
						"setpoint_D:%5.3f %5.3f %5.3f \n"
						"setpoint_H:%5.3f %5.3f %5.3f",
						setpoint_A.pose.position.x,setpoint_A.pose.position.y,setpoint_A.pose.position.z,
						setpoint_B.pose.position.x,setpoint_B.pose.position.y,setpoint_B.pose.position.z,
						setpoint_C.pose.position.x,setpoint_C.pose.position.y,setpoint_C.pose.position.z,
						setpoint_D.pose.position.x,setpoint_D.pose.position.y,setpoint_D.pose.position.z,
						setpoint_H.pose.position.x,setpoint_H.pose.position.y,setpoint_H.pose.position.z);

				ROS_INFO("board_position_received:\n"
						"board0: %d %5.3f %5.3f %5.3f \n"
						"board1: %d %5.3f %5.3f %5.3f \n"
						"board2: %d %5.3f %5.3f %5.3f \n"
						"board3: %d %5.3f %5.3f %5.3f \n"
						"board4: %d %5.3f %5.3f %5.3f \n"
						"board5: %d %5.3f %5.3f %5.3f \n"
						"board6: %d %5.3f %5.3f %5.3f \n"
						"board7: %d %5.3f %5.3f %5.3f \n"
						"board8: %d %5.3f %5.3f %5.3f \n"
						"board9: %d %5.3f %5.3f %5.3f \n",
						board10.drawingboard[0].valid,board10.drawingboard[0].x,board10.drawingboard[0].y,board10.drawingboard[0].z,
						board10.drawingboard[1].valid,board10.drawingboard[1].x,board10.drawingboard[1].y,board10.drawingboard[1].z,
						board10.drawingboard[2].valid,board10.drawingboard[2].x,board10.drawingboard[2].y,board10.drawingboard[2].z,
						board10.drawingboard[3].valid,board10.drawingboard[3].x,board10.drawingboard[3].y,board10.drawingboard[3].z,
						board10.drawingboard[4].valid,board10.drawingboard[4].x,board10.drawingboard[4].y,board10.drawingboard[4].z,
						board10.drawingboard[5].valid,board10.drawingboard[5].x,board10.drawingboard[5].y,board10.drawingboard[5].z,
						board10.drawingboard[6].valid,board10.drawingboard[6].x,board10.drawingboard[6].y,board10.drawingboard[6].z,
						board10.drawingboard[7].valid,board10.drawingboard[7].x,board10.drawingboard[7].y,board10.drawingboard[7].z,
						board10.drawingboard[8].valid,board10.drawingboard[8].x,board10.drawingboard[8].y,board10.drawingboard[8].z,
						board10.drawingboard[9].valid,board10.drawingboard[9].x,board10.drawingboard[9].y,board10.drawingboard[9].z);

			}
		}

		/* get 4 setpoints:A,B,C,D. -libn */
		switch(setpoint_indexed.index)
		{
			case 1:
				setpoint_A.pose.position.x = setpoint_indexed.x;
				setpoint_A.pose.position.y = setpoint_indexed.y;
				setpoint_A.pose.position.z = setpoint_indexed.z;
				break;
			case 2:
				setpoint_B.pose.position.x = setpoint_indexed.x;
				setpoint_B.pose.position.y = setpoint_indexed.y;
				setpoint_B.pose.position.z = setpoint_indexed.z;
				break;
			case 3:
				setpoint_C.pose.position.x = setpoint_indexed.x;
				setpoint_C.pose.position.y = setpoint_indexed.y;
				setpoint_C.pose.position.z = setpoint_indexed.z;
				break;
			case 4:
				setpoint_D.pose.position.x = setpoint_indexed.x;
				setpoint_D.pose.position.y = setpoint_indexed.y;
				setpoint_D.pose.position.z = setpoint_indexed.z;
				break;
			default:
				ROS_INFO("setpoint index error!");
				break;
		}

        if(current_mission_state == takeoff)
        {
        	local_vel_pub.publish(vel_pub);
        }
        else
        {
        	local_pos_pub.publish(pose_pub);
        }

        ros::spinOnce();
        rate.sleep();
    }

    return 0;
}

/* task state machine. -libn */
void state_machine_func(void)
{
	switch(current_mission_state)
	{
		case takeoff:
			/* local velocity setpoint publish. -libn */
			vel_pub.twist.linear.x = 0.0f;
			vel_pub.twist.linear.y = 0.0f;
			vel_pub.twist.linear.z = 3.0f;
			vel_pub.twist.angular.x = 0.0f;
			vel_pub.twist.angular.y = 0.0f;
			vel_pub.twist.angular.z = 0.0f;
			if((abs(current_pos.pose.position.x - current_pos.pose.position.x) < 0.2) &&      // switch to next state
			   (abs(current_pos.pose.position.y - current_pos.pose.position.y) < 0.2) &&
			   (abs(current_pos.pose.position.z - setpoint_H.pose.position.z) < 0.8))
			   {
					current_mission_state = takeoff_hover; // current_mission_state++;
					mission_last_time = ros::Time::now();
					mission_timer_t = ros::Time::now();
					setpoint_H.pose.position.x = current_pos.pose.position.x;
					setpoint_H.pose.position.y = current_pos.pose.position.y;
			   }
    		break;

        case takeoff_hover:
        	pose_pub.pose.position.x = current_pos.pose.position.x;
        	pose_pub.pose.position.y = current_pos.pose.position.y;
        	pose_pub.pose.position.z = setpoint_H.pose.position.z;
        	if(ros::Time::now() - mission_last_time > ros::Duration(5))	/* hover for 5 seconds. -libn */
        	{
        		current_mission_state = mission_point_A; // current_mission_state++;
        	}
            break;
        case mission_point_A:
        	if(loop > 5)
			{
				current_mission_state = mission_stop; // current_mission_state++;
				break;
			}
        	pose_pub.pose.position.x = setpoint_A.pose.position.x;
			pose_pub.pose.position.y = setpoint_A.pose.position.y;
			pose_pub.pose.position.z = setpoint_A.pose.position.z;
            if((abs(current_pos.pose.position.x - setpoint_A.pose.position.x) < 0.2) &&      // switch to next state
               (abs(current_pos.pose.position.y - setpoint_A.pose.position.y) < 0.2) &&
               (abs(current_pos.pose.position.z - setpoint_A.pose.position.z) < 0.2))
            {
            	current_mission_state = mission_point_A_hover_recognize; // current_mission_state++;
            	mission_last_time = ros::Time::now();
            }
            loop_timer_t = ros::Time::now();
            break;
        case mission_point_A_hover_recognize:
        	pose_pub.pose.position.x = setpoint_A.pose.position.x;
			pose_pub.pose.position.y = setpoint_A.pose.position.y;
			pose_pub.pose.position.z = setpoint_A.pose.position.z;
//			if(!display_screen_num_recognized)
//			{
//				/* TODO: to recognize the number. -libn */
//				display_screen_num_recognized = true;	/* number recognized. -libn */
//
//
//				if(loop == 1 && (ros::Time::now() - mission_last_time > ros::Duration(50)))	/* wait 50 seconds at most for the first time. -libn */
//				{
//					current_mission_state = mission_search; // current_mission_state++;
//					/* TODO: failure recorded. -libn */
//				}
//				else
//				{
//					if(loop!= 1 && ros::Time::now() - mission_last_time > ros::Duration(10))	/* wait 10 seconds at most for recognition. -libn */
//					{
//						current_mission_state = mission_search; // current_mission_state++;
//						/* TODO: failure recorded. -libn */
//					}
//				}
//			}
//			else
//			{
//				current_mission_state = mission_search; // current_mission_state++;
//			}
			//time delay added(just for test! --delete it directly!)
			if(ros::Time::now() - mission_last_time > ros::Duration(5))	/* hover for 5 seconds. -libn */
			{
				current_mission_state = mission_search; // current_mission_state++;
			}



			break;
        case mission_search:
        	if(board10.drawingboard[current_mission_num].valid)
        	{
        		pose_pub.pose.position.x = board10.drawingboard[current_mission_num].x;	/* TODO:switch to different board positions. -libn */
				pose_pub.pose.position.y = board10.drawingboard[current_mission_num].y;
				pose_pub.pose.position.z = board10.drawingboard[current_mission_num].z;
				if((abs(current_pos.pose.position.x - board10.drawingboard[current_mission_num].x) < 0.2) &&      // switch to next state
				   (abs(current_pos.pose.position.y - board10.drawingboard[current_mission_num].y) < 0.2) &&
				   (abs(current_pos.pose.position.z - board10.drawingboard[current_mission_num].z) < 0.2))
				{
					current_mission_state = mission_relocate; // current_mission_state++;
					mission_last_time = ros::Time::now();
				}
        	}
        	else
        	{
        		/* TODO: add scanning method! -libn */
        		current_mission_state = mission_scan; // current_mission_state++;
        		ROS_INFO("current mission state: mission_scan");
        	}
			break;
        case mission_relocate:
        	pose_pub.pose.position.x = board10.drawingboard[current_mission_num].x;	/* TODO:switch to different board positions. -libn */
			pose_pub.pose.position.y = board10.drawingboard[current_mission_num].y;
			pose_pub.pose.position.z = board10.drawingboard[current_mission_num].z;
			/* TODO:update the position of the drawing board.  -libn */
			relocate_valid = true;

			if(relocate_valid)
			{
				current_mission_state = mission_operate_move; // current_mission_state++;
			}
			else
			{
				if(ros::Time::now() - mission_last_time > ros::Duration(10))	/* wait for this operate 10 seconds at most. -libn */
				{
					/* TODO: add scanning method! -libn */
					current_mission_state = mission_scan; // current_mission_state++;
				}
			}
			break;
        case mission_operate_move:
        	pose_pub.pose.position.x = board10.drawingboard[current_mission_num].x;	/* TODO:switch to different board positions. -libn */
			pose_pub.pose.position.y = board10.drawingboard[current_mission_num].y;
			pose_pub.pose.position.z = board10.drawingboard[current_mission_num].z;
            if((abs(current_pos.pose.position.x - board10.drawingboard[current_mission_num].x) < 0.2) &&      // switch to next state
               (abs(current_pos.pose.position.y - board10.drawingboard[current_mission_num].y) < 0.2) &&
               (abs(current_pos.pose.position.z - board10.drawingboard[current_mission_num].z) < 0.2))
               {
            	current_mission_state = mission_operate_hover; // current_mission_state++;
            	mission_last_time = ros::Time::now();
               }
            break;
        case mission_operate_hover:
        	pose_pub.pose.position.x = board10.drawingboard[current_mission_num].x;	/* TODO:switch to different board positions. -libn */
			pose_pub.pose.position.y = board10.drawingboard[current_mission_num].y;
			pose_pub.pose.position.z = board10.drawingboard[current_mission_num].z;
        	if(ros::Time::now() - mission_last_time > ros::Duration(5))	/* hover for 5 seconds. -libn */
        	{
        		current_mission_state = mission_operate_spray; // current_mission_state++;
        		mission_last_time = ros::Time::now();
        		/* TODO: start spraying. -libn */

        	}
            break;
        case mission_operate_spray:
        	pose_pub.pose.position.x = board10.drawingboard[current_mission_num].x;	/* TODO:switch to different board positions. -libn */
			pose_pub.pose.position.y = board10.drawingboard[current_mission_num].y;
			pose_pub.pose.position.z = board10.drawingboard[current_mission_num].z;
            if(ros::Time::now() - mission_last_time > ros::Duration(1))	/* spray for 1 seconds. -libn */
        	{
        		/* TODO: stop spraying. -libn */

            	loop++;	/* switch to next loop. -libn */
            	if(loop > 5)
            	{
            		current_mission_state = mission_stop; // current_mission_state++;
            	}
            	else
            	{
            		current_mission_state = mission_point_A; // current_mission_state++;
            	}
        	}
            break;
        case mission_stop:
        	loop_timer_t = ros::Time::now();	/* disable loop_timer. -libn */
        	pose_pub.pose.position.x = current_pos.pose.position.x;	/* hover in current position. -libn */
        	pose_pub.pose.position.y = current_pos.pose.position.y;
        	pose_pub.pose.position.z = current_pos.pose.position.z;
        	/* TODO: mission check: if there are failures to be fixed -libn */
        	if(mission_failure != 0)
        	{
        		current_mission_state = mission_fix_failure; // current_mission_state++;
        		ROS_INFO("TODO: mission_fix_failure!");
        	}
        	else			/* mission is finished. -libn */
        	{
        		current_mission_state = mission_home; // current_mission_state++;
        		ROS_INFO("going to mission_home");
        		mission_last_time = ros::Time::now();
        	}
			break;
        case mission_fix_failure:
        	loop_timer_t = ros::Time::now();	/* disable loop_timer. -libn */
			/* TODO: add mission_failure_fixed. -libn */

        	pose_pub.pose.position.x = current_pos.pose.position.x;	/* hover in current position. -libn */
			pose_pub.pose.position.y = current_pos.pose.position.y;
			pose_pub.pose.position.z = current_pos.pose.position.z;
			if(ros::Time::now() - mission_last_time > ros::Duration(10))	/* just to keep safe. -libn */
			{
				current_mission_state = mission_home; // current_mission_state++;
			}
			break;

		case mission_home:
			loop_timer_t = ros::Time::now();	/* disable loop_timer. -libn */
			pose_pub.pose.position.x = setpoint_H.pose.position.x;
			pose_pub.pose.position.y = setpoint_H.pose.position.y;
			pose_pub.pose.position.z = setpoint_H.pose.position.z;
//			ROS_INFO("start mission_home");
//			ROS_INFO("setpoint_H*: %5.3f %5.3f %5.3f",setpoint_H.pose.position.x,setpoint_H.pose.position.y,setpoint_H.pose.position.z);
//			ROS_INFO("current position --2 : %5.3f %5.3f %5.3f",current_pos.pose.position.x,current_pos.pose.position.y,current_pos.pose.position.z);
			/* Bug! -libn */
			if((abs(current_pos.pose.position.x - setpoint_H.pose.position.x) < 0.2) &&      // switch to next state
			   (abs(current_pos.pose.position.y - setpoint_H.pose.position.y) < 0.2) &&
			   (abs(current_pos.pose.position.z - setpoint_H.pose.position.z) < 0.2) &&
			   (ros::Time::now() - mission_last_time > ros::Duration(5)))		/* Bug: mission_last_time is not necessary! -libn */
			{
				ROS_INFO("start mission_home_hover");
				current_mission_state = mission_home_hover; // current_mission_state++;
				mission_last_time = ros::Time::now();
			}
			break;
		case mission_home_hover:
			loop_timer_t = ros::Time::now();	/* disable loop_timer. -libn */
			pose_pub.pose.position.x = setpoint_H.pose.position.x;
			pose_pub.pose.position.y = setpoint_H.pose.position.y;
			pose_pub.pose.position.z = setpoint_H.pose.position.z;
			if(ros::Time::now() - mission_last_time > ros::Duration(2))	/* hover for 2 seconds. -libn */
			{
				current_mission_state = land; // current_mission_state++;
			}
			break;
        case land:
        	loop_timer_t = ros::Time::now();	/* disable loop_timer. -libn */
			break;
    }

}
