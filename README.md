## Websocket Client for Arduino

This is a simple library that implements a Websocket client running on an Arduino. The server code has been removed and all String has been replaced with char* to avoid heap fragmentation.

*If I have more time I will also re-implement the code for Websocket Server.*

### Getting started

Install the library to "libraries" folder in your Arduino sketchbook folder. For example, on a mac that's `~/Documents/Arduino/libraries`.

Try the examples to ensure that things work.

Start playing with your own code!

### Notes
Because of limitations of the current Arduino platform (Uno at the time of this writing), this library does not support messages larger than 65535 characters. In addition, this library only supports single-frame text frames. It currently does not recognize continuation frames, binary frames, or ping/pong frames.

### Credits

This repository is forked from [brandenhall/Arduino-Websocket](https://github.com/brandenhall/Arduino-Websocket) to add small improvements.