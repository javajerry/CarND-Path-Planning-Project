#include <fstream>
#include <math.h>
#include <uWS/uWS.h>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include "Eigen-3.3/Eigen/Core"
#include "Eigen-3.3/Eigen/QR"
#include "json.hpp"
#include "spline.h"

using namespace std;

// for convenience
using json = nlohmann::json;

// For converting back and forth between radians and degrees.
constexpr double pi() { return M_PI; }
double deg2rad(double x) { return x * pi() / 180; }
double rad2deg(double x) { return x * 180 / pi(); }

//reset the speed 
bool reset_terminal;

// maximum velocity
double MAX_VELOCITY = 49.5;

// Checks if the SocketIO event has JSON data.
// If there is data the JSON object in string format will be returned,
// else the empty string "" will be returned.
string hasData(string s) {
  auto found_null = s.find("null");
  auto b1 = s.find_first_of("[");
  auto b2 = s.find_first_of("}");
  if (found_null != string::npos) {
    return "";
  } else if (b1 != string::npos && b2 != string::npos) {
    return s.substr(b1, b2 - b1 + 2);
  }
  return "";
}

double distance(double x1, double y1, double x2, double y2)
{
	return sqrt((x2-x1)*(x2-x1)+(y2-y1)*(y2-y1));
}
int ClosestWaypoint(double x, double y, vector<double> maps_x, vector<double> maps_y)
{

	double closestLen = 100000; //large number
	int closestWaypoint = 0;

	for(int i = 0; i < maps_x.size(); i++)
	{
		double map_x = maps_x[i];
		double map_y = maps_y[i];
		double dist = distance(x,y,map_x,map_y);
		if(dist < closestLen)
		{
			closestLen = dist;
			closestWaypoint = i;
		}

	}

	return closestWaypoint;

}

int NextWaypoint(double x, double y, double theta, vector<double> maps_x, vector<double> maps_y)
{

	int closestWaypoint = ClosestWaypoint(x,y,maps_x,maps_y);

	double map_x = maps_x[closestWaypoint];
	double map_y = maps_y[closestWaypoint];

	double heading = atan2( (map_y-y),(map_x-x) );

	double angle = abs(theta-heading);

	if(angle > pi()/4)
	{
		closestWaypoint++;
	}

	return closestWaypoint;

}

// Transform from Cartesian x,y coordinates to Frenet s,d coordinates
vector<double> getFrenet(double x, double y, double theta, vector<double> maps_x, vector<double> maps_y)
{
	int next_wp = NextWaypoint(x,y, theta, maps_x,maps_y);

	int prev_wp;
	prev_wp = next_wp-1;
	if(next_wp == 0)
	{
		prev_wp  = maps_x.size()-1;
	}

	double n_x = maps_x[next_wp]-maps_x[prev_wp];
	double n_y = maps_y[next_wp]-maps_y[prev_wp];
	double x_x = x - maps_x[prev_wp];
	double x_y = y - maps_y[prev_wp];

	// find the projection of x onto n
	double proj_norm = (x_x*n_x+x_y*n_y)/(n_x*n_x+n_y*n_y);
	double proj_x = proj_norm*n_x;
	double proj_y = proj_norm*n_y;

	double frenet_d = distance(x_x,x_y,proj_x,proj_y);

	//see if d value is positive or negative by comparing it to a center point

	double center_x = 1000-maps_x[prev_wp];
	double center_y = 2000-maps_y[prev_wp];
	double centerToPos = distance(center_x,center_y,x_x,x_y);
	double centerToRef = distance(center_x,center_y,proj_x,proj_y);

	if(centerToPos <= centerToRef)
	{
		frenet_d *= -1;
	}

	// calculate s value
	double frenet_s = 0;
	for(int i = 0; i < prev_wp; i++)
	{
		frenet_s += distance(maps_x[i],maps_y[i],maps_x[i+1],maps_y[i+1]);
	}

	frenet_s += distance(0,0,proj_x,proj_y);

	return {frenet_s,frenet_d};

}

// Transform from Frenet s,d coordinates to Cartesian x,y
vector<double> getXY(double s, double d, vector<double> maps_s, vector<double> maps_x, vector<double> maps_y)
{
	int prev_wp = -1;

	while(s > maps_s[prev_wp+1] && (prev_wp < (int)(maps_s.size()-1) ))
	{
		prev_wp++;
	}

	int wp2 = (prev_wp+1)%maps_x.size();

	double heading = atan2((maps_y[wp2]-maps_y[prev_wp]),(maps_x[wp2]-maps_x[prev_wp]));
	// the x,y,s along the segment
	double seg_s = (s-maps_s[prev_wp]);

	double seg_x = maps_x[prev_wp]+seg_s*cos(heading);
	double seg_y = maps_y[prev_wp]+seg_s*sin(heading);

	double perp_heading = heading-pi()/2;

	double x = seg_x + d*cos(perp_heading);
	double y = seg_y + d*sin(perp_heading);

	return {x,y};

}

// obtain laneId based car's d coorinate
int getLane (double d_val)
{
  int car_lane = -1;
  int lane_width = 4;

  if ((d_val > 0 ) && (d_val < lane_width ))
    car_lane = 0;
  else if ((d_val > lane_width ) && (d_val < 2*lane_width ))
    car_lane = 1;
  else if((d_val > 2*lane_width ) && (d_val < 3*lane_width ))
    car_lane = 2;

  return car_lane;
}

vector<int> countCarsinCloseRange(vector<vector<double>> sensor_data, double car_s)
{
  int leftAhead=0, leftBehind=0,  centerAhead=0, centerBehind=0, rightAhead=0, rightBehind=0;
  for ( int i=0; i<sensor_data.size(); i++)
  {
    double s = sensor_data[i][5];
    double d = sensor_data[i][6];
    double collison_dist = 30;

    if ( getLane(d) == 0)
    {
      if ( s - car_s < collison_dist ) 
      {
        leftAhead += 1;  
      }
      else if ( car_s - s < collison_dist ) 
      {
        leftBehind += 1;  
      }      
    }
    else if ( getLane(d) == 1)
    {
      if ( s - car_s < collison_dist ) 
      {
        centerAhead += 1;  
      }
      else if ( car_s - s < collison_dist ) 
      {
        centerBehind += 1;  
      }      
    }
    else if ( getLane(d) == 2)
    {
      if ( s - car_s < collison_dist ) 
      {
        rightAhead += 1;  
      }
      else if ( car_s - s < collison_dist ) 
      {
        rightBehind += 1;  
      }      
    }
  }

  return {leftAhead, leftBehind, centerAhead, centerBehind, rightAhead, rightBehind};
}

// Analyze the sensor data and categorize into 6 parts and find the minimum distance between ego car and other cars
// in each lane.
vector<double> analyzeSensorData(vector<vector<double>> sensor_data,  int prev_size, double car_s, double car_d)
{
  double left_ahead_closest = 99999;
  double center_ahead_closest = 99999;
  double right_ahead_closest = 99999;

  double left_behind_closest = 99999;
  double center_behind_closest = 99999;
  double right_behind_closest = 99999;

  //get current lane
  int lane = getLane(car_d);

  // each vector will 3 parameter, difference in distance, collision_ind[0,1] and buffer_ind
  for ( int i=0; i<sensor_data.size(); i++)
  {
    double vx = sensor_data[i][3];
    double vy = sensor_data[i][4];

    double s = sensor_data[i][5];
    double d = sensor_data[i][6];

    double check_speed = sqrt(vx*vx + vy*vy);
    s += ((double)prev_size*.02*check_speed);

    double diff_ahead = s - car_s;
    double diff_behind = car_s - s;


    if ( lane == getLane(d) )
    {
      if ( (diff_ahead > 0) and (diff_ahead  < center_ahead_closest )) 
      {
        center_ahead_closest = diff_ahead;  
      }
      if ( (diff_behind > 0) and (diff_behind  < center_behind_closest )) 
      {
        center_behind_closest = diff_behind;  
      }
    }
    else if ( lane>0 && (lane == getLane(d)+1))  //car on left
    {
      if ( (diff_ahead > 0) and (diff_ahead  < left_ahead_closest )) 
      {
        left_ahead_closest = diff_ahead;  
      }
      if ( (diff_behind > 0) and (diff_behind  < left_behind_closest )) 
      {
        left_behind_closest = diff_behind;  
      }

    }
    else if ( lane == (getLane(d)-1)) // car on right
    {
      if ( (diff_ahead > 0) and (diff_ahead  < right_ahead_closest )) 
      {
        right_ahead_closest = diff_ahead;  
      }
      if ( (diff_behind > 0) and (diff_behind  < right_behind_closest )) 
      {
        right_behind_closest = diff_behind;  
      }

    }
  }

  return {left_ahead_closest, left_behind_closest, center_ahead_closest, center_behind_closest, right_ahead_closest, right_behind_closest};
}

// Analyze the sensor data and categorize into 6 parts and find the minimum distance between ego car and other cars
// in each lane.
vector<double> averageLaneSpeed(vector<vector<double>> sensor_data,  int prev_size)
{
	double left_lane_speed = 0;
	double center_lane_speed = 0;
	double right_lane_speed = 0;

  	double left_cars_count = 0;
  	double center_cars_count = 0;
  	double right_cars_count = 0;


  	// each vector will 3 parameter, difference in distance, collision_ind[0,1] and buffer_ind
  	for ( int i=0; i<sensor_data.size(); i++)
  	{
    	double vx = sensor_data[i][3];
    	double vy = sensor_data[i][4];
    	double d = sensor_data[i][6];

    	double check_speed = sqrt(vx*vx + vy*vy);

    	int lane = getLane(d);

    	if ( lane ==  0 ) //left lane
    	{
    		left_lane_speed += check_speed;
    		left_cars_count += 1;
    	}
    	else if ( lane == 1 )  //middle lane
    	{
    		center_lane_speed += check_speed;
    		center_cars_count += 1;
    	}
    	else if ( lane == 2) // right lane
    	{
    		right_lane_speed += check_speed;
    		right_cars_count += 1;
    	}

  	}

	if ( left_cars_count > 0 )
    {
    	left_lane_speed = left_lane_speed / left_cars_count;
    }
    else
    {
    	left_lane_speed = MAX_VELOCITY;
    }

    if ( center_cars_count > 0 )
    {
    	center_lane_speed = center_lane_speed / center_cars_count;
    }
    else
    {
    	center_lane_speed = MAX_VELOCITY;
    }

    if ( right_cars_count > 0 )
    {
    	right_lane_speed = right_lane_speed / right_cars_count;
    }
    else
    {
    	right_lane_speed = MAX_VELOCITY;
    }

  return {left_lane_speed, center_lane_speed, right_lane_speed};
}

double collision_cost(vector<double> data)
{
    
  // Binary cost function which penalizes collisions.
    
  auto nearest = min_element(data.begin(), data.end());
  // if less than 30 meters, collision detected
  if (*nearest < 30.0) 
  {
    return 1.0;
  }
  else 
  { 
    return 0.0;
  }
}

double logistic(double x)
{  
    //A function that returns a value between 0 and 1 for x in the 
    //range [0, infinity] and -1 to 1 for x in the range [-infinity, infinity].

    //Useful for cost functions.
    return 2.0 / (1 + exp(-x)) - 1.0;
}

double buffer_cost(vector<double> data)
{  
    // Penalizes getting close to other vehicles.
	auto nearest = min_element(data.begin(), data.end());

    //return logistic(50 / *nearest);
     // if less than 30 meters, collision detected
  	if (*nearest < 40.0) 
  	{
    	return 1.0;
  	}
  	else 
  	{ 
    	return 0.0;
  	}
}


int main() {
  uWS::Hub h;

  // Load up map values for waypoint's x,y,s and d normalized normal vectors
  vector<double> map_waypoints_x;
  vector<double> map_waypoints_y;
  vector<double> map_waypoints_s;
  vector<double> map_waypoints_dx;
  vector<double> map_waypoints_dy;

 // start in lane 1
  int lane = 1;

  // reference velocity to the target
  double ref_vel = 0;

  // CPU_cycles. Since my PC is slow so need faster for slowing down or acceleraing
  double cpu_cycles = 3;

  // Waypoint map to read from
  string map_file_ = "../data/highway_map.csv";
  // The max s value before wrapping around the track back to 0
  double max_s = 6945.554;

  ifstream in_map_(map_file_.c_str(), ifstream::in);

  string line;
  while (getline(in_map_, line)) {
  	istringstream iss(line);
  	double x;
  	double y;
  	float s;
  	float d_x;
  	float d_y;
  	iss >> x;
  	iss >> y;
  	iss >> s;
  	iss >> d_x;
  	iss >> d_y;
  	map_waypoints_x.push_back(x);
  	map_waypoints_y.push_back(y);
  	map_waypoints_s.push_back(s);
  	map_waypoints_dx.push_back(d_x);
  	map_waypoints_dy.push_back(d_y);
  }

 
  h.onMessage([&map_waypoints_x,&map_waypoints_y,&map_waypoints_s,&map_waypoints_dx,&map_waypoints_dy, &lane, &ref_vel, &cpu_cycles](uWS::WebSocket<uWS::SERVER> ws, char *data, size_t length,
                     uWS::OpCode opCode) {
    // "42" at the start of the message means there's a websocket message event.
    // The 4 signifies a websocket message
    // The 2 signifies a websocket event
    //auto sdata = string(data).substr(0, length);
    //cout << sdata << endl;
    if (length && length > 2 && data[0] == '4' && data[1] == '2') {

      auto s = hasData(data);

      if (s != "") {
        auto j = json::parse(s);
        
        string event = j[0].get<string>();
        
        if (event == "telemetry") {
          // j[1] is the data JSON object
          
        	// Main car's localization Data
          	double car_x = j[1]["x"];
          	double car_y = j[1]["y"];
          	double car_s = j[1]["s"];
          	double car_d = j[1]["d"];
          	double car_yaw = j[1]["yaw"];
          	double car_speed = j[1]["speed"];

          	// Previous path data given to the Planner
          	auto previous_path_x = j[1]["previous_path_x"];
          	auto previous_path_y = j[1]["previous_path_y"];
          	// Previous path's end s and d values 
          	double end_path_s = j[1]["end_path_s"];
          	double end_path_d = j[1]["end_path_d"];

          	// Sensor Fusion Data, a list of all other cars on the same side of the road.
          	auto sensor_fusion = j[1]["sensor_fusion"];

          	json msgJson;

          	vector<double> next_x_vals;
          	vector<double> next_y_vals;

          	// if simulator is stopped and started again, reset the velocity
          	if ( reset_terminal)
          	{
          		ref_vel = 1;
          		reset_terminal = false;
          	}

            int prev_size = previous_path_x.size();

            if ( prev_size > 0)
            {
              car_s = end_path_s;
            }

            bool too_close = false;

            double avg_speed = 0;

            lane = getLane(car_d);
            cout << "Ego lane:" << lane << "   car_d: " << car_d << endl; 

          	// TODO: define a path made up of (x,y) points that the car will visit sequentially every .02 seconds
            //Analyze sensor fusion data
            //vector<int> traffic = countCarsinCloseRange(sensor_fusion, car_s);
            //cout << "Found Number of cars :" << sensor_fusion.size() << "| lA:lB:cA:cB:rA:rB " << traffic[0] << ":" << traffic[1] << ":" << traffic[2] << ":" << traffic[3] << ":" << traffic[4] << ":" << traffic[5] << endl;
            for (int i = 0; i < sensor_fusion.size(); ++i)
            {
              auto vehicle = sensor_fusion[i];
              cout << "vehicle:" << vehicle << endl;
            }

            vector<double> distance_data = analyzeSensorData(sensor_fusion, prev_size, car_s, car_d);
            vector<double> lanes_avg_speed = averageLaneSpeed(sensor_fusion, prev_size);
            
            cout << "left Ahead Distance :" << distance_data[0] << " | Left Behind Distance " << distance_data[1] <<  endl;
            cout << "center Ahead Distance :" << distance_data[2] << " | center Behind Distance " << distance_data[3] <<  endl;
            cout << "right Ahead Distance :" << distance_data[4] << " | right Behind Distance " << distance_data[5] <<  endl;

            // decision making
            if ( lane == 1) // center lane
            {
            	double coll_ahead_cost = collision_cost({distance_data[2]});
              	cout << "collision cost : " << coll_ahead_cost << endl;
              	// check if collision detected in same lane ahead as well as in 
              	// left and right lane  
              	double coll_left_cost = collision_cost({distance_data[0],distance_data[1]});
              	double coll_right_cost = collision_cost({distance_data[4],distance_data[5]});
              	cout << "left coll cost : " << coll_left_cost << "  | right coll cost : " << coll_right_cost << endl;

              	double coll_cost = coll_ahead_cost + coll_left_cost + coll_right_cost;
			  	cout << "** total collision cost : " << coll_cost << endl;
              
              	if ( coll_cost == 3 ) // slow down, surrounded by cars in all lanes
              	{
                	too_close = true;
                	avg_speed = lanes_avg_speed[1]; 
                	cout << "slow down" << endl;
              	}
              	else if ( coll_cost >= 1 and coll_cost < 3) //check for possible lane change
              	{
              		if ( coll_ahead_cost == 1.0 && coll_left_cost == 0.0 && coll_right_cost == 1.0)
              		{
              			double left_buffer = buffer_cost({distance_data[0],distance_data[1]});
              			cout << "left up buffer : " << left_buffer << endl;

						if ( left_buffer == 0.0 )
						{
                    		lane = 0; //change lane to left
                    		cout << "change lane to left" << endl;
						}
						else
						{
							too_close = true;
							avg_speed = lanes_avg_speed[1];
        		        	cout << "slow down" << endl;
						}
					}					
              		else if ( coll_ahead_cost == 1.0 && coll_left_cost == 1.0 && coll_right_cost == 0.0)
              		{
              			//check for right lane
              			double right_buffer = buffer_cost({distance_data[4],distance_data[5]});
              			cout <<  "right up buffer : " << right_buffer << endl;

              			if ( right_buffer == 0)
              			{
							lane = 2; // change lane to right
                			cout << "change lane to right" << endl;
              			}
              			else
              			{
		                	too_close = true;
		                	avg_speed = lanes_avg_speed[1];
        		        	cout << "slow down" << endl;
              			}
              		}
              		else if (coll_ahead_cost == 1 && coll_left_cost == 0.0 && coll_right_cost == 0.0) {
              			// give priority to change to left lane
              			lane = 0;
              			cout << "change lane to left" << endl;
              		}

				}              
            }
            else if ( lane == 0 ) // ego car in left lane, check if can goto center lane
            {
              double coll_cost = collision_cost({distance_data[2]});
              cout << "collision cost : " << coll_cost << endl;

              if ( coll_cost == 1)
              {
              	// check right side
                double coll_center_cost = collision_cost({distance_data[4],distance_data[5]});
                if ( coll_center_cost == 1) // slow down in the current lane
                {
                  too_close = true;
                  avg_speed = lanes_avg_speed[0];
                  cout << "slow down" << endl;
                }
                else {
                  lane = 1;
                  cout << "change lane to center" << endl;
                }
              }
            }
            else if ( lane == 2) // ego car in left lane, check if can goto center lane
            {
              double coll_cost = collision_cost({distance_data[2]});
              cout << "collision cost : " << coll_cost << endl;

              if ( coll_cost == 1)
              {
              	// check left side
                double coll_center_cost = collision_cost({distance_data[0],distance_data[1]});
                if ( coll_center_cost == 1) // slow down in the current lane
                {
                  too_close = true;
                  avg_speed = lanes_avg_speed[2];
                  cout << "slow down" << endl;
                }
                else {
                  lane = 1;
                  cout << "change lane to center" << endl;
                }
              }

            }


            /*double closest = 999999;
            for (int i = 0; i < sensor_fusion.size(); ++i)
            {
              auto vehicle = sensor_fusion[i];
              cout << "vehicle:" << vehicle << endl;
              double vehicle_id = vehicle[0];
              double vehicle_x = vehicle[1];
              double vehicle_y = vehicle[2];
              double vehicle_vx = vehicle[3];
              double vehicle_vy = vehicle[4];
              double vehicle_s = vehicle[5];
              double vehicle_d = vehicle[6];

              // get LaneId
              int vehicle_lane = getLane(vehicle_d);

              // velocity magintude
              double check_speed = sqrt(vehicle_vx*vehicle_vx + vehicle_vy*vehicle_vy);
              vehicle_s += ((double)prev_size*.02*check_speed);

              //check for vehicle if its in our current lane
              // checking in Fernet coordinates
              //if ( vehicle_d < (2+4*lane+2) && vehicle_d > (2+4*lane-2))
              if ( lane == vehicle_lane)
              {
                // check for the gap with the car ahead. If distance is 
                // less than 30 meter, reduce the speed
                if ((vehicle_s > car_s) && ((vehicle_s - car_s)<30))
                {
                  //ref_vel = 29.5;  
                  too_close = true;
                  if ( lane > 0 )
                  {
                    lane = 0;
                  }
                }
              }

            } */

            if ( too_close )
            {
            	if ( ref_vel > avg_speed )
              		ref_vel -= .224 * cpu_cycles;
            }
            else if ( ref_vel < 49.5 ){

              if ( ref_vel > 0 and ref_vel < 4)
              {
                // to avoid max jerk warning when the simulator starts
                ref_vel += .224 ;
              }
              else
              {
                ref_vel += .224 * cpu_cycles;
              }

              // avoid max speed exceeded
              if ( ref_vel > MAX_VELOCITY )
              {
                ref_vel = MAX_VELOCITY;
              }

            }


            vector<double> ptsx;
            vector<double> ptsy;

            double ref_x = car_x;
            double ref_y = car_y;
            double ref_yaw = deg2rad(car_yaw);
            

            if ( prev_size < 2)
            {
              double prev_car_x = car_x - cos(car_yaw);
              double prev_car_y = car_y - sin(car_yaw);

              ptsx.push_back(prev_car_x);
              ptsx.push_back(car_x);

              ptsy.push_back(prev_car_y);
              ptsy.push_back(car_y);

            }
            else
            {
              ref_x = previous_path_x[prev_size-1];
              ref_y = previous_path_y[prev_size-1];

              double prev_ref_x = previous_path_x[prev_size-2];
              double prev_ref_y = previous_path_y[prev_size-2];
              ref_yaw = atan2(ref_y-prev_ref_y,ref_x-prev_ref_x);

              ptsx.push_back(prev_ref_x);
              ptsx.push_back(ref_x);

              ptsy.push_back(prev_ref_y);
              ptsy.push_back(ref_y);

            }

            vector<double> next_wp0 = getXY(car_s + 30, double(2+4*lane), map_waypoints_s, map_waypoints_x, map_waypoints_y);
            vector<double> next_wp1 = getXY(car_s + 60, double(2+4*lane), map_waypoints_s, map_waypoints_x, map_waypoints_y);
            vector<double> next_wp2 = getXY(car_s + 90, double(2+4*lane), map_waypoints_s, map_waypoints_x, map_waypoints_y);

            ptsx.push_back(next_wp0[0]);
            ptsx.push_back(next_wp1[0]);
            ptsx.push_back(next_wp2[0]);

            ptsy.push_back(next_wp0[1]);
            ptsy.push_back(next_wp1[1]);
            ptsy.push_back(next_wp2[1]);

            for ( int i=0; i<ptsx.size(); i++)
            {
              double shift_x = ptsx[i] - ref_x;
              double shift_y = ptsy[i] - ref_y;

              ptsx[i] = (shift_x * cos(0-ref_yaw) - shift_y * sin(0-ref_yaw));
              ptsy[i] = (shift_x * sin(0-ref_yaw) + shift_y * cos(0-ref_yaw));

            }

            // create a spline
            tk::spline s;

            // set x y points to the spline

            for (auto x : ptsx)
              std::cout << "x value is " << setw(4) << x << endl;
              std::cout << std::endl;

            for (auto x : ptsy)
              std::cout << "y value is " << setw(4) << x << endl;
              std::cout << std::endl;

            // set (X,Y) points to the spline
            s.set_points(ptsx, ptsy);

            for (int i=0; i<previous_path_x.size(); i++)
            {
              next_x_vals.push_back(previous_path_x[i]);
              next_y_vals.push_back(previous_path_y[i]); 
            }

            int closest_waypoint_index = NextWaypoint(car_x, car_y, car_yaw, map_waypoints_x, map_waypoints_y);
            cout << "closest_waypoint_index : " << closest_waypoint_index << endl;

            double next_waypoint_x = map_waypoints_x[closest_waypoint_index];
            double next_waypoint_y = map_waypoints_y[closest_waypoint_index];
            double next_waypoint_s = map_waypoints_s[closest_waypoint_index];
            double next_waypoint_dx = map_waypoints_dx[closest_waypoint_index];
            double next_waypoint_dy = map_waypoints_dy[closest_waypoint_index];

            cout << "next_waypoint_dx : " << next_waypoint_dx << endl;


            double target_x = 30;
            //double target_x = next_waypoint_x;
            double target_y = s(target_x);

            double target_dist = sqrt((target_x*target_x) + (target_y*target_y));

            double x_add_on = 0;
            

            double dist_inc = 0.4;
            for(int i = 1; i <= 50-previous_path_x.size(); i++)
            {    

            //  double next_s = car_s + (i+1) * dist_inc;
            //  double next_d = car_d;
            //  vector<double> xy = getXY(next_s, next_d, map_waypoints_s, map_waypoints_x, map_waypoints_y);
            //  next_x_vals.push_back(xy[0]);
            //  next_y_vals.push_back(xy[1]);              

              //next_x_vals.push_back(pos_x+(dist_inc)*cos(angle+(i+1)*(pi()/100)));
              //next_y_vals.push_back(pos_y+(dist_inc)*sin(angle+(i+1)*(pi()/100)));
              //pos_x += (dist_inc)*cos(angle+(i+1)*(pi()/100));
              //pos_y += (dist_inc)*sin(angle+(i+1)*(pi()/100));

              double N = (target_dist/(0.02*ref_vel/2.24));
              double x_point = x_add_on + (target_x/N);
              double y_point = s(x_point);

              x_add_on = x_point;

              double x_ref = x_point;
              double y_ref = y_point;

              //rotote it back to normal
              x_point = x_ref*cos(ref_yaw) - y_ref*sin(ref_yaw);
              y_point = x_ref*sin(ref_yaw) + y_ref*cos(ref_yaw);

              x_point += ref_x;
              y_point += ref_y;

              next_x_vals.push_back(x_point);
              next_y_vals.push_back(y_point);


            }

          	msgJson["next_x"] = next_x_vals;
          	msgJson["next_y"] = next_y_vals;

          	auto msg = "42[\"control\","+ msgJson.dump()+"]";

          	//this_thread::sleep_for(chrono::milliseconds(1000));
          	ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
          
        }
      } else {
        // Manual driving
        std::string msg = "42[\"manual\",{}]";
        ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
      }
    }
  });

  // We don't need this since we're not using HTTP but if it's removed the
  // program
  // doesn't compile :-(
  h.onHttpRequest([](uWS::HttpResponse *res, uWS::HttpRequest req, char *data,
                     size_t, size_t) {
    const std::string s = "<h1>Hello world!</h1>";
    if (req.getUrl().valueLength == 1) {
      res->end(s.data(), s.length());
    } else {
      // i guess this should be done more gracefully?
      res->end(nullptr, 0);
    }
  });

  h.onConnection([&h](uWS::WebSocket<uWS::SERVER> ws, uWS::HttpRequest req) {
    std::cout << "Connected!!!" << std::endl;
    reset_terminal = true;
  });

  h.onDisconnection([&h](uWS::WebSocket<uWS::SERVER> ws, int code,
                         char *message, size_t length) {
    ws.close();
    std::cout << "Disconnected" << std::endl;
  });

  int port = 4567;
  if (h.listen(port)) {
    std::cout << "Listening to port " << port << std::endl;
  } else {
    std::cerr << "Failed to listen to port" << std::endl;
    return -1;
  }
  h.run();
}
















































































