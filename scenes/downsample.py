import numpy as np
import cv2
import sys

if __name__ == "__main__":
    lines = sys.stdin.readlines()

    for line in lines:
        filename = line.strip()

        img = cv2.imread(filename)
        steps_y = img.shape[0] // 1024
        steps_x = img.shape[1] // 1024

        if steps_y == 0: steps_y = 1
        if steps_x == 0: steps_x = 1

        downsample = img[::steps_y, ::steps_x]

        cv2.imwrite("./compressed/" + filename, downsample)
        print("Finished ./compressed/" + filename + ". Reduced (" + str(img.shape[0]) + ", " + str(img.shape[1]) + ") -> (" + str(int(img.shape[0] / steps_y)) + ", " + str(int(img.shape[1] / steps_x)) + ").")