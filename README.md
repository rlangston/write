# Write

A lightweight terminal editor based on ncruses.

## Background

This was inspired by the kilo editor (http://antirez.com/news/108) and a tutorial which was in turn based on this (https://viewsourcecode.org/snaptoken/kilo/index.html)

## Usage

./write [filename]   

If no filename is specified

## Keyboard shortcuts

### Navigation
CTRL-End
Move to file end

CTRL-Home
Move to file home

CTRL-Left
Move word left

CTRL-Right
Move word right

### Other shortcuts
CTRL-q
Quit the program prompting to save any unsaved buffers.

CTRL-o
Open a file.

CTRL-n
Open a new buffer (with the default name new.txt)

CTRL-s
Save the current buffer, defaulting to the current filename.  Hit Escape to abort the file save.

CTRL-Page Down
Move to next open buffer

F4
Close current buffer
