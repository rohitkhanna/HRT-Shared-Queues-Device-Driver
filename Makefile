# Makefile for BeagleBoard
#	How to Use --->
# 		export PATH=/usr/local/angstrom/arm/bin:$PATH
# 		make ARCH=arm CROSS_COMPILE=arm-angstrom-linux-gnueabi-
	

# Compiler options
CC = /usr/local/angstrom/arm/bin/arm-angstrom-linux-gnueabi-gcc
FLAGS = -Wall -I -L -g -lpthread

#Remote deploy ( plz make sure SSH_IP is IP address of beagleboard )
SSH_IP = 192.168.1.113
SSH_USER = root
SSH_DIR = /home/root/Lab2

DRIVER_NAME1 = hrt-driver
DRIVER_NAME2 = squeue-driver
APP_NAME1 = hrt-tester
APP_NAME2 = squeue-tester

obj-m := $(DRIVER_NAME1).o $(DRIVER_NAME2).o
PWD := $(shell pwd)
KDIR := /home/rohit/cse598-ESP/kernel/kernel/

all:
	make -C $(KDIR) SUBDIRS=$(PWD) modules
	
app-hrt: $(APP_NAME1) 
	$(CC) $(FLAGS) -o $(APP_NAME1) $(APP_NAME1).c

app-squeue: $(APP_NAME1) 
	$(CC) $(FLAGS) -o $(APP_NAME2) $(APP_NAME2).c
	

clean:
	make -C $(KDIR) SUBDIRS=$(PWD) clean
	rm -f $(APP_NAME)

load-driver: 
	@echo "Copying ko to BeagleBoard..."
	scp $(DRIVER_NAME1).ko $(DRIVER_NAME2).ko $(SSH_USER)@$(SSH_IP):$(SSH_DIR)
	@echo "Done!"
	
load-app:
	@echo "Copying app to BeagleBoard..."
	scp  $(APP_NAME2) $(SSH_USER)@$(SSH_IP):$(SSH_DIR)
	@echo "Done!"


