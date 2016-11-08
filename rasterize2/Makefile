CFLAGS= \
	-ffast-math \
	-O3 \
	-std=gnu11 \
	-mmmx -msse -msse2 -msse3 -msse4a -mssse3 -mfpmath=sse \
	-falign-functions -falign-jumps -falign-labels -falign-loops \
	-fbranch-probabilities -fbranch-target-load-optimize2 \
	-fcprop-registers \
	-fcrossjumping \
	-finline-functions -finline-small-functions \
	-fipa-matrix-reorg \
	-fmodulo-sched \
	-fpeephole2 -fpeephole \
	-fpeel-loops \
	-foptimize-sibling-calls \
	-foptimize-register-move \
	-frounding-math \
	-fsel-sched-pipelining \
	-funsafe-math-optimizations \
	-funsafe-loop-optimizations \
	-funroll-all-loops \
	-masm=intel -m3dnow -mtune=core2

LIBS=-lm
OBJECTS=cityscape.o \
	rasterize.o \
	fixedmath.o \
	main.o
	
all: $(OBJECTS)
	gcc $(OBJECTS) -lm -lGL -lglut -lGLU -o raster
	
clean:
	rm -r *.o
