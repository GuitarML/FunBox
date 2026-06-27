# Project Name
TARGET = uranus
CPP_STANDARD = -std=gnu++20
USE_DAISYSP_LGPL = 1

# APP_TYPE = BOOT_SRAM

# Compiler options
#OPT = -O1
OPT=-Ofast


# Sources
CPP_SOURCES = uranus.cpp granularplayermod.cpp operator.cpp

# Library Locations
LIBDAISY_DIR = ../../libDaisy
DAISYSP_DIR = ../../DaisySP

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile


# Include funbox.h
C_INCLUDES += -I../../infra/include
C_INCLUDES += -I../../q/q_lib/include
C_INCLUDES += -I../../include