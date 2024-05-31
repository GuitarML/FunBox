# Project Name
TARGET = pluto

USE_DAISYSP_LGPL = 1

#APP_TYPE = BOOT_SRAM

# Compiler options
OPT = -O1
#OPT=-Os


# Sources
CPP_SOURCES = pluto.cpp reverbsc96.cpp

# Library Locations
LIBDAISY_DIR = ../../libDaisy
DAISYSP_DIR = ../../DaisySP

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile


# Include funbox.h
C_INCLUDES += -I../../include