:: Math optimization flags are important here.
:: taken from http://spfrnd.de/posts/2018-03-10-fast-exponential.html
gcc -O3 -ffast-math -funsafe-math-optimizations -msse4.2 -o beam_render beam_render.c main.c && beam_render
