 /**
* @file     : offb_simulation_test.cpp
* @brief    : offboard simulation test: demo rewritten -> 4 setpoints flight -> complete state machine.
* @author   : libn
* @Time     : Aug 25, 201610:06:42 PM
*/

#include <ros/ros.h>
#include <geometry_msgs/PoseStamped.h>
#include "state_machine/CommandBool.h"
#include "state_machine/SetMode.h"
#include "state_machine/State.h"
#include "state_machine/CommandTOL.h"
#include "state_machine/Setpoint.h"
#include "state_machine/DrawingBoard.h"

/* switch environment between real flight and simulation(0: simulation;1: real flight). -libn */
#define environment_switch 0	/* 0: simulation;1: real flight. -libn */

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


ros::Time mission_timer_t;	/* timer to control the whole mission. -libn */
ros::Time loop_timer_t;	/* timer to control subtask. -libn */

state_machine::State current_state;
void state_cb(const state_machine::State::ConstPtr& msg){
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

/* 4 setpoints. -libn */
geometry_msgs::PoseStamped setpoint_A;
geometry_msgs::PoseStamped setpoint_B;
geometry_msgs::PoseStamped setpoint_C;
geometry_msgs::PoseStamped setpoint_D;
geometry_msgs::PoseStamped setpoint_H;	/* home position. -libn */

geometry_msgs::PoseStamped pose_pub;

state_machine::DrawingBoard board;
void DrawingBoardCallback(const state_machine::DrawingBoard::ConstPtr& msg)
{
	board = *msg;
}
/* 10 drawing board position. -libn */
state_machine::DrawingBoard board_1;
state_machine::DrawingBoard board_2;
state_machine::DrawingBoard board_3;
state_machine::DrawingBoard board_4;
state_machine::DrawingBoard board_5;
state_machine::DrawingBoard board_6;
state_machine::DrawingBoard board_7;
state_machine::DrawingBoard board_8;
state_machine::DrawingBoard board_9;
state_machine::DrawingBoard board_0;


int main(int argc, char **argv)
{
    ros::init(argc, argv, "offb_node");
    ros::NodeHandle nh;

    ros::Subscriber state_sub = nh.subscribe<state_machine::State>
            ("mavros/state", 10, state_cb);
    ros::Publisher local_pos_pub = nh.advertise<geometry_msgs::PoseStamped>
            ("mavros/setpoint_position/local", 10);
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

	/* receive 10 drawing board position. -libn */
	ros::Subscriber drawingboard_Indexed_sub = nh.subscribe("DrawingBoard_Position", 100 ,DrawingBoardCallback);

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

	setpoint_H.pose.position.x = 0.0f;
	setpoint_H.pose.position.y = 0.0f;
	setpoint_H.pose.position.z = 2.0f;

	board_0.valid = false;	/* used to check if the positon of the board is valid. -libn */
	board_1.valid = false;
	board_2.valid = false;
	board_3.valid = false;
	board_4.valid = false;
	board_5.valid = false;
	board_6.valid = false;
	board_7.valid = false;
	board_8.valid = false;
	board_9.valid = false;

    //send a few setpoints before starting
    for(int i = 100; ros::ok() && i > 0; --i){
        local_pos_pub.publish(pose_pub);
        ros::spinOnce();
        rate.sleep();
    }

    state_machine::SetMode offb_set_mode;
    offb_set_mode.request.custom_mode = "OFFBOARD";

    state_machine::CommandBool arm_cmd;
    arm_cmd.request.value = true;

    ros::Time last_request = ros::Time::now();
    ros::Time last_show_request = ros::Time::now();
    loop_timer_t = ros::Time::now();

    while(ros::ok()){
        if( current_state.mode != "OFFBOARD" && current_state.mode != "AUTO.LAND" &&	/* set offboard mode for the first time. -libn */
          (ros::Time::now() - last_request > ros::Duration(5.0)))
        {
            if( set_mode_client.call(offb_set_mode) &&
                offb_set_mode.response.success){
                ROS_INFO("Offboard enabled");
            }
            last_request = ros::Time::now();
        } else {
            if( !current_state.armed && current_state.mode != "AUTO.LAND" &&	/* set armed for the first time. -libn */
                (ros::Time::now() - last_request > ros::Duration(5.0))){
                if( arming_client.call(arm_cmd) &&
                    arm_cmd.response.success){
                    ROS_INFO("Vehicle armed");
                    loop_timer_t = ros::Time::now();
                    mission_timer_t = ros::Time::now();
                }
                last_request = ros::Time::now();
            }

        }

        /* for real flight. -libn */
		#if switch_mode == 1
        // auto task off
		if( current_state.mode == "AUTO.TAKEOFF"){
			ROS_INFO("AUTO TAKEOFF!");
		}
		#endif

		// landing
		if(current_state.armed && current_mission_state == land)	/* set landing mode until uav stopped. -libn */
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

		/* get 10 drawing board's letter and position. -libn */
		switch(board.num)
		{
			case 0:
				board_0 = board;
				break;
			case 1:
				board_1 = board;
				break;
			case 2:
				board_2 = board;
				break;
			case 3:
				board_3 = board;
				break;
			case 4:
				board_4 = board;
				break;
			case 5:
				board_5 = board;
				break;
			case 6:
				board_6 = board;
				break;
			case 7:
				board_7 = board;
				break;
			case 8:
				board_8 = board;
				break;
			case 9:
				board_9 = board;
				break;
			default:
				ROS_INFO("board index error!");
				break;
		}

		if(current_state.mode == "OFFBOARD" && current_state.armed)
		{
			ROS_INFO("state machine started!");
			state_machine_func();

			/* mission timer(5 loops). -libn */
			if(ros::Time::now() - mission_timer_t > ros::Duration(100.0))
			{
				current_mission_state = mission_home;	/* mission timeout. -libn */
				ROS_INFO("mission time out!");
			}
			/* subtask timer(1 loop). -libn */
			if(current_mission_state >= mission_point_A && ros::Time::now() - loop_timer_t > ros::Duration(10.0))
			{
				loop++;
				ROS_INFO("loop timeout");
				current_mission_state = mission_point_A;	/* loop timeout, forced to switch to next loop. -libn */
				/* TODO: mission failure recorded(using switch/case). -libn */

			}
		}

		/* mission state display. -libn */
		if(current_state.mode == "OFFBOARD"
				&& current_state.armed
//				&& ros::Time::now() - last_show_request > ros::Duration(0.5)
		)	/* set message display delay(0.5s). -libn */
		{
			ROS_INFO("current loop: %d",loop);
			ROS_INFO("current_mission_state: %d",current_mission_state);
			ROS_INFO("position*: %5.3f %5.3f %5.3f",pose_pub.pose.position.x,pose_pub.pose.position.y,pose_pub.pose.position.z);
			ROS_INFO("current position: %5.3f %5.3f %5.3f",current_pos.pose.position.x,current_pos.pose.position.y,current_pos.pose.position.z);
			ROS_INFO("mission_timer_t = %.3f",mission_timer_t);
			ROS_INFO("loop_timer_t = %.3f",loop_timer_t);
//			ROS_INFO("setpoint_received:\n"
//					"setpoint_A:%5.3f %5.3f %5.3f \n"
//					"setpoint_B:%5.3f %5.3f %5.3f \n"
//					"setpoint_C:%5.3f %5.3f %5.3f \n"
//					"setpoint_D:%5.3f %5.3f %5.3f \n"
//					"setpoint_H:%5.3f %5.3f %5.3f",
//					setpoint_A.pose.position.x,setpoint_A.pose.position.y,setpoint_A.pose.position.z,
//					setpoint_B.pose.position.x,setpoint_B.pose.position.y,setpoint_B.pose.position.z,
//					setpoint_C.pose.position.x,setpoint_C.pose.position.y,setpoint_C.pose.position.z,
//					setpoint_D.pose.position.x,setpoint_D.pose.position.y,setpoint_D.pose.position.z,
//					setpoint_H.pose.position.x,setpoint_H.pose.position.y,setpoint_H.pose.position.z);

//			ROS_INFO("board_position_received:\n"
//					"board0: %d %5.3f %5.3f %5.3f \n"
//					"board1: %d %5.3f %5.3f %5.3f \n"
//					"board2: %d %5.3f %5.3f %5.3f \n"
//					"board3: %d %5.3f %5.3f %5.3f \n"
//					"board4: %d %5.3f %5.3f %5.3f \n"
//					"board5: %d %5.3f %5.3f %5.3f \n"
//					"board6: %d %5.3f %5.3f %5.3f \n"
//					"board7: %d %5.3f %5.3f %5.3f \n"
//					"board8: %d %5.3f %5.3f %5.3f \n"
//					"board9: %d %5.3f %5.3f %5.3f \n",
//					board_0.valid,board_0.x,board_0.y,board_0.z,
//					board_1.valid,board_1.x,board_1.y,board_1.z,
//					board_2.valid,board_2.x,board_2.y,board_2.z,
//					board_3.valid,board_3.x,board_3.y,board_3.z,
//					board_4.valid,board_4.x,board_4.y,board_4.z,
//					board_5.valid,board_5.x,board_5.y,board_5.z,
//					board_6.valid,board_6.x,board_6.y,board_6.z,
//					board_7.valid,board_7.x,board_7.y,board_7.z,
//					board_8.valid,board_8.x,board_8.y,board_8.z,
//					board_9.valid,board_9.x,board_9.y,board_9.z);
			ROS_INFO("");
			last_show_request = ros::Time::now();
		}

        local_pos_pub.publish(pose_pub);
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
			pose_pub.pose.position.x = setpoint_H.pose.position.x;
			pose_pub.pose.position.y = setpoint_H.pose.position.y;
			pose_pub.pose.position.z = setpoint_H.pose.position.z;
			if((abs(current_pos.pose.position.x - setpoint_H.pose.position.x) < 0.2) &&      // switch to next state
			   (abs(current_pos.pose.position.y - setpoint_H.pose.position.y) < 0.2) &&
			   (abs(current_pos.pose.position.z - setpoint_H.pose.position.z) < 0.2))
			   {
					current_mission_state = takeoff_hover; // current_mission_state++;
					mission_last_time = ros::Time::now();
					mission_timer_t = ros::Time::now();
			   }
    		break;

        case takeoff_hover:
        	pose_pub.pose.position.x = setpoint_H.pose.position.x;
        	pose_pub.pose.position.y = setpoint_H.pose.position.y;
        	pose_pub.pose.position.z = setpoint_H.pose.position.z;
        	if(ros::Time::now() - mission_last_time > ros::Duration(2))	/* hover for 2 seconds. -libn */
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
			if(!display_screen_num_recognized)
			{
				/* TODO: to recognize the number. -libn */
				display_screen_num_recognized = true;	/* number recognized. -libn */


				if(loop == 1 && (ros::Time::now() - mission_last_time > ros::Duration(50)))	/* wait 50 seconds at most for the first time. -libn */
				{
					current_mission_state = mission_search; // current_mission_state++;
					/* TODO: failure recorded. -libn */
				}
				else
				{
					if(loop!= 1 && ros::Time::now() - mission_last_time > ros::Duration(10))	/* wait 10 seconds at most for recognition. -libn */
					{
						current_mission_state = mission_search; // current_mission_state++;
						/* TODO: failure recorded. -libn */
					}
				}
			}
			else
			{
				current_mission_state = mission_search; // current_mission_state++;
			}
			break;
        case mission_search:
        	if(board_0.valid)
        	{
        		pose_pub.pose.position.x = board_0.x;	/* TODO:switch to different board positions. -libn */
				pose_pub.pose.position.y = board_0.y;
				pose_pub.pose.position.z = board_0.z;
				if((abs(current_pos.pose.position.x - board_0.x) < 0.2) &&      // switch to next state
				   (abs(current_pos.pose.position.y - board_0.y) < 0.2) &&
				   (abs(current_pos.pose.position.z - board_0.z) < 0.2))
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
    		pose_pub.pose.position.x = board_0.x;	/* TODO:switch to different board positions. -libn */
			pose_pub.pose.position.y = board_0.y;
			pose_pub.pose.position.z = board_0.z;
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
    		pose_pub.pose.position.x = board_0.x;	/* TODO:switch to different board positions. -libn */
			pose_pub.pose.position.y = board_0.y;
			pose_pub.pose.position.z = board_0.z;
            if((abs(current_pos.pose.position.x - board_0.x) < 0.2) &&      // switch to next state
               (abs(current_pos.pose.position.y - board_0.y) < 0.2) &&
               (abs(current_pos.pose.position.z - board_0.z) < 0.2))
               {
            	current_mission_state = mission_operate_hover; // current_mission_state++;
            	mission_last_time = ros::Time::now();
               }
            break;
        case mission_operate_hover:
        	pose_pub.pose.position.x = board_0.x;	/* TODO:switch to different board positions. -libn */
			pose_pub.pose.position.y = board_0.y;
			pose_pub.pose.position.z = board_0.z;
        	if(ros::Time::now() - mission_last_time > ros::Duration(1))	/* hover for 1 seconds. -libn */
        	{
        		current_mission_state = mission_operate_spray; // current_mission_state++;
        		mission_last_time = ros::Time::now();
        		/* TODO: start spraying. -libn */

        	}
            break;
        case mission_operate_spray:
        	pose_pub.pose.position.x = board_0.x;	/* TODO:switch to different board positions. -libn */
			pose_pub.pose.position.y = board_0.y;
			pose_pub.pose.position.z = board_0.z;
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
