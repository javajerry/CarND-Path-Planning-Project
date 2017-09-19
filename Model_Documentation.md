Though the project is interesting and challenging, I started with the starter code to see how car moves. It was
after watching the Walkthrough and Q&A video, it helped me to understand better and code the project.
<br>Below are the steps for plotting the trajectory which included avoiding collisions, lane changing, staying within speed limit of 50mph, avoiding max jerk. I pretty much followed the code in the Walkthrough video.
<br>
<h2> Determine the lane position</h2>
<br>
Simulator is returning the data related to our ego car as well as sensor fusion data. Sensor fusion data consists of other cars in the traffic but on the right side. 
<br>
Code starts with determining the lane number of ego car using car_d, which is distance from the center lane (lateral displacement). Since lane width is 4 meters. So if the car is between 0 and 4, then car is in the left lane and given laneId =0. If a car is between 4 and 8meters, then car is in the center lane and given laneId = 1. If car is between 8 and 12 then car is in the right lane and it's given laneId = 2.
<br>
<h2> Analyze the sensor fusion data and categorize</h2>
The sensor_fusion variable contains all the information about the cars on the right-hand side of the road. For each car it provides id, global map coordinates, velocity component and Fernet coordinates. For analyzing the sensor fusion data, I relied on Fernet coordinates. Since its 3 lanes, I divided it into 6 categories 
<li> Left Ahead </li>
<li> Left Behind </li>

