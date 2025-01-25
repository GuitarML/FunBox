# Project Name
TARGET = granular

USE_DAISYSP_LGPL = 1

# APP_TYPE = BOOT_SRAM

# Compiler options
#OPT = -O1
OPT=-Ofast


# Sources
CPP_SOURCES = granular.cpp

# Library Locations
LIBDAISY_DIR = ../../../libDaisy
DAISYSP_DIR = ../../../DaisySP

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile


# Include funbox.h
C_INCLUDES += -I../../../include