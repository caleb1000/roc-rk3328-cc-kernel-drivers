obj-m += sm_s2309s_driver.o

all: app
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	-rm userapp
app:
	gcc -o userapp userapp.c
