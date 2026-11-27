################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Src/gde021a1.c \
../Src/main.c \
../Src/stm32l0538_discovery.c \
../Src/stm32l0538_discovery_epd.c \
../Src/stm32l0xx_hal_msp.c \
../Src/stm32l0xx_it.c \
../Src/system_stm32l0xx.c 

OBJS += \
./Src/gde021a1.o \
./Src/main.o \
./Src/stm32l0538_discovery.o \
./Src/stm32l0538_discovery_epd.o \
./Src/stm32l0xx_hal_msp.o \
./Src/stm32l0xx_it.o \
./Src/system_stm32l0xx.o 

C_DEPS += \
./Src/gde021a1.d \
./Src/main.d \
./Src/stm32l0538_discovery.d \
./Src/stm32l0538_discovery_epd.d \
./Src/stm32l0xx_hal_msp.d \
./Src/stm32l0xx_it.d \
./Src/system_stm32l0xx.d 


# Each subdirectory must supply rules for building sources it contributes
Src/%.o Src/%.su Src/%.cyclo: ../Src/%.c Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0plus -std=gnu11 -g3 '-D__weak=__attribute__((weak))' '-D__packed=__attribute__((__packed__))' -DUSE_HAL_DRIVER -DSTM32L053xx -c -I../Inc -I../Drivers/STM32L0xx_HAL_Driver/Inc -I../Drivers/STM32L0xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32L0xx/Include -I../Drivers/CMSIS/Include -Og -ffunction-sections -Wall -fstack-usage -fcyclomatic-complexity -ftls-model=local-exec -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-Src

clean-Src:
	-$(RM) ./Src/gde021a1.cyclo ./Src/gde021a1.d ./Src/gde021a1.o ./Src/gde021a1.su ./Src/main.cyclo ./Src/main.d ./Src/main.o ./Src/main.su ./Src/stm32l0538_discovery.cyclo ./Src/stm32l0538_discovery.d ./Src/stm32l0538_discovery.o ./Src/stm32l0538_discovery.su ./Src/stm32l0538_discovery_epd.cyclo ./Src/stm32l0538_discovery_epd.d ./Src/stm32l0538_discovery_epd.o ./Src/stm32l0538_discovery_epd.su ./Src/stm32l0xx_hal_msp.cyclo ./Src/stm32l0xx_hal_msp.d ./Src/stm32l0xx_hal_msp.o ./Src/stm32l0xx_hal_msp.su ./Src/stm32l0xx_it.cyclo ./Src/stm32l0xx_it.d ./Src/stm32l0xx_it.o ./Src/stm32l0xx_it.su ./Src/system_stm32l0xx.cyclo ./Src/system_stm32l0xx.d ./Src/system_stm32l0xx.o ./Src/system_stm32l0xx.su

.PHONY: clean-Src

