import cv2
import numpy as np
from bitstream import BitStream

with open("frames.bin",'rb') as f:
	good = f.read()

with open("framesdec.bin",'rb') as f:
	bad = f.read()

size = 128*64
size_b = size//8

if len(bad) != len(good):
	print("Length mismatch, is %i should be %i"%(len(bad),len(good)))
	print("(%i frames, %i frames)"%(len(bad)//size_b,len(good)//size_b))

def toframe(f,dat):
	print(len(dat))
	b = BitStream()
	b.write(dat)
	for i in range(len(b)):
		if b.read(bool):
			f[i//128,i%128] = 255
		else:
			f[i//128,i%128] = 0

goodframe = np.zeros((64,128), np.uint8)
badframe = np.zeros((64,128), np.uint8)
frames = len(bad)//(128*64//8)
for f in range(frames):
	for pi in range(size_b):
		if bad[f*size_b+pi] != good[f*size_b+pi]:
			print("Frame %i mismatch"%f)
			print(bad[f*size_b+pi],good[f*size_b+pi])
			toframe(goodframe,good[f*size_b:(f+1)*size_b])
			toframe(badframe,bad[f*size_b:(f+1)*size_b])
			cv2.imshow("Good",cv2.resize(goodframe,(128*4,64*4),interpolation=cv2.INTER_NEAREST))
			cv2.imshow("Bad",cv2.resize(badframe,(128*4,64*4),interpolation=cv2.INTER_NEAREST))
			#fdiffcol = cv2.cvtColor(xdiff,cv2.COLOR_GRAY2BGR)
			#for idx in chunkdiffs:
			#	srx = (idx%wd)*dc_chunk_size
			#	sry = (idx//wd)*dc_chunk_size
			#	cv2.rectangle(fdiffcol,(srx,sry),(srx+dc_chunk_size,sry+dc_chunk_size),(0,0,255),1)
			#cv2.imshow("Diff",cv2.resize(fdiffcol,(128*4,64*4),interpolation=cv2.INTER_NEAREST))
			cv2.waitKey(0)
			cv2.destroyAllWindows()
			break