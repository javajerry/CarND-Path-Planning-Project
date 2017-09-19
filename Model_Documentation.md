Though the project is interesting and challenging, I started with the starter code to see how car moves. It was
after watching the Walkthrough and Q&A video, it helped me to understand better and code the project.
<br>
<a href="https://www.youtube.com/watch?v=PgHvrfcToYA">Complete Project Demonstration Video</a>
<br>
<a href="https://youtu.be/PgHvrfcToYA?t=1m14s">Lane Change Demo 1 (at 1m14s)</a>
<br>
<a href="https://youtu.be/PgHvrfcToYA?t=3m30s">Lane Change Demo 2 (at 3m30s)</a>

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
<li> Center Ahead </li>
<li> Center Behind </li>
<li> Right Ahead </li>
<li> Right Behind </li>
The code will go thru the sensor fusion data list and tried to find the minimum distance between ego car and car in the traffic for each category. These categories will help further in determining possible two main actions - collision ahead, perform lane change OR collision ahead, slow down the car. 
<br>
After the sensor data is categorized, then the decision is made based on the lane position of the car. For example, if car is the left or right lane, it has to perform lane change, then the car has to move to the center lane. If the car is in the center lane and code has to figure which lane out of left and right lanes, is the best to move to. For the decision making, I used the cost functions which are similar to the python code for Polynomial Trajectory Generation. I used Cost function for collision, lane buffer and logistic ( which returns the value between 0 and 1).
<h2>Decision Making</h2>
1> If ego car is the center lane, check for the collision cost. If car ahead is less than 30 meters then cost is 1 else 0. Since ego car is in center lane, it can go either in left or right lane. Before it can perform the lane change, it will check for collision cost between left ahead, left behind and right ahead , right behind. Cost will pick the minimum of the ahead and behind value and check if its less than 30 meters or not. It will also check for the lane buffer, make sure it has 50 meters of the buffer to perfrom the safe lane change and avoid the collison with the car ahead or behind. It uses logistic function to determine whether goto left or right lane. Bigger the lane buffer, lower will be the cost and safe to change the lane. If collision is detected in both left and right lane, then its not safe for ego car to change the change in either direction. In this case, the code will slow down by reducing the velocity.
<br>
2> if the ego car is in left lane, it will check for the collision cost with the car ahead in the same lane. if collision cost is returned 1, it will check collision cost with cars in the center ahead and behind lane. if cost returned is 0, the car will move to the center lane. If collision is detected in the center lane, then its not safe for ego car to change the change. In this case, the code will slow down by reducing the velocity.
<br>
3> if the ego car is in right lane, it will check for the collision cost with the car ahead in the same lane. if collision cost is returned 1, it will check collision cost with cars in the center ahead and behind lane. if cost returned is 0, the car will move to the center lane.If collision is detected in the center lane, then its not safe for ego car to change the change. In this case, the code will slow down by reducing the velocity.
<br>
<br>
<h2>Trajectory Generation</h2>
Pretty much followed the code from Walkthru video. We will collect 5 points (X,Y) and use spline algorithm. Spline algorithm is a fine alternative to Jerk Minimizing trajectory. First two points comes from the previous path returned by the simulator. Last two points from the previous path are considered. Next step to get global map corodinates 30 meters, 60 meters and 90 meters ahead of the ego car. These 5 (X,) points are fed to the spline function which determines the smooth curve path between the ego car position and the target distance. For target distance, next_waypoint method is utilized. X coordinate is fed to spline function to get Y cooridnate. Now we know the distance between ego position and target position by applying the linear path. For smoothing purpose ( which in turn helps in avoiding the jerks during the lane change), distance is  divided by the current velcity of the ego car to give the N steps. For each N step, new X,Y coordinate is calulated and added to the previous path. This new path is fed to the simulator which is displayed as green color and guide the car to move. 
<br>
<h2>Improvements for the future</h2>
My Mac is slow and I observed slow response-reaction between the code and the simulator. I had to introduce the cpu_cycles to multiply with 2.24 for the faster response acceleration and deacceleration. It did cause Max Jerk Exceed when the simulator started. So I had to introudce reduced velocity increment till car's velocity reaches 4mph.
<br>
Thinking of a scenario which I didnt run into during my testing. If ego car in the left lane and there's a car ahead about 30 meters. My guess code will make ego car change lane to center. Car ahead also decided to change to the center at the same time, I am assuming code can handle that scenario and avoid the collision.  
<br> My car stopped after completing one loop (4.25 miles), I need to figured how to recycle the waypoints.



