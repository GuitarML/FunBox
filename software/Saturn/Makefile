# Project Name
TARGET = saturn

# Sources
CPP_SOURCES = saturn.cpp

#OPT = -O1
OPT = -Ofast
#OPT ?= -O3
#OPT = -Os

# Library Locations
LIBDAISY_DIR = ../../libDaisy
DAISYSP_DIR = ../../DaisySP


# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile


# Include funbox.h
C_INCLUDES += -I../../include