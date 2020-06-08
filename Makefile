KERN_DIR = /home/book/imx_wksps/imx6ull-linux4988/imx-linux4.9.88

all:
	make -C $(KERN_DIR) M=`pwd` modules
	$(CROSS_COMPILE)gcc -o led_key_int_dev_test led_key_int_dev_test.c

clean:
	make -C $(KERN_DIR) M=`pwd` modules clean
	rm -rf modules.order
	rm -f led_key_int_dev_test

obj-m += led_key_int_dev.o

