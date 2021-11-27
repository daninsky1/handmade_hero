.SILENT:
.IGNORE:

# Source
SRC_DIR=.\src
PROGRAM_SRC=$(SRC_DIR)\win32_main.cpp
HANDMADE_LIB_SRC=$(SRC_DIR)\handmade_hero.cpp

# Objects
OBJ=

# Headers
HDR=

# Defines
DDEFINES=/DHANDMADE_DEVELOPER_BUILD=1 /DHANDMADE_DEVELOPER_PERFORMANCE=0
RDEFINES=/DHANDMADE_DEVELOPER_BUILD=0 /DHANDMADE_DEVELOPER_PERFORMANCE=1

# Compiler Debug Flags
DCFLAGS=/EHa /MDd /Zi /Od /FC /MDd /MP /diagnostics:column /nologo

# Compiler Release Flags
RCFLAGS=

# c4189 initialized but not referenced warning
WINIT=/wd4189
WARNINGS=/W4 /wd4201 $(WINIT)

BUILD_DIR=build
DEBUG_DIR=debug
RELEASE_DIR=release

DEBUG_TARGET_PATH=$(BUILD_DIR)\$(DEBUG_DIR)
RELEASE_TARGET_PATH=$(BUILD_DIR)\$(RELEASE_DIR)

SYS_LIB=User32.lib gdi32.lib winmm.lib

HANDMADE_LIB=handmade_hero.dll

HANDMADE_LIB_DEPENDECIES=
LIB_SRC_DIR=
LIB_SRC=

PROGRAM=win32_handmade_hero.exe
PROGRAM_DEPENDECIES=$(SYS_LIB) $(HANDMADE_LIB)

DEBUG_PROGRAM=d$(PROGRAM)


all: $(DEBUG_PROGRAM)

$(DEBUG_PROGRAM): $(HANDMADE_LIB)
	if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)
	if not exist $(BUILD_DIR)\$(DEBUG_DIR) mkdir $(BUILD_DIR)\$(DEBUG_DIR)
	$(CC) $(PROGRAM_SRC) $(DCFLAGS) $(WARNINGS) $(INCLUDE_DIR) $(SYS_LIB) $(DDEFINES) /Fo$(DEBUG_TARGET_PATH)\ /Fe$(DEBUG_TARGET_PATH)\$(PROGRAM)

$(HANDMADE_LIB): $(HANDMADE_LIB_SRC)
	if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)
	if not exist $(BUILD_DIR)\$(DEBUG_DIR) mkdir $(BUILD_DIR)\$(DEBUG_DIR)
	del /q /s $(DEBUG_TARGET_PATH)\handmade_hero_*.pdb
	$(CC) $(HANDMADE_LIB_SRC) $(DCFLAGS) $(WARNINGS) $(INCLUDE_DIR) $(HANDMADE_LIB_DEPENDECIES) $(DDEFINES) /Fo$(DEBUG_TARGET_PATH)\ /Fe$(DEBUG_TARGET_PATH)\$(HANDMADE_LIB) /link /INCREMENTAL:NO /DEBUG /PDB:$(DEBUG_TARGET_PATH)\handmade_hero_%%random%%.pdb /DLL /EXPORT:game_update_and_render /EXPORT:game_get_sound_samples

clean:
	del build /s /q

tags: $(SRC) $(LIB_SRC)
	ctags -R --kinds-C=+l --c++-kinds=+p --fields=+iaS --extras=+q --language-force=C++ --sort=true $(SRC_DIR) $(LIB_SRC_DIR)

test:
	echo $(DEBUG_TARGET_PATH)\*.pdb
