import cv2
import sys

if __name__ == "__main__":
    lines = sys.stdin.readlines()
    inpath = "./original/"
    outpath = "./downsampled/"

    # Will only force this MAX_SIZE as long as img.shape / MAX_SIZE >= 2 
    MAX_SIZE = 512

    for line in lines:
        filename = line.strip()

        img = cv2.imread(inpath + filename, cv2.IMREAD_UNCHANGED)

        if img is None: continue

        steps_y = img.shape[0] // MAX_SIZE
        steps_x = img.shape[1] // MAX_SIZE

        if steps_y == 0: steps_y = 1
        if steps_x == 0: steps_x = 1

        downsample = img[::steps_y, ::steps_x]

        cv2.imwrite(outpath + filename, downsample)
        print("Finished " + outpath + filename + ". Reduced (" + str(img.shape[0]) + ", " + str(img.shape[1]) + ") -> (" + str(int(img.shape[0] / steps_y)) + ", " + str(int(img.shape[1] / steps_x)) + ").")