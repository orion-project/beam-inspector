'''
Measuring and learning from the Laserbeamsize package which is taken as reference
https://pypi.org/project/laserbeamsize/
https://laserbeamsize.readthedocs.io/en/latest/01-Definitions.html


The function basic_beam_size_naive() is not public in the laserbeamsize package.
It uses raw python loops over image data and very-very slow and impractical.
So if need to try it for learning purposes, one have to hack into the package:
In file "calc\.venv\Lib\site-packages\laserbeamsize\analysis.py"

__all__ = ('basic_beam_size',
           'beam_size',
           'basic_beam_size_naive' # <----- add the name into __all__
           )
'''

import numpy as np
import imageio.v3 as iio
import laserbeamsize as lbs
import time

SAMPLES = 30

# dot* files are tiny and do not have practical meaning but they allows to track calculations in each pixel
#FILENAME = "../beams/dot_8b.pgm"
#FILENAME = "../beams/dot_8b_ast.pgm"
#FILENAME = "../beams/beam_8b.pgm"
FILENAME = "../beams/beam_8b_ast.pgm"


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

if __name__ == "__main__":
    image = iio.imread(FILENAME)

    t = time.process_time()

    for i in range(SAMPLES):
        x, y, dx, dy, phi = calc_bullseye(image)
        print(f"{i}: center=[{x:.0f},{y:.0f}], diam=[{dx:.0f},{dy:.0f}], angle={(phi * 180/3.1416):.1f}°")
        
        #x, y, dx, dy, phi = lbs.basic_beam_size_naive(image)
        #x, y, dx, dy, phi = lbs.basic_beam_size(image)
        #print(f"{i}: center=[{x:.0f},{y:.0f}], diam=[{dx:.0f},{dy:.0f}], angle={(phi * 180/3.1416):.1f}°")

    elapsed = time.process_time() - t
    print(f"Elapsed: {elapsed:.3f}, FPS: {(SAMPLES / elapsed) if elapsed > 0 else 0:.1f}")
