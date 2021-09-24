#include <uWS/uWS.h>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include "Eigen-3.3/Eigen/Core"
#include "Eigen-3.3/Eigen/QR"
#include "helpers.h"
#include "json.hpp"
#include "spline.h"

// for convenience
using nlohmann::json;
using std::string;
using std::vector;

int main() {
  uWS::Hub h;

  // Load up map values for waypoint's x,y,s and d normalized normal vectors
  vector<double> map_waypoints_x;
  vector<double> map_waypoints_y;
  vector<double> map_waypoints_s;
  vector<double> map_waypoints_dx;
  vector<double> map_waypoints_dy;

  // Waypoint map to read from
  string map_file_ = "../data/highway_map.csv";
  // The max s value before wrapping around the track back to 0
  double max_s = 6945.554;

  std::ifstream in_map_(map_file_.c_str(), std::ifstream::in);

  string line;
  while (getline(in_map_, line)) {
    std::istringstream iss(line);
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
  
  int lane = 1;
  double ref_vel = 0;

  h.onMessage([&ref_vel, &map_waypoints_x,&map_waypoints_y,&map_waypoints_s,
               &map_waypoints_dx,&map_waypoints_dy, &lane]
              (uWS::WebSocket<uWS::SERVER> ws, char *data, size_t length,
               uWS::OpCode opCode) {
    // "42" at the start of the message means there's a websocket message event.
    // The 4 signifies a websocket message
    // The 2 signifies a websocket event
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

          // Sensor Fusion Data, a list of all other cars on the same side 
          //   of the road.
          auto sensor_fusion = j[1]["sensor_fusion"];

          json msgJson;

          vector<double> next_x_vals;
          vector<double> next_y_vals;

          /**
           * TODO: define a path made up of (x,y) points that the car will visit
           *   sequentially every .02 seconds
           */
          int prev_size = previous_path_x.size();
          if(prev_size > 0) {
            car_s = end_path_s;
          }
          // these variables are used to guide the ego car to change lane and keep a desired velocity.
          bool too_close = false;
          bool car_left = false;
          bool car_right = false;

          for(int i = 0; i < sensor_fusion.size(); i++) {
            float d = sensor_fusion[i][6];
            int car_lane = -1;
            //check if is the same lane with ego car
            if(d > 0 && d < 4) {
              car_lane = 0;
            } else if(d > 4 && d < 8) {
              car_lane = 1;
            } else if(d > 8 && d < 12) {
              car_lane = 2;
            }
            if(car_lane < 0) {
              continue;
            }
  
            // Find the car speed
            double vx = sensor_fusion[i][3];
            double vy = sensor_fusion[i][4];
            double check_speed = sqrt(vx*vx + vy*vy);
            double check_car_s = sensor_fusion[i][5];
            // Estimate car s when using previous points.
            check_car_s +=((double)check_speed * 0.02 * prev_size);
            if ( car_lane == lane ) {
              // car in our lane.
              too_close |= check_car_s > car_s && check_car_s - car_s < 30;
            } else if ( car_lane - lane == -1 ) {
              // check car left
              car_left |= car_s - 30 < check_car_s && car_s + 30 > check_car_s;
            } else if ( car_lane - lane == 1 ) {
              //check car right
              car_right |= car_s - 30 < check_car_s && car_s + 30 > check_car_s;
            }         
           }
          
          // lane change actions
          double speed_diff = 0;
          if(too_close) {
            if(!car_left && lane > 0) {
              //There is no car on the left lane and ego car not in lane 0, ego car change left lane.
              lane--;
            } else if(!car_right && lane < 2) {
              //There is no car on the right lane and ego car not in lane 2, ego car change right lane.
              lane++;
            } else {
              ref_vel -= 0.224;  // 5m/s acceleration
            }
          } else {
            if(lane != 1) {
              if((lane == 0 && !car_right) || (lane == 2 && !car_left)) {
                lane = 1; //back to the center lane
              }
            }
            if(ref_vel < 49.5) {
              ref_vel += 0.224;
            }
          }            
          // generate the best trajectory.    
          // create a list widely spaced (x, y) waypoints, evenly spaced at 30m
          // later we will interoplate these waypoints with s spline.
          vector<double> ptsx;
          vector<double> ptsy;

          double ref_x = car_x;
          double ref_y = car_y;
          double ref_raw = deg2rad(car_yaw);

          // Reference the starting point as where the car is or at the previous paths end points.
          // If previous size is almost empty, use the car as a starting reference
          if(prev_size < 2) {
            double prev_car_x = car_x - cos(car_yaw);
            double prev_car_y = car_y - sin(car_yaw);

            ptsx.push_back(prev_car_x);
            ptsx.push_back(car_x);

            ptsy.push_back(prev_car_y);
            ptsy.push_back(car_y);
          } else {
            // use the last two points.
            ref_x = previous_path_x[prev_size - 1];
            ref_y = previous_path_y[prev_size - 1];

            double ref_x_prev =  previous_path_x[prev_size - 2];
            double ref_y_prev =  previous_path_y[prev_size - 2];
            ref_raw = atan2(ref_y - ref_y_prev, ref_x - ref_x_prev);

            // use two points that make the path tangent to the previous path's end points.
            ptsx.push_back(ref_x_prev);
            ptsx.push_back(ref_x);

            ptsy.push_back(ref_y_prev);
            ptsy.push_back(ref_y);
          }
          // In Frenet add evenly 30m points ahead of the staring reference
          vector<double> next_wp0 = getXY(car_s + 30, (2+4*lane), map_waypoints_s, map_waypoints_x, map_waypoints_y);
          vector<double> next_wp1 = getXY(car_s + 60, (2+4*lane), map_waypoints_s, map_waypoints_x, map_waypoints_y);
          vector<double> next_wp2 = getXY(car_s + 90, (2+4*lane), map_waypoints_s, map_waypoints_x, map_waypoints_y);
          ptsx.push_back(next_wp0[0]);
          ptsx.push_back(next_wp1[0]);
          ptsx.push_back(next_wp2[0]);

          ptsy.push_back(next_wp0[1]);
          ptsy.push_back(next_wp1[1]);
          ptsy.push_back(next_wp2[1]);

          // Shift the points to local car coordinates make sure that the car or that
          // last point of previous path at zero, the origin and its angle at zero degree.
          for(int i = 0; i < ptsx.size(); ++i) {
            //shift car reference ange to 0 degree
            double shift_x = ptsx[i] - ref_x;
            double shift_y = ptsy[i] - ref_y;
            ptsx[i] = shift_x * cos(0 - ref_raw) - shift_y * sin(0 - ref_raw);
            ptsy[i] = shift_x * sin(0 - ref_raw) + shift_y * cos(0 - ref_raw);
          }

          // create a spline
          tk::spline s;
          // set (x, y) points to the spline
          s.set_points(ptsx, ptsy);

          // start with all of the previous path point from last time
          // (previous path size will be less than 50)
          for(int i = 0; i < prev_size; i++) {
            next_x_vals.push_back(previous_path_x[i]);
            next_y_vals.push_back(previous_path_y[i]);
          }

          // Calculate how to break up the spline points so that the car will travel at
          // a desired velocity(50 MPH constant)
          double target_x = 30.0;
          double target_y = s(target_x);
          double target_dist = sqrt(target_x * target_x + target_y * target_y);

          double x_add_on = 0;

          // Fill up the rest of our planer after filling with previous.outputs will be 50 points.
          for(int i = 0; i <= 50 - prev_size; ++i) {
            double N = target_dist / (0.02 * ref_vel/2.24);
            double x_point = x_add_on + target_x / N;
            double y_point = s(x_point);

            x_add_on = x_point;

            double x_ref = x_point;
            double y_ref = y_point;

            // Rotating back to normal after rotating it earlier.
            x_point = x_ref * cos(ref_raw) - y_ref * sin(ref_raw);
            y_point = x_ref * sin(ref_raw) + y_ref * cos(ref_raw);

            x_point += ref_x;
            y_point += ref_y;

            next_x_vals.push_back(x_point);
            next_y_vals.push_back(y_point);
          }

          msgJson["next_x"] = next_x_vals;
          msgJson["next_y"] = next_y_vals;

          auto msg = "42[\"control\","+ msgJson.dump()+"]";

          ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
        }  // end "telemetry" if
      } else {
        // Manual driving
        std::string msg = "42[\"manual\",{}]";
        ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
      }
    }  // end websocket if
  }); // end h.onMessage

  h.onConnection([&h](uWS::WebSocket<uWS::SERVER> ws, uWS::HttpRequest req) {
    std::cout << "Connected!!!" << std::endl;
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