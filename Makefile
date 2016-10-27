# You may need to change this if the SDK was installed somewhere else
AMDAPPSDK = /opt/AMDAPPSDK-3.0/include

CC = gcc
CPPFLAGS = -std=c99 -pedantic -Wextra -Wall -ggdb \
    -Wno-deprecated-declarations \
    -I${AMDAPPSDK}
LDFLAGS = -rdynamic
LDLIBS = /usr/lib/x86_64-linux-gnu/amdgpu-pro/libOpenCL.so -lrt
OBJ = main.o blake.o
INCLUDES = blake.h param.h

all : silentarmy

silentarmy : ${OBJ} kernel.cl
	${CC} -o silentarmy ${OBJ} ${LDFLAGS} ${LDLIBS}

${OBJ} : param.h

kernel.cl: input.cl param.h
	cpp -o $@ $<

test: silentarmy
	./silentarmy --nonces 100 -v -v 2>&1 | grep Soln: | \
	    diff -u sols-100 - | cut -c 1-75

clean :
	rm -f silentarmy kernel.cl *.o _temp_*

re : clean all

.cpp.o :
	${CC} ${CPPFLAGS} -o $@ -c $<
