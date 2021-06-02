import cv2,math,time,struct,argparse
from PIL import Image
from bitstream import BitStream
import numpy as np

parser = argparse.ArgumentParser(description='Converter')
parser.add_argument("file", help="Input video")
parser.add_argument("output", help="Optional output file", nargs='?', default="data.bin")
parser.add_argument("-d", "--dithering", help="Enable dithering (makes compression trash)", action="store_true")
cmd_args = parser.parse_args()

infile = cmd_args.file
outfile = cmd_args.output

CLEAR = "\033[K"

print("Reading from %s"%infile)
vid = cv2.VideoCapture(infile)
fps = vid.get(cv2.CAP_PROP_FPS)
width = int(vid.get(cv2.CAP_PROP_FRAME_WIDTH))
height = int(vid.get(cv2.CAP_PROP_FRAME_HEIGHT))
nbframes = int(vid.get(cv2.CAP_PROP_FRAME_COUNT))
normal_size = (nbframes*width*height)/8
duration = nbframes/fps
print("Video: %i frames (%i:%i) @ %ffps (%ix%i)"%(nbframes,duration/60,duration%60,fps,width,height))
print("Writing to file: %s"%outfile)
dithering = cmd_args.dithering
print("Dithering is %s"%("on" if dithering else "off"))

def img2bin(image):
	if dithering:
		img = cv2.cvtColor(image, cv2.COLOR_BGR2RGB)
		im_pil = Image.fromarray(img).convert('1')
		bwi = np.asarray(im_pil)
		return np.logical_not(bwi)

	gray = cv2.cvtColor(f, cv2.COLOR_BGR2GRAY)
	_, bwi = cv2.threshold(gray, 127, 255, cv2.THRESH_BINARY)
	return np.logical_not(bwi == 255)

# 3 -> Duplicate frame
# 2 -> Delta Frame
# 1 -> Huffman Frame
# 0 -> END

gbits = BitStream()
tempbs = BitStream()

symboltable = {}

rle_min_same = 16
dc_chunk_size = 4 # needs to divide width&height
dc_chunk_bits = int(math.log((width*height)//(dc_chunk_size*dc_chunk_size),2))
print("DC Chunk size: %i"%(dc_chunk_size))
print("DC Chunk bits: %i"%(dc_chunk_bits))

def addbits(bs,by,nb):
	for i in range(nb):
		bs.write((by >> (nb-1-i)) & 1 == 1)

def huffcomp(f,bs):
	bs.read()
	tempbs.read()
	addbits(bs,1,2)
	for y in range(0,height,2):
		for x in range(0,width,4):
			for y2 in range(0,2):
				for x2 in range(0,4):
					tempbs.write(f[y+y2,x+x2])
			val = ord(tempbs.read(bytes))
			symb = symboltable[val]
			#print(val,symb)
			bs.write(symb)

def deltacomp(f,lf,xdiff,bs):
	bs.read()
	tempbs.read()
	addbits(bs,2,2)
	wd = width//dc_chunk_size
	chunkdiffs = {}

	xdiff_flat = xdiff.flatten()
	for i in range(len(xdiff_flat)):
		if xdiff_flat[i]:
			x = i%width
			y = i//width
			idx = ((y//dc_chunk_size)*wd)+(x//dc_chunk_size)
			if idx not in chunkdiffs:
				chunkdiffs[idx] = 1
			else:
				chunkdiffs[idx] += 1

	addbits(bs,len(chunkdiffs)-1,dc_chunk_bits)
	for idx in chunkdiffs:
		addbits(bs,idx,dc_chunk_bits)
		srx = (idx%wd)*dc_chunk_size
		sry = (idx//wd)*dc_chunk_size
		for y in range(sry,sry+dc_chunk_size):
			for x in range(srx,srx+dc_chunk_size):
				tempbs.write(f[y,x])
		bys = tempbs.read(bytes)
		for i in bys:
			bs.write(symboltable[i])


"""
	print(len(diff),len(diff)/(dc_chunk_size*dc_chunk_size))
	print(len(chunkdiffs),sum(chunkdiffs.values())/len(chunkdiffs))
	cv2.imshow("Current",cv2.resize(f,(width*4,height*4),interpolation=cv2.INTER_NEAREST))
	cv2.imshow("Old",cv2.resize(lf,(width*4,height*4),interpolation=cv2.INTER_NEAREST))
	fdiffcol = cv2.cvtColor(xdiff,cv2.COLOR_GRAY2BGR)
	for idx in chunkdiffs:
		srx = (idx%wd)*dc_chunk_size
		sry = (idx//wd)*dc_chunk_size
		cv2.rectangle(fdiffcol,(srx,sry),(srx+dc_chunk_size,sry+dc_chunk_size),(0,0,255),1)
	cv2.imshow("Diff",cv2.resize(fdiffcol,(width*4,height*4),interpolation=cv2.INTER_NEAREST))
	cv2.waitKey(0)
	cv2.destroyAllWindows()
	"""

def addframe(bs,f):
	addbits(bs,0,2)
	for y in range(height):
		for x in range(width):
			bs.write(f[y,x])

nbdup = 0
nbhuff = 0
nbdelta = 0
nboverh = 0
nboverd = 0
ratiohuff = 0
ratiodelta = 0

lastframe = np.zeros((height,width), np.uint8)

rlebs = BitStream()
deltabs = BitStream()

cframe = 0
encodedframes = 0

starttime = time.time()

addbits(gbits,int(fps*1000),16)

frequency = {}

print("Generating frequency table")
precalc_frames = []
lastframeflat = np.zeros((height,width), np.uint8)
freqframes = 0
while vid.isOpened():
	r,f = vid.read()
	if not r:
		break
	f = img2bin(f)
	precalc_frames.append(f)
	if not (f ^ lastframeflat).any():
		continue
	print(CLEAR+"Frame: %i/%i (%.2f%%)"%(freqframes,nbframes,(freqframes/nbframes)*100),end='\r')

	for y in range(0,height,2):
		for x in range(0,width,4):
			for y2 in range(0,2):
				for x2 in range(0,4):
					tempbs.write(f[y+y2,x+x2])

	for i in tempbs.read(bytes):
		if i in frequency:
			frequency[i] += 1
		else:
			frequency[i] = 1
	freqframes += 1
	lastframeflat = f

def get_smallest(nodes):
	s = 1
	n = -1
	for i,k in enumerate(nodes):
		if k["p"] != -1:
			continue
		if k["f"] < s:
			n = i
			s = k["f"]
	return n

# Otherwise C decompressor won't have same tree
tnparr = np.array((1,),dtype=np.single)
def addfloat(f1,f2):
	tnparr[0] = 0
	tnparr[0] += f1
	tnparr[0] += f2
	return tnparr[0]

def huffman(freqs):
	nodes = []
	for i in sorted(freqs, key=freqs.get):
		nodes.append({"s":i,"f":freqs[i],"p":-1,"l":-1,"r":-1})
	rootnode = -1
	while True:
		nm = len(nodes)
		n1 = get_smallest(nodes)
		if n1 == -1:
			rootnode = len(nodes)-1
			break
		nodes[n1]["p"] = nm
		n2 = get_smallest(nodes)
		if n2 == -1:
			nodes[n1]["p"] = -1
			nodes[n1]["f"] = 1
			rootnode = n1
			break
		nodes[n2]["p"] = nm
		nodes.append({"s":0,"f":addfloat(nodes[n1]["f"],nodes[n2]["f"]),"p":-1,"l":n1,"r":n2})
	table = {}
	for i in range(len(freqs)):
		vals = []
		nidx = i
		while True:
			nidxn = nodes[nidx]["p"]
			n = nodes[nidxn]
			if n["l"] == nidx:
				vals.insert(0,False)
			elif n["r"] == nidx:
				vals.insert(0,True)
			if n["p"] == -1:
				break
			nidx = nidxn
		table[nodes[i]["s"]] = vals
	return table

addbits(gbits,len(frequency)-1,8)
for i in sorted(frequency, key=frequency.get):
	frequency[i] = frequency[i]/(freqframes*((width*height)/8))
	packed = struct.pack("<f",frequency[i])
	# Make sure same value as in decoder (truncate precision), just in case
	frequency[i] = struct.unpack("<f",packed)[0]

for i in sorted(frequency, key=frequency.get):
	addbits(gbits,i,8)
	gbits.write(struct.pack("<f",frequency[i]))

symboltable = huffman(frequency)

print(CLEAR+"Done %i symbols"%(len(frequency)))

for f in precalc_frames:
	cframe += 1
	fps = cframe/(time.time()-starttime)
	#if cframe%10 == 0:
	print(CLEAR+"Frame: %i/%i (%.2f%%) %is remaining"%(cframe,nbframes,(cframe/nbframes)*100,(nbframes-cframe)/fps),end='\r')

	diff = f ^ lastframe
	if not diff.any():
		nbdup += 1
		addbits(gbits,3,2) # Duplicate frame
		continue

	frame_size = 2+width*height

	huffcomp(f,rlebs)
	deltacomp(f,lastframe,diff,deltabs)

	if len(rlebs) >= frame_size:
		nboverh += 1
	if len(deltabs) >= frame_size:
		nboverd += 1

	ratiohuff += (len(rlebs)-2)/(frame_size-2)
	ratiodelta += (len(deltabs)-2)/(frame_size-2)
	encodedframes += 1

	if len(rlebs) <= len(deltabs):
		gbits.write(rlebs)
		nbhuff += 1
	else:
		gbits.write(deltabs)
		nbdelta += 1

	lastframe = f

endtime = time.time()

nbbits = len(gbits)
if nbbits%8 != 0:
	for i in range(8-(nbbits%8)):
		gbits.write(False)

size = 0
with open(outfile,"wb") as ofile:
	ofile.write(gbits.read(bytes))
	size = ofile.tell()

print("")
print("----------")
print("Number of frames: %i"%(cframe))
print("Huff frames     : %i (%.2f%%)"%(nbhuff,(nbhuff/nbframes)*100))
print("Delta frames    : %i (%.2f%%)"%(nbdelta,(nbdelta/nbframes)*100))
print("Duplicate frames: %i (%.2f%%)"%(nbdup,(nbdup/nbframes)*100))
print("----------")
print("Huff frames over      : %i (%.2f%%)"%(nboverh,(nboverh/encodedframes)*100))
print("Delta frames over     : %i (%.2f%%)"%(nboverd,(nboverd/encodedframes)*100))
print("Huff frames avg ratio : %.2f%%"%((1-(ratiohuff/encodedframes))*100))
print("Delta frames avg ratio: %.2f%%"%((1-(ratiodelta/encodedframes))*100))
print("----------")
print("Normal    : %i bytes"%normal_size)
print("Compressed: %i bytes"%size)
print("Ratio     : %.2f%%"%((1-(size/normal_size))*100))
print("Speed     : %.2ffps"%(nbframes/(endtime-starttime)))