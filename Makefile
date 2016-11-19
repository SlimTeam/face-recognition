# determine build (debug, release)
ifndef BUILD
BUILD = debug
endif

# determine matrix library (netlib, mkl, cuda)
# TODO: implement flags for cuda option
ifndef MATLIB
MATLIB = netlib
endif

# determine compiler suite (gcc/g++, icc/icpc)
ifeq ($(MATLIB), netlib)
CC = gcc
CXX = g++
else ifeq ($(ENV), mkl)
CC = icc
CXX = icpc
endif

# determine compiler and linker flags
CFLAGS =
LFLAGS = -lm

ifeq ($(BUILD), release)
CFLAGS += -O3
else ifeq ($(BUILD), debug)
CFLAGS += -g -Wall
endif

ifeq ($(MATLIB), netlib)
LFLAGS += -lblas -llapacke
else ifeq ($(MATLIB), mkl)
CFLAGS += -D INTEL_MKL
LFLAGS += -mkl
endif

INCS = src/database.h src/image.h src/image_entry.h src/matrix.h src/timing.h
OBJS = database.o image.o image_entry.o matrix.o pca.o lda.o ica.o timing.o
BINS = face-rec test-matrix test-image

all: config $(BINS)

config:
	$(info BUILD  = $(BUILD))
	$(info MATLIB = $(MATLIB))
	$(info CC     = $(CC))
	$(info CXX    = $(CXX))
	$(info CFLAGS = $(CFLAGS))
	$(info LFLAGS = $(LFLAGS))

image.o: src/image.h src/image.cpp
	$(CXX) -c $(CFLAGS) src/image.cpp -o $@

image_entry.o: src/image_entry.h src/image_entry.cpp
	$(CXX) -c $(CFLAGS) src/image_entry.cpp -o $@

matrix.o: image.o src/matrix.h src/matrix.cu
	$(CXX) -c -x c++ $(CFLAGS) src/matrix.cu -o $@

database.o: image.o image_entry.o matrix.o src/database.h src/database.cpp
	$(CXX) -c $(CFLAGS) src/database.cpp -o $@

timing.o: src/timing.h src/timing.cpp
	$(CXX) -c $(CFLAGS) src/timing.cpp -o $@

pca.o: matrix.o timing.o src/database.h src/pca.cpp
	$(CXX) -c $(CFLAGS) src/pca.cpp -o $@

lda.o: matrix.o timing.o src/database.h src/lda.cpp
	$(CXX) -c $(CFLAGS) src/lda.cpp -o $@

ica.o: matrix.o src/database.h src/ica.cpp
	$(CXX) -c $(CFLAGS) src/ica.cpp -o $@

main.o: database.o timing.o src/main.cpp
	$(CXX) -c $(CFLAGS) src/main.cpp -o $@

test_image.o: matrix.o image.o src/test_image.cpp
	$(CXX) -c $(CFLAGS) src/test_image.cpp -o $@

test_matrix.o: matrix.o src/test_matrix.cpp
	$(CXX) -c $(CFLAGS) src/test_matrix.cpp -o $@

face-rec: image.o image_entry.o matrix.o database.o pca.o lda.o ica.o timing.o main.o
	$(CXX) $(CFLAGS) $^ $(LFLAGS) -o $@

test-image: image.o matrix.o test_image.o
	$(CXX) $(CFLAGS) $^ $(LFLAGS) -o $@

test-matrix: matrix.o test_matrix.o
	$(CXX) $(CFLAGS) $^ $(LFLAGS) -o $@

test-cublas: src/matrix.h src/matrix.cu test_image.o
	nvcc -c src/matrix.cu -o matrix.o
	$(CXX) $(CFLAGS) matrix.o test_image.o -lm -lcudart -lcublas -o $@

clean:
	rm -f *.o $(BINS)
	rm -rf test_images train_images
