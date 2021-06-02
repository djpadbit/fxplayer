import cv2,time
import scipy.fftpack as fft
import numpy as np

indata = (np.random.random((8,8))-0.5)*255

iters = 100000

st = time.time()
for i in range(iters):
	cv2.dct(indata)
cvtime = time.time()-st

st = time.time()
for i in range(iters):
	fft.dct(fft.dct(indata,axis=0,norm="ortho"),axis=1,norm="ortho")
scitime = time.time()-st

print("Scipy time is %f (%f per iter)"%(scitime,scitime/iters))
print("CV2 time is %f (%f per iter)"%(cvtime,cvtime/iters))