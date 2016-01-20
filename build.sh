#!
g++ -std=c++11 $(pkg-config --cflags libconfig++) onehand.cpp -o onehand $(pkg-config --libs libconfig++)
