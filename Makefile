.PHONY: run, runallfiles, all, clean

CC=g++

CFLAGS=-c -Wall -std=c++11

run: all
	gnome-terminal -e "bash -c './server -lag; exec bash'" --title="Server"
	gnome-terminal -e "bash -c './client "in/text1.txt" "in/text2.txt" ; echo Press enter or ctrl + c to close terminal ; read line'" --title="Client"

runallfiles: all
	gnome-terminal -e "bash -c './server ; exec bash'" --title="Server"
	gnome-terminal -e "bash -c './client "in/text1.txt" "in/text2.txt" "in/picture.jpg" "in/video.mp4" "in/file" "in/gif.gif" "in/music.mp3" ; echo Press enter or ctrl + c to close terminal ; read line'" --title="Client"

all: server client

server: server.o allfunc.o
	$(CC) -o server server.o allfunc.o

client: client.o allfunc.o
	$(CC) -o client client.o allfunc.o

server.o: server.cpp
	$(CC) $(CFLAGS) server.cpp

client.o: client.cpp
	$(CC) $(CFLAGS) client.cpp

allfunc.o: allfunc.cpp
	$(CC) $(CFLAGS) allfunc.cpp

clean:
	rm -rf *.o server client