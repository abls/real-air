demo opengl program whose camera is controlled using a reverse engineered understanding of the nreal air's imu packet format

completely standalone from nreal's unity sdk, so this supports whateva platform u wanna port it to, not just locked into unity games on android

still need to do sensor fusion, rn I am just using angular velocity data but not accelerometer data. also need to clean the angular velocity data better. after that processing is all cleaned up, I will try to turn this into a reuseable library with a nice api and demote the opengl stuff to an example usage of the library.

and then if I actually get around to doing all of that lol, I would then use this brand new library to make steamvr and/or monado drivers so that we can use openxr applications

currently only writing this with linux in mind but an eventual goal is making sure this works cross-platform

---

https://user-images.githubusercontent.com/112491550/220664611-1abeb873-bc96-4269-9b96-61c9a3b2fce9.mp4

---

```
git submodule update --init
make
./demo
```
