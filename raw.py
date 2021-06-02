import cv2
from bitstream import BitStream

infile = "out.mkv"
outfile = "frames.bin"

print("Reading from %s"%infile)
vid = cv2.VideoCapture(infile)
fps = vid.get(cv2.CAP_PROP_FPS)
width = int(vid.get(cv2.CAP_PROP_FRAME_WIDTH))
height = int(vid.get(cv2.CAP_PROP_FRAME_HEIGHT))
nbframes = int(vid.get(cv2.CAP_PROP_FRAME_COUNT))
duration = nbframes/fps
print("Video: %i frames (%i:%i) @ %ifps (%ix%i)"%(nbframes,duration/60,duration%60,fps,width,height))
print("Writing to file: %s"%outfile)

def img2bin(image):
	gray = cv2.cvtColor(f, cv2.COLOR_BGR2GRAY)
	_, bwi = cv2.threshold(gray, 127, 255, cv2.THRESH_BINARY)
	return bwi

gbits = BitStream()

cframe = 0

while vid.isOpened():
	r,f = vid.read()
	if not r:
		break

	f = cv2.bitwise_not(img2bin(f).flatten())
	cframe += 1
	if cframe%10 == 0:
		print("Frame: %i/%i (%.2f%%)"%(cframe,nbframes,(cframe/nbframes)*100),end='\r')
	for i in f:
		gbits.write(i == 255)


print("")
print("%i frames"%(cframe))
print("Done")

nbbits = len(gbits)
if nbbits%8 != 0:
	for i in range(8-(nbbits%8)):
		gbits.write(False)

with open(outfile,"wb") as ofile:
	ofile.write(gbits.read(bytes))
