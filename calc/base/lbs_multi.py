'''
Measuring and learning from the Laserbeamsize package which is taken as reference
https://pypi.org/project/laserbeamsize/
https://laserbeamsize.readthedocs.io/en/latest/01-Definitions.html

--------------------------------------

The function basic_beam_size_naive() is not public in the laserbeamsize package.
It uses raw python loops over image data and very-very slow and impractical.
So if need to try it for learning purposes, one have to hack into the package:
In file "calc\.venv\Lib\site-packages\laserbeamsize\analysis.py"

__all__ = ('basic_beam_size',
           'beam_size',
           'basic_beam_size_naive' # <----- add the name into __all__
           )

--------------------------------------
Intel i7-2600 CPU 3.40GHz

Image: ../../beams/beam_8b_ast.pgm
Frames: 30

calc_bullseye
29: center=[1534,981], diam=[1474,1120], angle=-11.8°
Elapsed: 1.703, FPS: 17.6

lbs.basic_beam_size
29: center=[1534,981], diam=[1474,1120], angle=11.8°
Elapsed: 3.031, FPS: 9.9

lbs.beam_size
29: center=[1573,964], diam=[889,724], angle=0.3°
Elapsed: 120.422, FPS: 0.2

'''

import numpy as np
import imageio.v3 as iio
import laserbeamsize as lbs
import time

#FRAMES = 30
FRAMES = 1

# dot* files are tiny and do not have practical meaning but they allows to track calculations in each pixel
#FILENAME = "../../beams/dot_8b.pgm"
#FILENAME = "../../beams/dot_8b_ast.pgm"
#FILENAME = "../../beams/beam_8b.pgm"
#FILENAME = "../../beams/beam_8b_ast.pgm"
FILENAME = "../../tmp/real_beams/test_image_45.pgm"

# Alternative version is taken from Bullseye projects
# https://github.com/jordens/bullseye
# Semantically it's the same as lbs.basic_beam_size() but gives better performance
def calc_bullseye(im):
    #print(f"Image=\n{im}")
    #print(f"Shape={im.shape}")

    y, x = np.ogrid[:im.shape[0], :im.shape[1]]

    imx = im.sum(axis=0)[None, :]
    imy = im.sum(axis=1)[:, None]
    #print(f"Sums over Y: imx=\n{imx}")
    #print(f"Sums over X: imy=\n{imy}")
    #print(f"Sums shapes: imx={imx.shape}, imy={imy.shape}")

    p = float(imx.sum()) or 1.
    #print(f"Total energy: p={p}")

    xc = (imx * x).sum() / p
    yc = (imy * y).sum() / p
    x = x - xc
    y = y - yc
    xx = (imx * x**2).sum() / p
    yy = (imy * y**2).sum() / p
    xy = (im * x * y).sum() / p
    #print(f"Moments: xx={xx}, yy={yy}, xy={xy}")

    dx = 2 * np.sqrt(2) * np.sqrt(xx + yy + np.sign(xx - yy) * np.sqrt((xx - yy)**2 + 4 * xy**2))
    dy = 2 * np.sqrt(2) * np.sqrt(xx + yy - np.sign(xx - yy) * np.sqrt((xx - yy)**2 + 4 * xy**2))
    phi = 0.5 * np.arctan2(2 * xy, (xx - yy))
    return xc, yc, dx, dy, phi

def measure(func):
    tm = time.process_time()
    for i in range(FRAMES):
        x, y, dx, dy, phi = func(image, 
                                #corner_fraction=0.2,
                                #iso_noise=False,
                                )
        print(f"{i}: center=[{x:.0f},{y:.0f}], diam=[{dx:.0f},{dy:.0f}], angle={(phi * 180/3.1416):.1f}°")
    elapsed = time.process_time() - tm
    print(f"Elapsed: {elapsed:.3f}, FPS: {(FRAMES / elapsed) if elapsed > 0 else 0:.1f}")

if __name__ == "__main__":
    print(f"Image: {FILENAME}")
    image = iio.imread(FILENAME)

    print(f"Frames: {FRAMES}")

    #print("\ncalc_bullseye")
    #measure(calc_bullseye)

    #print("\nlbs.basic_beam_size")
    #measure(lbs.basic_beam_size)

    print("\nlbs.beam_size")
    measure(lbs.beam_size)

    #print("\nlbs.basic_beam_size_naive")
    #measure(lbs.basic_beam_size_naive)
