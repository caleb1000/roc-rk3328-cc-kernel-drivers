obj-m += uart_driver.o

all: module app
	echo Builded Device Tree Overlay and kernel module

module:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	-rm userapp
app:
	gcc -o userapp userapp.c
