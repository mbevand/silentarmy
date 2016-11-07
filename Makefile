#EDiting for OSX
# Change this path if the SDK was installed in a non-standard location
OPENCL_HEADERS = "/System/Library/Frameworks/OpenCL.framework/Headers/"
# By default libOpenCL.so is searched in default system locations, this path
# lets you adds one more directory to the search path.
LIBOPENCL = "/System/Library/Frameworks/OpenCL.framework/Versions/Current/Libraries"

CC = gcc-6
CPPFLAGS = -std=gnu99 -pedantic -Wextra -Wall -ggdb \
    -Wno-deprecated-declarations \
    -Wno-overlength-strings \
		-I${OPENCL_HEADERS} \
		-framework OpenCL

LDFLAGS = -rdynamic -L${LIBOPENCL}
#LDLIBS = -lOpenCL
OBJ = main.o blake.o sha256.o
INCLUDES = blake.h param.h _kernel.h sha256.h

all : sa-solver

sa-solver : ${OBJ}
	${CC} -o sa-solver ${OBJ} ${LDFLAGS} ${LDLIBS} ${CPPFLAGS}

${OBJ} : ${INCLUDES}

_kernel.h : input.cl param.h
	echo 'const char *ocl_code = R"_mrb_(' >$@
	cpp $< >>$@
	echo ')_mrb_";' >>$@

test : sa-solver
	./sa-solver --nonces 100 -v -v 2>&1 | grep Soln: | \
	    diff -u testing/sols-100 - | cut -c 1-75

clean :
	rm -f sa-solver _kernel.h *.o _temp_*

re : clean all

.cpp.o :
	${CC} ${CPPFLAGS} -o $@ -c $<
