| **ProSLAM** |
| :------: |
| Authors: Dominik Schlegel, Mirco Colosi, Giorgio Grisetti |

### Demo videos ###
![ProSLAM: Full run KITTI Sequence 00 (real-time, 1 thread@2.40GHz/i7-4700MQ)](https://www.youtube.com/watch?v=n_UmEpIwb9Y)

---
### Supported environments ###
Currently Linux only:
 - Ubuntu 14.04 LTS + ROS Indigo /(OpenCV2 + Qt4)
 - Ubuntu 16.04 LTS + ROS Kinetic/(OpenCV3 + Qt5)<br/>

The complete system **runs on a single thread** (including visualization components)

---
### How do I get set up? ###
1) install the Ubuntu packages

    sudo apt-get install build-essential libeigen3-dev libsuitesparse-dev freeglut3-dev libqglviewer-dev

---
2) download and install
 - ROS: http://wiki.ros.org/ROS/Installation

or (OpenCV + Qt)
 - OpenCV3: https://github.com/opencv/opencv/archive/3.2.0.zip (Version 3.2.0)
 - Qt5: https://wiki.qt.io/Install_Qt_5_on_Ubuntu (Version 5.7.0)

---
3) download and install ROS catkin: http://wiki.ros.org/catkin

---
4) download and install
- g2o for catkin: https://github.com/yorsh87/g2o_catkin

or
- the native g2o library: https://github.com/RainerKuemmerle/g2o (G2O_ROOT environment variable must be set)


---
5) download and build (using catkin_make) this repository:

    git clone https://gitlab.com/srrg-software/srrg_proslam.git

The catkin build will automatically fetch the SRRG libraries (if not existing):
 - srrg_boss: https://gitlab.com/srrg-software/srrg_boss (required by srrg_core)
 - srrg_core: https://gitlab.com/srrg-software/srrg_core
 - srrg_gl_helpers: https://gitlab.com/srrg-software/srrg_gl_helpers (required by srrg_core_viewers)
 - srrg_core_viewers: https://gitlab.com/srrg-software/srrg_core_viewers
 - srrg_hbst: https://gitlab.com/srrg-software/srrg_hbst

CMake variables that must be set when building without ROS or to select specific libraries:

    OpenCV_DIR=/your/path/to/the/opencv/build/folder

---
### How do I check if it works? ###

1) download the KITTI sequence 00 into a folder on your computer: https://drive.google.com/open?id=0ByaBRAPfmgEqdXhJRmktQ2lsMEE (2.8GB)

---
2) launch a terminal in that folder and uncompress the tarball:

    tar -xzvf 00.tar.gz

The folder should now contain 4 files (.txt) and 1 folder (.txt.d) plus the tarball 00.tar.gz

---
3) run the system directly in the folder:

    rosrun srrg_proslam srrg_proslam_app 00.txt -use-gui

Two windows will pop up (Input and Output)

---
4) press [Backspace] on the Input window to toggle between automatic processing and stepwise (press [Space] for stepping) mode

---
### It doesn't work? ###

Feel free to contact the author: schdomin@gmail.com