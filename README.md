# System Programming Lab 11 Multiprocessing
## Multiprocessing Implementation
The multiprocessing was implemeted using a shared memory object to contain a struct with all of the image parameters, updating the scale for each iteration/frame. The program creates n processes to split up the processing cost of multiple frames and keeps track of active procs using a seperate varibale in the struct.
## Results
![graph of n procs vs time](procs_vs_time.png)
### Discussion
The program was tested using the run command `time ./mandel_movie -x -0.7849927743249281 -y 0.14659168099751696 -s 0.0009044129401445389 -m 3300 -n <num procs>`, and the video file was created using `ffmpeg -i mandel%d.jpg mandel.mpg`. The program is very slow without any child processes, but it speeds up dramatically as more are used. The speed begins to platau around 10-15 processes, which makes sense because the computer used has a total of 12 logical processors.