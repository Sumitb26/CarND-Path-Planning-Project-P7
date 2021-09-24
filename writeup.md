**Path Planning Project**

The goals / steps of this project are the following:

* To design a path planner that is able to create smooth, safe paths for the car to follow along a 3 lane highway with traffic.
* The car is able to drive at least 4.32 miles without incident.
* The car drives according to the speed limit.
* Max Acceleration and Jerk are not Exceeded.
* Car does not have collisions.
* The car stays in its lane, except for the time between changing lanes.
* The car is able to change lanes.


[image1]: ./path-planning.png


### Compilation
Code complies correctly.


### Valid trajectories

#### The car is able to drive at least 4.32 miles without incident.
![alt text][image1]
#### The car drives according to the speed limit.
No speed limit red message was seen.

#### Max Acceleration and Jerk are not Exceeded.
Max jerk red message was not seen.

#### Car does not have collisions.
No collisions.

#### The car stays in its lane, except for the time between changing lanes.
The car stays in its lane most of the time but when it changes lane because of traffic or to return to the center lane.

#### The car is able to change lanes.
The car change lanes when the there is a slow car in front of it, and it is safe to change lanes (no other cars around) or when it is safe to return the center lane.


### Reflection
Planning algorithm starts at line 104 till line 268 in src/main.cpp file. The code consists of three parts:

#### Prediction (line 108 - 146)
This part of the code deal with the telemetry and sensor fusion data. It intents to reason about the environment. In this case, we want to know three aspects of it:

* Is there a car in front of us blocking the traffic.
* Is there a car to the right of us making a lane change not safe.
* Is there a car to the left of us making a lane change not safe.

 A car is considered "dangerous" when its distance to our car is less than 30 meters in front or behind us.

#### Behaviour Planning (line 148 - 168)
This part decides what to do:

* If we have a car in front of us, do we change lanes?
* Do we speed up or slow down?

Based on the prediction of the situation we are in, this code increases the speed, decrease speed, or make a lane change when it is safe.
This approach makes the car more responsive acting faster to changing situations like a car in front of it trying to apply breaks to cause a collision.
#### Trajectory Generation (line 172 - 268)
This code does the calculation of the trajectory based on the speed and lane output from the behavior, car coordinates and past path points.
First, the last two points of the previous trajectory are used in conjunction three points at a far distance to initialize the spline calculation. To make the work less complicated to the spline calculation based on those points, the coordinates are transformed to local car coordinates.

