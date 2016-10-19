## Why?

Over the last year or two, I've been teaching myself to type with one hand. Some people might think it's useless, but it's actually quite useful. Just being able to hit enter by hitting capslock + space has saved me loads of time on entry with one hand on my mouse and one hand on my keyboard.

I wrote a program on Windows about a year ago to shift all the keys on the right side of the keyboard over to the left side when I held caps lock. A month later I got a MacBook, and rewrote the program for OSX. A week ago, I bought a Chromebook to use as my daily driver and installed Ubuntu on it, so naturally, I had to port this to Linux.


# Warning!

This was an afternoon project so I can have one handed typing on my Chromebook. After getting used to it for so long, it's hard to go back. 

If something goes wrong, you might have to ssh in and kill the program or plug in a second keyboard to free the EVIOCGRAB exclusive lock on your keyboard.

## Building

Requirements:
* uinput module
* g++ with c++11 support

The uinput module comes boxed with most versions of the Linux kernel, but you might have to enable it explicitly. You can find instructions online.

Run the following and a binary file "onehand" should pop out.
```
git submodule init
git submodule update
./build.sh
```


## Usage

* Identify the device your keyboard is hooked up to.
```bash
cat /proc/bus/input/devices
```

* Configure onehand to use that device and save the configuration to disk:

*example*
```bash
sudo onehand --device /dev/input/<device event name> --configure ~/.onehand
```

* Follow the onscreen prompts. The first key you press will be your modifier key. When you're done configuring, the modifier key will rebind keys on your keyboard while you hold it.


* Press a key you want to change when the modifier key is held.

* Press the key you want that key to become. You'll repeat these two steps in a loop to rebind your keyboard.


* When you're satisfied with your rebindings, press the modifier key again to stop configuring.


* Run onehand and load your configuration file:

*example*
```bash
sudo onehand --device /dev/input/<device event name> --load ~/.onehand
```
