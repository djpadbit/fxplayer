import cv2,math,time,struct,argparse
from bitstream import BitStream
import numpy as np
from skimage.util.shape import view_as_blocks

parser = argparse.ArgumentParser(description='Converter')
parser.add_argument("file", help="Input video")
parser.add_argument("output", help="Optional output file", nargs='?', default="data.bin")
parser.add_argument("-s", "--show", help="Show dct results", action="store_true")
parser.add_argument("-p", "--play", help="Play video in dct", action="store_true")
parser.add_argument("-d", "--debug", help="Print dct results ", action="store_true")
parser.add_argument("-q", "--quality", help="Scale quantization matrix", dest="quality", type=float, default=1.0)
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
normal_size = nbframes*width*height
duration = nbframes/fps
print("Video: %i frames (%i:%i) @ %ffps (%ix%i)"%(nbframes,duration/60,duration%60,fps,width,height))
print("Writing to file: %s"%outfile)

def img2bin(image):
	gray = cv2.cvtColor(f, cv2.COLOR_BGR2GRAY)
	return (gray-128).astype(np.int8)

quant = np.array([[16,11,10,16,24,40,51,61],
				[12,12,14,19,26,58,60,55],
				[14,13,16,24,40,57,69,56],
				[14,17,22,29,51,87,80,62],
				[18,22,37,56,68,109,103,77],
				[24,36,55,64,81,104,113,92],
				[49,64,78,87,103,121,120,101],
				[72,92,95,98,112,100,103,99]])*cmd_args.quality

quant = np.rint(quant).astype(np.uint16)
print(quant)

# 3 -> Duplicate frame
# 2 -> Delta Frame
# 1 -> Huffman Frame
# 0 -> END

gbits = BitStream()
tempbs = BitStream()

symboltable = {}
valsymboltable = {}

frequency = {}
valfrequency = {}

rle_min_same = 16
dc_chunk_size = 8 # needs to divide width&height
dc_chunk_bits = int(math.log((width*height)//(dc_chunk_size*dc_chunk_size),2))
print("DC Chunk size: %i"%(dc_chunk_size))
print("DC Chunk bits: %i"%(dc_chunk_bits))

def addbits(bs,by,nb):
	for i in range(nb):
		bs.write((by >> (nb-1-i)) & 1 == 1)

def huffcomp(dcts,bs):
	bs.read()
	tempbs.read()
	addbits(bs,1,2)
	for dct in dcts:
		bs.write(dct)

def deltacomp(dcts,xdiff,bs):
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
		bs.write(dcts[idx])

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

def add_freq(freqt, symb):
	if "tot" not in freqt:
		freqt["tot"] = 1
	else:
		freqt["tot"] += 1
	if symb in freqt:
		freqt[symb] += 1
	else:
		freqt[symb] = 1
#(0, 0),
zigzag = [(0, 1), (1, 0),
		(2, 0), (1, 1), (0, 2),
		(0, 3), (1, 2), (2, 1), (3, 0),
		(4, 0), (3, 1), (2, 2), (1, 3), (0, 4),
		(0, 5), (1, 4), (2, 3), (3, 2), (4, 1), (5, 0),
		(6, 0), (5, 1), (4, 2), (3, 3), (2, 4), (1, 5), (0, 6),
		(0, 7), (1, 6), (2, 5), (3, 4), (4, 3), (5, 2), (6, 1), (7, 0),
		(7, 1), (6, 2), (5, 3), (4, 4), (3, 5), (2, 6), (1, 7),
		(2, 7), (3, 6), (4, 5), (5, 4), (6, 3), (7, 2),
		(7, 3), (6, 4), (5, 5), (4, 6), (3, 7),
		(4, 7), (5, 6), (6, 5), (7, 4),
		(7, 5), (6, 6), (5, 7),
		(6, 7), (7, 6),
		(7, 7)]

def do_dct(chunk):
	quantized = np.rint(cv2.dct(chunk.astype(np.single))/quant).astype(np.int8)
	output = []
	nbzeros = 0
	lastval = 256
	lastnb = 0
	for pos in zigzag:
		val = quantized[pos[0]][pos[1]]
		if val != lastval:
			while lastnb >= 16:
				lastnb -= 15
				if lastval == 0:
					output.append((0xF0,0))
				else:
					output.append((0xF | ((nbzeros & 0xF) << 4),lastval))
					nbzeros = 0
			if lastval != 256 and lastval != 0:
				output.append(((lastnb & 0xF) | ((nbzeros & 0xF) << 4),lastval))
				nbzeros = 0
			elif lastval == 0:
				nbzeros = lastnb
			lastnb = 1
			lastval = val
		else:
			lastnb += 1
	if lastnb != 0:
		if lastval == 0:
			output.append((0,0))
		else:
			output.append(((lastnb & 0xF) | ((nbzeros & 0xF) << 4),lastval))
	return quantized[0][0],output,quantized


print("Applying DCT & Generating frequency table")
precalc_frames = []
precalc_frames_dct = {}
lastframeflat = np.zeros((height,width), np.uint8)
freqframes = 0
while vid.isOpened():
	r,f = vid.read()
	if not r:
		break
	f = img2bin(f)
	idx = len(precalc_frames)
	precalc_frames.append(f)
	if not (f ^ lastframeflat).any():
		continue
	print(CLEAR+"Frame: %i/%i (%.2f%%)"%(freqframes,nbframes,(freqframes/nbframes)*100),end='\r')

	wd = width//dc_chunk_size
	hd = height//dc_chunk_size

	chunks = view_as_blocks(f,(dc_chunk_size,dc_chunk_size))

	if cmd_args.show:
		dcto = []

		temph = []
		for linechunks in chunks:
			for i,chunk in enumerate(linechunks):
				dct = np.rint(cv2.dct(chunk.astype(np.single))/quant)
				idct = np.clip(cv2.dct(dct*quant,flags=cv2.DCT_INVERSE),-128,127)
				cout = np.rint(idct+128).astype(np.uint8)
				if cmd_args.debug: # and (i%wd) == wd-1:
					print("--------------------")
					print("Chunk:")
					print(chunk)
					print("DCT:")
					print(dct)
					print("IDCT:")
					print(idct)
					print("IDCT RINT:")
					print(np.rint(idct))
					print("Out:")
					print(cout)
				#cout = (np.rint(cv2.dct((np.rint(cv2.dct(chunk.astype(np.single))/quant).astype(np.int8).astype(np.single))*quant,flags=cv2.DCT_INVERSE))+127).astype(np.uint8)
				temph.append(cout)

			dcto.append(np.hstack(temph))
			temph.clear()
		dcto = np.vstack(dcto).astype(np.uint8)

		img = np.concatenate(((f.astype(np.int16)+128).astype(np.uint8),dcto),axis=1)
		cv2.imshow("Image",cv2.resize(img,(width*4*2,height*4),interpolation=cv2.INTER_NEAREST))
		waittime = 0
		if cmd_args.play:
			waittime = round((1/fps)*1000)
		if cv2.waitKey(waittime) == 27:
			cv2.destroyAllWindows()
			exit()

	precalc_frames_dct[idx] = []
	for linechunks in chunks:
		for chunk in linechunks:
			dct = do_dct(chunk)
			precalc_frames_dct[idx].append(dct)
			add_freq(valfrequency,dct[0])
			for i in dct[1]:
				add_freq(frequency,i[0])
				if i[0] != 0xF0 and i[0] != 0:
					add_freq(valfrequency,i[1])

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

def to_unsigned(val):
	if val < 0:
		return val+256
	return val

def add_table(freqs,bs):
	total = freqs["tot"]
	del freqs["tot"]
	addbits(bs,len(freqs)-1,8)
	for i in sorted(freqs, key=freqs.get):
		freqs[i] = freqs[i]/total
		packed = struct.pack("<f",freqs[i])
		# Make sure same value as in decoder (truncate precision), just in case
		freqs[i] = struct.unpack("<f",packed)[0]

	for i in sorted(freqs, key=freqs.get):
		addbits(bs,to_unsigned(i),8)
		bs.write(struct.pack("<f",freqs[i]))

	return huffman(freqs)

def dct_huffman(dct):
	out = []
	out.extend(valsymboltable[dct[0]])
	for i in dct[1]:
		out.extend(symboltable[i[0]])
		if i[0] != 0xF0 and i[0] != 0:
			out.extend(valsymboltable[i[1]])
	return out

for row in quant:
	for item in row:
		addbits(gbits,item,16)

symboltable = add_table(frequency,gbits)
valsymboltable = add_table(valfrequency,gbits)
"""
def mkst(vals):
	st = ""
	for i in vals:
		if i:
			st += "1"
		else:
			st += "0"
	return st

print("")
print("")
print("Marker:")
for i in sorted(frequency, key=frequency.get):
	print(to_unsigned(i),mkst(symboltable[i]))
print("Values:")
for i in sorted(valfrequency, key=valfrequency.get):
	print(to_unsigned(i),mkst(valsymboltable[i]))
print("")
"""

print(CLEAR+"Done marker:%i,values:%i symbols"%(len(frequency),len(valfrequency)))

for idx,f in enumerate(precalc_frames):
	cframe += 1
	fps = cframe/(time.time()-starttime)
	#if cframe%10 == 0:
	print(CLEAR+"Frame: %i/%i (%.2f%%) %is remaining"%(cframe,nbframes,(cframe/nbframes)*100,(nbframes-cframe)/fps),end='\r')

	diff = f ^ lastframe
	if not diff.any():
		nbdup += 1
		addbits(gbits,3,2) # Duplicate frame
		continue

	frame_size = 2+width*height*8

	dcts = [dct_huffman(dct) for dct in precalc_frames_dct[idx]]

	huffcomp(dcts,rlebs)
	deltacomp(dcts,diff,deltabs)

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