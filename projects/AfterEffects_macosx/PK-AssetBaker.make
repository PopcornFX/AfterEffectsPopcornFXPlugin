# Alternative GNU Make project makefile autogenerated by Premake

ifndef config
  config=debug_x64
endif

ifndef verbose
  SILENT = @
endif

.PHONY: clean prebuild

SHELLTYPE := posix
ifeq (.exe,$(findstring .exe,$(ComSpec)))
	SHELLTYPE := msdos
endif

# Configurations
# #############################################

ifeq ($(origin CC), default)
  CC = clang
endif
ifeq ($(origin CXX), default)
  CXX = clang++
endif
ifeq ($(origin AR), default)
  AR = ar
endif
PCH = ../../ExternalLibs/PK-AssetBaker/precompiled.h
PCH_PLACEHOLDER = $(OBJDIR)/$(notdir $(PCH))
GCH = $(PCH_PLACEHOLDER).gch
INCLUDES += -I../../ExternalLibs/Runtime -I../../ExternalLibs/Runtime/include -I../../ExternalLibs/Runtime/include/license/AfterEffects -I../../ExternalLibs/PK-AssetBaker -I../../ExternalLibs/PK-AssetBakerLib -I../../ExternalLibs/pk_upgraderlib/include -I../../SDK/Samples/Common/PKFX -I../../ExternalLibs/Runtime/libs/zlib-1.2.8
FORCE_INCLUDE +=
ALL_CPPFLAGS += $(CPPFLAGS) -MD -MP $(DEFINES) $(INCLUDES)
ALL_RESFLAGS += $(RESFLAGS) $(DEFINES) $(INCLUDES)
ALL_LDFLAGS += $(LDFLAGS) -L../../ExternalLibs/Runtime/libs/freetype-2.5.5/lib/macosx_x64 -L../../ExternalLibs/CodecMesh_FBX/libs/macosx/legacy_clang -L../../ExternalLibs/Runtime/bin/AfterEffects/gmake_macosx_x64 -m64 -target x86_64-apple-macos10.14 -liconv
LINKCMD = $(CXX) -o "$@" $(OBJECTS) $(RESOURCES) $(ALL_LDFLAGS) $(LIBS)
define PREBUILDCMDS
endef
define PRELINKCMDS
endef
define POSTBUILDCMDS
endef

ifeq ($(config),debug_x64)
TARGETDIR = ../../../release/application/BinariesGM_macosx_x64_d
TARGET = $(TARGETDIR)/PK-AssetBaker
OBJDIR = ../intermediate/AfterEffects/GM/x64/Debug/PK-AssetBaker
DEFINES += -D_DEBUG -DPK_COMPILER_BUILD_COMPILER_D3D11=1 -DPK_COMPILER_BUILD_COMPILER_D3D12=1 -DZ_PREFIX -DZ_PREFIX_CUSTOM=pk_z_ -DPK_USE_RENDER_HELPERS=0 -DUSE_FBXIMPORTER -DUSE_IMAGE_PLUGIN_HDR -DUSE_IMAGE_PLUGIN_TIFF -DUSE_IMAGE_PLUGIN_PKM -DUSE_IMAGE_PLUGIN_PVR -DUSE_COMPILER_BACKEND_D3D -DMACOSX
ALL_CFLAGS += $(CFLAGS) $(ALL_CPPFLAGS) -m64 -Wshadow -Wundef -fno-omit-frame-pointer -fno-strict-aliasing -g -msse2 -Wall -Wextra -Winvalid-pch -Wno-pragma-pack -fno-math-errno -fno-trapping-math -ggdb -mfpmath=sse -target x86_64-apple-macos10.14 -iwithsysroot `xcrun --show-sdk-path`
ALL_CXXFLAGS += $(CXXFLAGS) $(ALL_CPPFLAGS) -m64 -Wshadow -Wundef -fno-omit-frame-pointer -fno-strict-aliasing -g -msse2 -Wall -Wextra -std=gnu++0x -fno-exceptions -fvisibility-inlines-hidden -fno-rtti -fvisibility=hidden -Winvalid-pch -Wno-pragma-pack -fno-math-errno -fno-trapping-math -ggdb -mfpmath=sse -target x86_64-apple-macos10.14 -iwithsysroot `xcrun --show-sdk-path`
LIBS += ../../ExternalLibs/Runtime/bin/AfterEffects/gmake_macosx_x64/libPK-AssetBakerLib_d.a ../../ExternalLibs/Runtime/bin/AfterEffects/gmake_macosx_x64/libPK-UpgraderLib_d.a -framework Cocoa -lPK-Plugin_CodecImage_PKIM_d -lPK-Plugin_CodecImage_DDS_d -lPK-Plugin_CodecImage_PNG_d -lPK-Plugin_CodecImage_JPG_d -lPK-Plugin_CodecImage_TGA_d -lPK-Plugin_CodecImage_PKM_d -lPK-Plugin_CodecImage_PVR_d -lPK-Plugin_CodecImage_TIFF_d -lPK-Plugin_CodecImage_HDR_d -lPK-Plugin_CompilerBackend_CPU_VM_d -lPK-ZLib_d -lPK-Plugin_CodecMesh_FBX_d -lPK-Plugin_CompilerBackend_GPU_D3D_d -lPK-Plugin_CodecImage_EXR_d -lfreetype -lfbxsdk_d -lxml2 -lz -lPK-ParticlesToolbox_d -lPK-Runtime_d
LDDEPS += ../../ExternalLibs/Runtime/bin/AfterEffects/gmake_macosx_x64/libPK-AssetBakerLib_d.a ../../ExternalLibs/Runtime/bin/AfterEffects/gmake_macosx_x64/libPK-UpgraderLib_d.a

else ifeq ($(config),release_x64)
TARGETDIR = ../../../release/application/BinariesGM_macosx_x64_r
TARGET = $(TARGETDIR)/PK-AssetBaker
OBJDIR = ../intermediate/AfterEffects/GM/x64/Release/PK-AssetBaker
DEFINES += -DNDEBUG -DPK_COMPILER_BUILD_COMPILER_D3D11=1 -DPK_COMPILER_BUILD_COMPILER_D3D12=1 -DZ_PREFIX -DZ_PREFIX_CUSTOM=pk_z_ -DPK_USE_RENDER_HELPERS=0 -DUSE_FBXIMPORTER -DUSE_IMAGE_PLUGIN_HDR -DUSE_IMAGE_PLUGIN_TIFF -DUSE_IMAGE_PLUGIN_PKM -DUSE_IMAGE_PLUGIN_PVR -DUSE_COMPILER_BACKEND_D3D -DMACOSX
ALL_CFLAGS += $(CFLAGS) $(ALL_CPPFLAGS) -m64 -Wshadow -Wundef -fno-omit-frame-pointer -O3 -fno-strict-aliasing -g -msse2 -Wall -Wextra -Winvalid-pch -Wno-pragma-pack -fno-math-errno -fno-trapping-math -mfpmath=sse -target x86_64-apple-macos10.14 -iwithsysroot `xcrun --show-sdk-path`
ALL_CXXFLAGS += $(CXXFLAGS) $(ALL_CPPFLAGS) -m64 -Wshadow -Wundef -fno-omit-frame-pointer -O3 -fno-strict-aliasing -g -msse2 -Wall -Wextra -std=gnu++0x -fno-exceptions -fvisibility-inlines-hidden -fno-rtti -fvisibility=hidden -Winvalid-pch -Wno-pragma-pack -fno-math-errno -fno-trapping-math -mfpmath=sse -target x86_64-apple-macos10.14 -iwithsysroot `xcrun --show-sdk-path`
LIBS += ../../ExternalLibs/Runtime/bin/AfterEffects/gmake_macosx_x64/libPK-AssetBakerLib_r.a ../../ExternalLibs/Runtime/bin/AfterEffects/gmake_macosx_x64/libPK-UpgraderLib_r.a -framework Cocoa -lPK-Plugin_CodecImage_PKIM_r -lPK-Plugin_CodecImage_DDS_r -lPK-Plugin_CodecImage_PNG_r -lPK-Plugin_CodecImage_JPG_r -lPK-Plugin_CodecImage_TGA_r -lPK-Plugin_CodecImage_PKM_r -lPK-Plugin_CodecImage_PVR_r -lPK-Plugin_CodecImage_TIFF_r -lPK-Plugin_CodecImage_HDR_r -lPK-Plugin_CompilerBackend_CPU_VM_r -lPK-ZLib_r -lPK-Plugin_CodecMesh_FBX_r -lPK-Plugin_CompilerBackend_GPU_D3D_r -lPK-Plugin_CodecImage_EXR_r -lfreetype -lfbxsdk_r -lxml2 -lz -lPK-ParticlesToolbox_r -lPK-Runtime_r
LDDEPS += ../../ExternalLibs/Runtime/bin/AfterEffects/gmake_macosx_x64/libPK-AssetBakerLib_r.a ../../ExternalLibs/Runtime/bin/AfterEffects/gmake_macosx_x64/libPK-UpgraderLib_r.a

#else
#  $(error "invalid configuration $(config)")
endif

# Per File Configurations
# #############################################


# File sets
# #############################################

GENERATED :=
OBJECTS :=

GENERATED += $(OBJDIR)/AssetBaker.o
GENERATED += $(OBJDIR)/AssetBaker_EditorMaterialProxy.o
GENERATED += $(OBJDIR)/AssetBaker_SimulationInterfaces.o
GENERATED += $(OBJDIR)/AssetBaker_Unzip.o
GENERATED += $(OBJDIR)/AssetBaker_Upgrade.o
GENERATED += $(OBJDIR)/FxPlugins.o
GENERATED += $(OBJDIR)/FxStartup.o
GENERATED += $(OBJDIR)/precompiled.o
GENERATED += $(OBJDIR)/unzip.o
OBJECTS += $(OBJDIR)/AssetBaker.o
OBJECTS += $(OBJDIR)/AssetBaker_EditorMaterialProxy.o
OBJECTS += $(OBJDIR)/AssetBaker_SimulationInterfaces.o
OBJECTS += $(OBJDIR)/AssetBaker_Unzip.o
OBJECTS += $(OBJDIR)/AssetBaker_Upgrade.o
OBJECTS += $(OBJDIR)/FxPlugins.o
OBJECTS += $(OBJDIR)/FxStartup.o
OBJECTS += $(OBJDIR)/precompiled.o
OBJECTS += $(OBJDIR)/unzip.o

# Rules
# #############################################

all: $(TARGET)
	@:

$(TARGET): $(GENERATED) $(OBJECTS) $(LDDEPS) | $(TARGETDIR)
	$(PRELINKCMDS)
	@echo Linking PK-AssetBaker
	$(SILENT) $(LINKCMD)
	$(POSTBUILDCMDS)

$(TARGETDIR):
	@echo Creating $(TARGETDIR)
ifeq (posix,$(SHELLTYPE))
	$(SILENT) mkdir -p $(TARGETDIR)
else
	$(SILENT) mkdir $(subst /,\\,$(TARGETDIR))
endif

$(OBJDIR):
	@echo Creating $(OBJDIR)
ifeq (posix,$(SHELLTYPE))
	$(SILENT) mkdir -p $(OBJDIR)
else
	$(SILENT) mkdir $(subst /,\\,$(OBJDIR))
endif

clean:
	@echo Cleaning PK-AssetBaker
ifeq (posix,$(SHELLTYPE))
	$(SILENT) rm -f  $(TARGET)
	$(SILENT) rm -rf $(GENERATED)
	$(SILENT) rm -rf $(OBJDIR)
else
	$(SILENT) if exist $(subst /,\\,$(TARGET)) del $(subst /,\\,$(TARGET))
	$(SILENT) if exist $(subst /,\\,$(GENERATED)) del /s /q $(subst /,\\,$(GENERATED))
	$(SILENT) if exist $(subst /,\\,$(OBJDIR)) rmdir /s /q $(subst /,\\,$(OBJDIR))
endif

prebuild: | $(OBJDIR)
	$(PREBUILDCMDS)

ifneq (,$(PCH))
$(OBJECTS): $(GCH) | $(PCH_PLACEHOLDER)
$(GCH): $(PCH) | prebuild
	@echo $(notdir $<)
	$(SILENT) $(CXX) -x c++-header $(ALL_CXXFLAGS) -o "$@" -MF "$(@:%.gch=%.d)" -c "$<"
$(PCH_PLACEHOLDER): $(GCH) | $(OBJDIR)
ifeq (posix,$(SHELLTYPE))
	$(SILENT) touch "$@"
else
	$(SILENT) echo $null >> "$@"
endif
else
$(OBJECTS): | prebuild
endif


# File Rules
# #############################################

$(OBJDIR)/AssetBaker.o: ../../ExternalLibs/PK-AssetBaker/AssetBaker.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/AssetBaker_EditorMaterialProxy.o: ../../ExternalLibs/PK-AssetBaker/AssetBaker_EditorMaterialProxy.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/AssetBaker_SimulationInterfaces.o: ../../ExternalLibs/PK-AssetBaker/AssetBaker_SimulationInterfaces.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/AssetBaker_Unzip.o: ../../ExternalLibs/PK-AssetBaker/AssetBaker_Unzip.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/AssetBaker_Upgrade.o: ../../ExternalLibs/PK-AssetBaker/AssetBaker_Upgrade.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/FxPlugins.o: ../../SDK/Samples/Common/PKFX/FxPlugins.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/FxStartup.o: ../../SDK/Samples/Common/PKFX/FxStartup.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/precompiled.o: ../../ExternalLibs/PK-AssetBaker/precompiled.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/unzip.o: ../../ExternalLibs/PK-AssetBaker/unzip.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"

-include $(OBJECTS:%.o=%.d)
ifneq (,$(PCH))
  -include $(PCH_PLACEHOLDER).d
endif
