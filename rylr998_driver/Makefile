obj-m += rylr998_driver.o

module:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	dtc -@ -I dts -O dtb -o rylr998.dtbo rylr998.dts

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -f rylr998.dtbo
