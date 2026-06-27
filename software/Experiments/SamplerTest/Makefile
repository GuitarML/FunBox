# Project Name
TARGET = sampler

USE_DAISYSP_LGPL = 1

# APP_TYPE = BOOT_SRAM

# Compiler options
#OPT = -O1
OPT=-O3


# Sources
CPP_SOURCES = sampler.cpp

# Library Locations
LIBDAISY_DIR = ../../../libDaisy
DAISYSP_DIR = ../../../DaisySP

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile


# Include funbox.h
C_INCLUDES += -I../../../include