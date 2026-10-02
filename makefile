NAME = PHASEBND
DESCRIPTION = "Phasebound - five modes, campaign and editor"
COMPRESSED = YES
CFLAGS = -Wall -Wextra -O2 -std=c11
CXXFLAGS = -Wall -Wextra -O2
include $(shell cedev-config --makefile)
