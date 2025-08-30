
# 是否使能低功耗
KB_LPM_ENABLED = yes
KB_LPM_DRIVER = lpm_stm32f4
# 是否使能QMK端读取电池电压
KB_CHECK_BATTERY_ENABLED = yes
# 开启键盘层DEBUG  这里是用RTT输出日记的
KM_DEBUG = no


include keyboards/keymagichorse/km_common/km_common.mk

VIA_ENABLE = yes
