#
# Makefile for ALi mmc driver
#


ali_mmc-objs	:= mmc-ali.o mmc-ali-hw-v3.o mmc-ali-hw-v4.o mmc-ali-m3733-reset.o mmc-ali-m3733-clock-gate.o mmc-ali-m3733-pinmux.o
obj-$(CONFIG_ALI_MMC)	:= ali_mmc.o

.PHONY : clean
clean:
	@echo "mmc driver clean..."
	@find . -name '*.o' | xargs rm -rf
	@find . -name '*.cmd' | xargs rm -rf
	@find . -name 'modules.order' | xargs rm -rf
	@find . -name 'modules.builtin' | xargs rm -rf
	@find . -name '*.mod.c' | xargs rm -rf

.PHONY : cleanln
cleanln:
	@echo "mmc driver clean link file..."
	@find . -type l |xargs rm -rf
