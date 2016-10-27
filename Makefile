# Change this path if the SDK was installed in a non-standard location
AMDAPPSDK = "/opt/AMDAPPSDK-3.0/include"
# Change this path if your libOpenCL.so library is located elsewhere
LIBOPENCL = "/usr/lib/x86_64-linux-gnu/amdgpu-pro/libOpenCL.so"

CC = gcc
CPPFLAGS = -std=gnu99 -pedantic -Wextra -Wall -ggdb \
    -Wno-deprecated-declarations \
    -Wno-overlength-strings \
    -I${AMDAPPSDK}
LDFLAGS = -rdynamic
LDLIBS = ${LIBOPENCL}
OBJ = main.o blake.o
INCLUDES = blake.h param.h _kernel.h

all : silentarmy

silentarmy : ${OBJ}
	${CC} -o silentarmy ${OBJ} ${LDFLAGS} ${LDLIBS}

${OBJ} : ${INCLUDES}

_kernel.h: input.cl param.h
	echo 'const char *ocl_code = R"_mrb_(' >$@
	cpp $< >>$@
	echo ')_mrb_";' >>$@

test: silentarmy
	./silentarmy --nonces 100 -v -v 2>&1 | grep Soln: | \
	    diff -u sols-100 - | cut -c 1-75

clean :
	rm -f silentarmy _kernel.h *.o _temp_*

re : clean all

.cpp.o :
	${CC} ${CPPFLAGS} -o $@ -c $<
