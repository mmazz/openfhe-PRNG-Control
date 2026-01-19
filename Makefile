PROJECT_ROOT := $(shell pwd)
INSTALL_PREFIX := $(PROJECT_ROOT)/install

all:
	mkdir -p build
	cd build && \
	cmake -DCMAKE_INSTALL_PREFIX=$(INSTALL_PREFIX) \
	      -DBUILD_STATIC=OFF \
	      -DBUILD_SHARED=ON \
	      -DCMAKE_BUILD_TYPE=Release \
	      -DWITH_OPENMP=OFF \
	      -DBUILD_UNITTESTS=OFF \
	      -DBUILD_BENCHMARKS=OFF \
	      -DBUILD_EXTRAS=OFF .. && \
	make -j$(shell nproc) && \
	make install

clean:
	rm -rf build install

.PHONY: all clean
