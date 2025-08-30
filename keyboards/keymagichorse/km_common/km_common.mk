
# 74HC595
ifeq ($(strip $(SHIFT595_ENABLED)), yes)
    OPT_DEFS += -DSHIFT595_ENABLED
    VPATH += keyboards/keymagichorse/km_common/74hc595/
    SRC += km_common/74hc595/74hc595.c
endif

ifeq ($(strip $(KM_DEBUG)), yes)
	OPT_DEFS += -DKM_DEBUG
	SRC+= km_common/rtt/SEGGER_RTT.c
	SRC+= km_common/rtt/SEGGER_RTT_printf.c
	VPATH += keyboards/keymagichorse/km_common/rtt
endif   

VPATH += keyboards/keymagichorse/km_common/

ifeq ($(strip $(BLUETOOTH_DRIVER)), bhq)
    # 低功耗
    ifeq ($(strip $(KB_LPM_ENABLED)), yes)
        OPT_DEFS += -DKB_LPM_ENABLED
        SRC += km_common/${KB_LPM_DRIVER}.c
    endif

    ifeq ($(strip $(KB_CHECK_BATTERY_ENABLED)), yes)
        OPT_DEFS += -DKB_CHECK_BATTERY_ENABLED
        # 打开QMK的ADC读取功能
        ANALOG_DRIVER_REQUIRED = yes
        SRC += km_common/battery.c
    endif

    SRC += km_common/bhq_common.c
    SRC += km_common/transport.c
    SRC += km_common/wireless.c

endif
