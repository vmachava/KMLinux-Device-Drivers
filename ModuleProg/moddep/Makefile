obj-m	:= add.o
obj-m	+= avg.o

all:
	make -C /lib/modules/`uname -r`/build M=`pwd` modules

clean:
	make -C /lib/modules/`uname -r`/build M=`pwd` clean

install:
	make -C /lib/modules/`uname -r`/build M=`pwd` modules_install
	depmod -a
