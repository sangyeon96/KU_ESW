# Web streaming example
# Source code from the official PiCamera package
# http://picamera.readthedocs.io/en/latest/recipes2.html#web-streaming

import io
import picamera
import logging
import socketserver
from threading import Condition
from http import server
from os import curdir
from os.path import join as pjoin

PAGE="""\
<html>
<head>
	<title>Raspberry Pi - I2I Camera</title>
	<style type="text/css">
		.img{
			position: relative;
			background-image: url(/stream.mjpg);
			height: 100vh;
			background-size: cover;
		}
		.img-cover{
			position: absolute;
			height: 100%;
			width: 100%;
			background-color: rgba(0, 0, 0, 0.4);
			z-index: 1;
		}
		.img .content{
			position: absolute;
			top: 7%;
			left: 70%;
			transform: translate(+35%, -50%);
			font-size: 1.5rem;
			color: white;
			z-index: 2;
			text-align: right;
		}
		h1{
			margin-block-start: 0px;
			margin-block-end: 0px;
		}
	</style>
</head>
<body>
<center>
	<h1>Raspberry Pi - Surveillance Camera</h1>
</center>
<center>
	<div class=img>
		<div class="content">
			<h1>hum : <span id="hum"></span></h1>
			<h1>dust : <span id="dust"></span></h1>
			<script type="text/javascript">

				function readTextDust(file){
					var rawFile = new XMLHttpRequest();
					rawFile.open("GET", file, false);
					rawFile.onreadystatechange = function(){
							if(rawFile.status === 200 || rawFile.status == 0){
								var allText = rawFile.responseText;
								document.getElementById('dust').innerHTML = allText;
							}
					}
					rawFile.send(null);
				}

				function readTextHum(file){
					var rawFile2 = new XMLHttpRequest();
					rawFile2.open("GET", file, false);
					rawFile2.onreadystatechange = function(){
							if(rawFile2.status === 200 || rawFile2.status == 0){
								var allText2 = rawFile2.responseText;
								document.getElementById('hum').innerHTML = allText2;
							}
					}
					rawFile2.send(null);
				}

				readTextDust("/dust.txt");
				readTextHum("/hum.txt");
				
				function refreshData(){
					readTextDust("/dust.txt");
					readTextHum("/hum.txt");
				}

				setInterval("refreshData()", 3000);
			</script>
		</div>	
	</div>
</center>
</body>
</html>
"""

class StreamingOutput(object):
    def __init__(self):
        self.frame = None
        self.buffer = io.BytesIO()
        self.condition = Condition()

    def write(self, buf):
        if buf.startswith(b'\xff\xd8'):
            # New frame, copy the existing buffer's content and notify all
            # clients it's available
            self.buffer.truncate()
            with self.condition:
                self.frame = self.buffer.getvalue()
                self.condition.notify_all()
            self.buffer.seek(0)
        return self.buffer.write(buf)

class StreamingHandler(server.BaseHTTPRequestHandler):
    store_path = pjoin(curdir, 'file.txt')
    store_path_hum = pjoin(curdir, 'hum.txt')
    store_path_dust = pjoin(curdir, 'dust.txt')
    def do_GET(self):
        if self.path == '/':
            self.send_response(301)
            self.send_header('Location', '/index.html')
            self.end_headers()
        elif self.path == '/file.txt':
            with open(self.store_path) as fh:
                    self.send_response(200)
                    self.send_header('Content-Type', 'text')
                    self.end_headers()
                    self.wfile.write(fh.read().encode())
        elif self.path == '/dust.txt':
            with open(self.store_path_dust) as fh:
                    self.send_response(200)
                    self.send_header('Content-Type', 'text')
                    self.end_headers()
                    self.wfile.write(fh.read().encode())
        elif self.path == '/hum.txt':
            with open(self.store_path_hum) as fh:
                    self.send_response(200)
                    self.send_header('Content-Type', 'text')
                    self.end_headers()
                    self.wfile.write(fh.read().encode())
        elif self.path == '/index.html':
            content = PAGE.encode('utf-8')
            self.send_response(200)
            self.send_header('Content-Type', 'text/html')
            self.send_header('Content-Length', len(content))
            self.end_headers()
            self.wfile.write(content)
        elif self.path == '/stream.mjpg':
            self.send_response(200)
            self.send_header('Age', 0)
            self.send_header('Cache-Control', 'no-cache, private')
            self.send_header('Pragma', 'no-cache')
            self.send_header('Content-Type', 'multipart/x-mixed-replace; boundary=FRAME')
            self.end_headers()
            try:
                while True:
                    with output.condition:
                        output.condition.wait()
                        frame = output.frame
                    self.wfile.write(b'--FRAME\r\n')
                    self.send_header('Content-Type', 'image/jpeg')
                    self.send_header('Content-Length', len(frame))
                    self.end_headers()
                    self.wfile.write(frame)
                    self.wfile.write(b'\r\n')
            except Exception as e:
                logging.warning(
                    'Removed streaming client %s: %s',
                    self.client_address, str(e))
        else:
            self.send_error(404)
            self.end_headers()

class StreamingServer(socketserver.ThreadingMixIn, server.HTTPServer):
    allow_reuse_address = True
    daemon_threads = True

with picamera.PiCamera(resolution='640x480', framerate=24) as camera:
    output = StreamingOutput()
    #Uncomment the next line to change your Pi's Camera rotation (in degrees)
    #camera.rotation = 90
    camera.start_recording(output, format='mjpeg')
    try:
        address = ('', 8000)
        server = StreamingServer(address, StreamingHandler)
        server.serve_forever()
    finally:
        camera.stop_recording()

