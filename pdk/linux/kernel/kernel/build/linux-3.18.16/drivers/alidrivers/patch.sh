#!/bin/bash

# kernel code fetching checking
echo "will execute $KERNEL_DIR/src/$LINUX_KERNEL_VER.sh" ; \
$KERNEL_DIR/src/$LINUX_KERNEL_VER.sh ; \
echo "execute $KERNEL_DIR/src/$LINUX_KERNEL_VER.sh finished!" 

# create some symbol links for all .c and .h file in 'alimmc/core' and 'alimmc/card',except core.c and core.h
# core.c and core.h don't need create a link since I had modified them.
echo "------==========Prepare for create some links for mmc driver============-------"
echo "LINUX_DIR: $LINUX_DIR"
echo "ALIDRIVERS_DIR: $ALIDRIVERS_DIR"

if [ "$RELEASEPKT" == "Y" ]; then
   unset ALIDRIVERS_DIR
   ALIDRIVERS_DIR=$LINUX_DIR/drivers/alidrivers/modules
   echo "ALIDRIVERS_DIR Changed to: $ALIDRIVERS_DIR"
   rm -rf $LINUX_DIR/drivers/alidrivers;cp -a $KERNEL_DIR/../alidrivers $LINUX_DIR/drivers/ 
fi

# try to patch the common kernel files
cd $LINUX_DIR/include/linux
if [ "$RELEASEPKT" == "Y" ]; then
ln -sf ../../drivers/alidrivers/include/ali_gpio.h ali_gpio.h
ln -sf ../../drivers/alidrivers/include/ali_reg.h ali_reg.h
ln -sf ../../drivers/alidrivers/include/ali_interrupt.h ali_interrupt.h
ln -sf ../../drivers/alidrivers/include/ali_dma.h ali_dma.h
ln -sf ../../drivers/alidrivers/include/ali_cache.h ali_cache.h
else
ln -sf ../../../../../alidrivers/include/ali_gpio.h ali_gpio.h
ln -sf ../../../../../alidrivers/include/ali_reg.h ali_reg.h
ln -sf ../../../../../alidrivers/include/ali_interrupt.h ali_interrupt.h
ln -sf ../../../../../alidrivers/include/ali_dma.h ali_dma.h
ln -sf ../../../../../alidrivers/include/ali_cache.h ali_cache.h
cd $LINUX_DIR/drivers
rm -rf alidrivers
ln -sf ../../../../alidrivers alidrivers
fi

if [ "$LINUX_KERNEL_VER" == "linux-linaro-3.4-rc3" ]; then
  cd mtd 
  rm -rf ubi 
  echo "--== ubi mlc patch ==--"
  ln -sf ../../../../../alidrivers/kernelpatchs/linux-linaro-3.4-rc3/ubi ubi 
  cd -
fi

# check if need to copy the *.bin file to *.o
cd $ALIDRIVERS_DIR ; find ./ -name "*.bin" | awk -F "." '{print $2}' | xargs -i -t cp -f ./{}.bin  ./{}.o

