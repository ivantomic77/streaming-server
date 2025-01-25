# Simple Video Streaming Service

This service streams video chunks to an HTML video player using the HTTP 206 Partial Content response.

It's a simple project I built to experiment with the C programming language and create a basic TCP server. This is not intended for production use, as it requires significant improvements. It’s primarily a proof of concept to enhance my understanding of video streaming and server development.

## How does it work?

The HTML video tag requests video data by using the Range header.

Here’s an example of the HTTP request made by the browser:
```http
GET / HTTP/1.1
Range: 0-
```
This tells the server to send a chunk of the video starting from the 0th byte. By default, if no end byte is specified, the server will send 1MB (1048576 bytes).

Once the browser receives a chunk, it plays that part of the video. When it needs more, it sends a new request for the next chunk.

The server also sends the total size of the video so the browser knows how long the video is.

## How to run?

To run this project, you need to be in a Linux environment (or use WSL with Ubuntu on Windows).

To build the project, use the following command:
```bash
make build
```

To run the server use:
```bash
make run
```

Once the server is running, open the index.html file in your browser and try streaming the video.

## Attribution

This project includes video content from Big Buck Bunny by the Blender Foundation, which is licensed under the Creative Commons Attribution 3.0 Unported License (CC BY 3.0).

Big Buck Bunny is a work by the Blender Foundation, and you can find more information about it at https://www.bigbuckbunny.org/.