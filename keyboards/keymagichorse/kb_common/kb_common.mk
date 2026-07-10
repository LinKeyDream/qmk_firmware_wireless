KB_COMMON_DIR = keyboards/keymagichorse/kb_common
VPATH += ${KB_COMMON_DIR}



# 日记类型 不写默认就是不打开 uart是借用蓝牙的串口的简单调试可以
# rtt这个低功耗之后可能就用不了了，用起来就很麻烦，所以添加了uart借用bhq的串口用一下
KB_DEBUG ?= no
SRC+= kb_common/km_printf.c
ifeq ($(strip $(KB_DEBUG)), rtt)
	OPT_DEFS += -DKB_DEBUG
	OPT_DEFS += -DKB_DEBUG_RTT
	SRC+= kb_common/rtt/SEGGER_RTT.c
	SRC+= kb_common/rtt/SEGGER_RTT_printf.c
	VPATH += ${KB_COMMON_DIR}/kb_common/rtt
endif   
ifeq ($(strip $(KB_DEBUG)), uart_bhq)
	OPT_DEFS += -DKB_DEBUG_UART_BHQ
	OPT_DEFS += -DKB_DEBUG
endif   
ifeq ($(strip $(KB_DEBUG)), no)
    OPT_DEFS += -DNO_PRINT
    OPT_DEFS += -DNO_DEBUG
endif   

# 矩阵扫描相关
MATRIX_TYPE ?= default
ifneq ($(filter $(MATRIX_TYPE), default shift595),)
    ifeq ($(strip $(MATRIX_TYPE)), shift595)
        OPT_DEFS += -DSHIFT595_ENABLED
        CUSTOM_MATRIX = lite	
        VPATH += ${KB_COMMON_DIR}/matrix/matrix_type/shift595/
        VPATH += ${KB_COMMON_DIR}/matrix/matrix_type/shift595/74hc595/
        SRC += kb_common/matrix/matrix_type/shift595/74hc595/74hc595.c
        SRC += kb_common/matrix/matrix_type/shift595/matrix_shift595.c
    endif
endif



# 静电容
ifeq ($(strip $(KB_EC_ENABLED)), yes)
	OPT_DEFS += -DKB_EC_ENABLED
    ifeq ($(strip $(MUX_TYPE)), USE_74HC4051)
	    OPT_DEFS += -DUSE_74HC4051
    endif

    ifeq ($(strip $(MUX_TYPE)), USE_74HC4067)
	    OPT_DEFS += -DUSE_74HC4067
    endif
    VPATH += ${KB_COMMON_DIR}/ec_lib/
    SRC += kb_common/ec_lib/mux_74hc40xx.c
    # custom matrix setup
    CUSTOM_MATRIX = lite	
    SRC += kb_common/ec_lib/ec_matrix.c
endif

# 蓝牙
ifeq ($(strip $(BLUETOOTH_DRIVER)), bhq)

    # 低功耗 start
    ifeq ($(strip $(KB_LPM_ENABLED)), yes)
        OPT_DEFS += -DKB_LPM_ENABLED
        OPT_DEFS += -DKB_LPM_DRIVER

        # ---- 芯片系列选择（根据驱动名自动解析） ----
        ifneq (,$(findstring stm32f4,$(KB_LPM_DRIVER)))
            OPT_DEFS += -DLPM_CHIP_STM32F4
        endif
        ifneq (,$(findstring stm32f1,$(KB_LPM_DRIVER)))
            OPT_DEFS += -DLPM_CHIP_STM32F1
        endif
        ifneq (,$(findstring at32,$(KB_LPM_DRIVER)))
            OPT_DEFS += -DLPM_CHIP_AT32
        endif

        # ---- 功能选择（根据驱动名自动解析） ----
        # RTC 周期性唤醒（休眠中定时轮询按键）
        ifneq (,$(findstring rtc,$(KB_LPM_DRIVER)))
            OPT_DEFS += -DLPM_RTC_WAKEUP
        endif
        # EC 静电容矩阵（使用 ADC 扫描，需要 lpm_hal_init）
        ifneq (,$(findstring _ec,$(KB_LPM_DRIVER)))
            OPT_DEFS += -DLPM_EC_MATRIX
        endif

        # 矩阵唤醒方式
        VPATH += ${KB_COMMON_DIR}/matrix/matrix_type
        VPATH += ${KB_COMMON_DIR}/matrix/matrix_sleep
        ifeq ($(strip $(MATRIX_TYPE)), default)
            ifneq (,$(findstring at32,$(KB_LPM_DRIVER)))
                SRC+= kb_common/matrix/matrix_sleep/matrix_sleep_${MATRIX_TYPE}_at32.c
            else
                SRC+= kb_common/matrix/matrix_sleep/matrix_sleep_${MATRIX_TYPE}_stm32.c
            endif
        else
            SRC+= kb_common/matrix/matrix_sleep/matrix_sleep_${MATRIX_TYPE}.c
        endif

        # 统一 LPM 核心实现（替代原先分散的 lpm_stm32f4.c / lpm_stm32f1.c / ...）
        SRC += kb_common/lpm_core.c
    endif
    # 低功耗 end

    # 电池
    ifeq ($(strip $(KB_CHECK_BATTERY_ENABLED)), yes)
        OPT_DEFS += -DKB_CHECK_BATTERY_ENABLED
        ANALOG_DRIVER_REQUIRED = yes
        OPT_DEFS += -DHAL_USE_ADC=TRUE
        SRC += kb_common/battery.c
    endif

    SRC += kb_common/bhq_common.c
    SRC += kb_common/transport.c
    SRC += kb_common/wireless.c

endif


# rgb_matrix 闪烁功能
ifeq ($(strip $(RGB_MATRIX_CUSTOM_BLINK_EFFECT)), yes)
ifeq ($(strip $(RGB_MATRIX_ENABLE)), no)
  $(error RGB_MATRIX_CUSTOM_BLINK_EFFECT depends on the RGB_MATRIX_ENABLE)
endif
    OPT_DEFS += -DRGB_MATRIX_CUSTOM_BLINK_EFFECT
    VPATH += ${KB_COMMON_DIR}/rgb_matrix_custom_effect/rgb_matrix_blink_effect
    SRC += kb_common/rgb_matrix_custom_effect/rgb_matrix_blink_effect/rgb_matrix_blink_effect.c
    
    # 通过keycode获取rgb index的
    VPATH += ${KB_COMMON_DIR}/rgb_matrix_custom_effect/rgb_matrix_index_by_wireless_keycode
    SRC += kb_common/rgb_matrix_custom_effect/rgb_matrix_index_by_wireless_keycode/rgb_matrix_index_by_wireless_keycode.c
    
endif
# rgb_matrix 电量百分比显示功能
ifeq ($(strip $(RGB_MATRIX_CUSTOM_BATTERY_EFFECT)), yes)
ifeq ($(strip $(RGB_MATRIX_ENABLE)), no)
  $(error RGB_MATRIX_CUSTOM_BATTERY_EFFECT depends on the RGB_MATRIX_ENABLE)
endif
ifeq ($(strip $(KB_CHECK_BATTERY_ENABLED)), no)
  $(error RGB_MATRIX_CUSTOM_BATTERY_EFFECT depends on the KB_CHECK_BATTERY_ENABLED)
endif
    OPT_DEFS += -DRGB_MATRIX_CUSTOM_BATTERY_EFFECT
    VPATH += ${KB_COMMON_DIR}/rgb_matrix_custom_effect/rgb_matrix_battery_effect
    SRC += kb_common/rgb_matrix_custom_effect/rgb_matrix_battery_effect/rgb_matrix_battery_effect.c
endif


