################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../source/drivers/ir_remote.c \
../source/drivers/joystick.c \
../source/drivers/menu.c \
../source/drivers/pong_game.c \
../source/drivers/st7735_simple.c 

C_DEPS += \
./source/drivers/ir_remote.d \
./source/drivers/joystick.d \
./source/drivers/menu.d \
./source/drivers/pong_game.d \
./source/drivers/st7735_simple.d 

OBJS += \
./source/drivers/ir_remote.o \
./source/drivers/joystick.o \
./source/drivers/menu.o \
./source/drivers/pong_game.o \
./source/drivers/st7735_simple.o 


# Each subdirectory must supply rules for building sources it contributes
source/drivers/%.o: ../source/drivers/%.c source/drivers/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: MCU C Compiler'
	arm-none-eabi-gcc -D__REDLIB__ -DCPU_MKL25Z128VLK4 -DCPU_MKL25Z128VLK4_cm0plus -DSDK_OS_BAREMETAL -DFSL_RTOS_BM -DSDK_DEBUGCONSOLE=1 -DCR_INTEGER_PRINTF -DPRINTF_FLOAT_ENABLE=0 -D__MCUXPRESSO -D__USE_CMSIS -DDEBUG -I"C:\Users\Tanase\Desktop\Proiect MP MCUProjects\MKL25Z4_Main_Project\board" -I"C:\Users\Tanase\Desktop\Proiect MP MCUProjects\MKL25Z4_Main_Project\source" -I"C:\Users\Tanase\Desktop\Proiect MP MCUProjects\MKL25Z4_Main_Project" -I"C:\Users\Tanase\Desktop\Proiect MP MCUProjects\MKL25Z4_Main_Project\drivers" -I"C:\Users\Tanase\Desktop\Proiect MP MCUProjects\MKL25Z4_Main_Project\startup" -I"C:\Users\Tanase\Desktop\Proiect MP MCUProjects\MKL25Z4_Main_Project\utilities" -I"C:\Users\Tanase\Desktop\Proiect MP MCUProjects\MKL25Z4_Main_Project\CMSIS" -O0 -fno-common -g3 -gdwarf-4 -Wall -c -fmessage-length=0 -fno-builtin -ffunction-sections -fdata-sections -fmerge-constants -fmacro-prefix-map="$(<D)/"= -mcpu=cortex-m0plus -mthumb -D__REDLIB__ -fstack-usage -specs=redlib.specs -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.o)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


clean: clean-source-2f-drivers

clean-source-2f-drivers:
	-$(RM) ./source/drivers/ir_remote.d ./source/drivers/ir_remote.o ./source/drivers/joystick.d ./source/drivers/joystick.o ./source/drivers/menu.d ./source/drivers/menu.o ./source/drivers/pong_game.d ./source/drivers/pong_game.o ./source/drivers/st7735_simple.d ./source/drivers/st7735_simple.o

.PHONY: clean-source-2f-drivers

