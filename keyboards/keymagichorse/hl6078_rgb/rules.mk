MAKEFLAGS += -j10

# custom matrix setup
CUSTOM_MATRIX = lite	
SRC  += matrix.c

VPATH += $(TOP_DIR)/keyboards/keymagichorse
