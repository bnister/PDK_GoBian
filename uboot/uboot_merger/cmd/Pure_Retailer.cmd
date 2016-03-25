[PARAMETER]
chip_name=3921
loader_entry=0x80100000
aux_code_len=0x3700
aux_code= boot.abs
bl_name= u-boot.bin_0319
output= output_uboot.abs 

[NAND CONFIG]
block_perchip=0x800
pages_perblock=0x100
bytes_perpage=0x2000
eccsec_size=0x400
eccredu_size=0x20
rowaddr_cycle=0x3
ecctype=0x0
read_timing=0x22
write_timing=0x22
eccsec_perpage=0x2
[CMD]
merge