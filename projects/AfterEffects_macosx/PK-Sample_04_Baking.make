# Alternative GNU Make project makefile autogenerated by Premake

ifndef config
  config=debug_x64
endif

ifndef verbose
  SILENT = @
endif

.PHONY: clean prebuild

SHELLTYPE := posix
ifeq ($(shell echo "test"), "test")
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
RESCOMP = windres
PCH = ../../SDK/Samples/ParticleSample/src/precompiled.h
PCH_PLACEHOLDER = $(OBJDIR)/$(notdir $(PCH))
GCH = $(PCH_PLACEHOLDER).gch
INCLUDES += -I../../ExternalLibs/Runtime -I../../ExternalLibs/Runtime/include -I../../ExternalLibs/Runtime/include/license/AfterEffects -I../../SDK/Samples/ParticleSample/src -I../../SDK/Samples/ParticleSample/src/include -I../../ExternalLibs/PK-AssetBakerLib -I../../SDK/Samples/Common/PKFX
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
TARGETDIR = ../../../release/builds/x64_Samples
TARGET = $(TARGETDIR)/PK-Sample_04_Baking_macosx_d
OBJDIR = ../intermediate/AfterEffects/GM/x64/Debug/PK-Sample_04_Baking
DEFINES += -D_DEBUG -DPK_COMPILER_BUILD_COMPILER_D3D11=1 -DPK_COMPILER_BUILD_COMPILER_D3D12=1 -DPK_USE_RENDER_HELPERS=0 -DUSE_COMPILER_BACKEND_D3D -DMACOSX
ALL_CFLAGS += $(CFLAGS) $(ALL_CPPFLAGS) -m64 -Wshadow -Wundef -fvisibility-inlines-hidden -fno-omit-frame-pointer -fno-strict-aliasing -g -msse2 -fvisibility=hidden -Wall -Wextra -Winvalid-pch -Wno-pragma-pack -fno-math-errno -fno-trapping-math -ggdb -mfpmath=sse -target x86_64-apple-macos10.14 -iwithsysroot `xcrun --show-sdk-path`
ALL_CXXFLAGS += $(CXXFLAGS) $(ALL_CPPFLAGS) -m64 -Wshadow -Wundef -fvisibility-inlines-hidden -fno-omit-frame-pointer -fno-strict-aliasing -g -msse2 -fvisibility=hidden -Wall -Wextra -std=gnu++0x -fno-exceptions -fno-rtti -Winvalid-pch -Wno-pragma-pack -fno-math-errno -fno-trapping-math -ggdb -mfpmath=sse -target x86_64-apple-macos10.14 -iwithsysroot `xcrun --show-sdk-path`
LIBS += ../../ExternalLibs/Runtime/bin/AfterEffects/gmake_macosx_x64/libPK-AssetBakerLib_d.a -framework Cocoa -lPK-Plugin_CodecImage_PKIM_d -lPK-Plugin_CodecImage_DDS_d -lPK-Plugin_CodecImage_PNG_d -lPK-Plugin_CodecImage_JPG_d -lPK-Plugin_CodecImage_TGA_d -lPK-Plugin_CodecImage_PKM_d -lPK-Plugin_CodecImage_PVR_d -lPK-Plugin_CodecImage_TIFF_d -lPK-Plugin_CodecImage_HDR_d -lPK-Plugin_CompilerBackend_CPU_VM_d -lPK-ZLib_d -lPK-Plugin_CodecMesh_FBX_d -lPK-Plugin_CompilerBackend_GPU_D3D_d -lPK-Plugin_CodecImage_EXR_d -lfreetype -lfbxsdk_d -lxml2 -lz -lPK-ParticlesToolbox_d -lPK-Runtime_d
LDDEPS += ../../ExternalLibs/Runtime/bin/AfterEffects/gmake_macosx_x64/libPK-AssetBakerLib_d.a

else ifeq ($(config),release_x64)
TARGETDIR = ../../../release/builds/x64_Samples
TARGET = $(TARGETDIR)/PK-Sample_04_Baking_macosx_r
OBJDIR = ../intermediate/AfterEffects/GM/x64/Release/PK-Sample_04_Baking
DEFINES += -DNDEBUG -DPK_COMPILER_BUILD_COMPILER_D3D11=1 -DPK_COMPILER_BUILD_COMPILER_D3D12=1 -DPK_USE_RENDER_HELPERS=0 -DUSE_COMPILER_BACKEND_D3D -DMACOSX
ALL_CFLAGS += $(CFLAGS) $(ALL_CPPFLAGS) -m64 -Wshadow -Wundef -fvisibility-inlines-hidden -fno-omit-frame-pointer -O3 -fno-strict-aliasing -g -msse2 -fvisibility=hidden -Wall -Wextra -Winvalid-pch -Wno-pragma-pack -fno-math-errno -fno-trapping-math -mfpmath=sse -target x86_64-apple-macos10.14 -iwithsysroot `xcrun --show-sdk-path`
ALL_CXXFLAGS += $(CXXFLAGS) $(ALL_CPPFLAGS) -m64 -Wshadow -Wundef -fvisibility-inlines-hidden -fno-omit-frame-pointer -O3 -fno-strict-aliasing -g -msse2 -fvisibility=hidden -Wall -Wextra -std=gnu++0x -fno-exceptions -fno-rtti -Winvalid-pch -Wno-pragma-pack -fno-math-errno -fno-trapping-math -mfpmath=sse -target x86_64-apple-macos10.14 -iwithsysroot `xcrun --show-sdk-path`
LIBS += ../../ExternalLibs/Runtime/bin/AfterEffects/gmake_macosx_x64/libPK-AssetBakerLib_r.a -framework Cocoa -lPK-Plugin_CodecImage_PKIM_r -lPK-Plugin_CodecImage_DDS_r -lPK-Plugin_CodecImage_PNG_r -lPK-Plugin_CodecImage_JPG_r -lPK-Plugin_CodecImage_TGA_r -lPK-Plugin_CodecImage_PKM_r -lPK-Plugin_CodecImage_PVR_r -lPK-Plugin_CodecImage_TIFF_r -lPK-Plugin_CodecImage_HDR_r -lPK-Plugin_CompilerBackend_CPU_VM_r -lPK-ZLib_r -lPK-Plugin_CodecMesh_FBX_r -lPK-Plugin_CompilerBackend_GPU_D3D_r -lPK-Plugin_CodecImage_EXR_r -lfreetype -lfbxsdk_r -lxml2 -lz -lPK-ParticlesToolbox_r -lPK-Runtime_r
LDDEPS += ../../ExternalLibs/Runtime/bin/AfterEffects/gmake_macosx_x64/libPK-AssetBakerLib_r.a

else ifeq ($(config),retail_x64)
TARGETDIR = ../../../release/builds/x64_Samples
TARGET = $(TARGETDIR)/PK-Sample_04_Baking_macosx_s
OBJDIR = ../intermediate/AfterEffects/GM/x64/Retail/PK-Sample_04_Baking
DEFINES += -DNDEBUG -DPK_RETAIL -DPK_COMPILER_BUILD_COMPILER_D3D11=1 -DPK_COMPILER_BUILD_COMPILER_D3D12=1 -DPK_USE_RENDER_HELPERS=0 -DUSE_COMPILER_BACKEND_D3D -DMACOSX
ALL_CFLAGS += $(CFLAGS) $(ALL_CPPFLAGS) -m64 -Wshadow -Wundef -fvisibility-inlines-hidden -fomit-frame-pointer -O3 -fno-strict-aliasing -msse2 -fvisibility=hidden -Wall -Wextra -Winvalid-pch -Wno-pragma-pack -fno-math-errno -fno-trapping-math -mfpmath=sse -target x86_64-apple-macos10.14 -iwithsysroot `xcrun --show-sdk-path`
ALL_CXXFLAGS += $(CXXFLAGS) $(ALL_CPPFLAGS) -m64 -Wshadow -Wundef -fvisibility-inlines-hidden -fomit-frame-pointer -O3 -fno-strict-aliasing -msse2 -fvisibility=hidden -Wall -Wextra -std=gnu++0x -fno-exceptions -fno-rtti -Winvalid-pch -Wno-pragma-pack -fno-math-errno -fno-trapping-math -mfpmath=sse -target x86_64-apple-macos10.14 -iwithsysroot `xcrun --show-sdk-path`
LIBS += ../../ExternalLibs/Runtime/bin/AfterEffects/gmake_macosx_x64/libPK-AssetBakerLib_s.a -framework Cocoa -lPK-Plugin_CodecImage_PKIM_s -lPK-Plugin_CodecImage_DDS_s -lPK-Plugin_CodecImage_PNG_s -lPK-Plugin_CodecImage_JPG_s -lPK-Plugin_CodecImage_TGA_s -lPK-Plugin_CodecImage_PKM_s -lPK-Plugin_CodecImage_PVR_s -lPK-Plugin_CodecImage_TIFF_s -lPK-Plugin_CodecImage_HDR_s -lPK-Plugin_CompilerBackend_CPU_VM_s -lPK-ZLib_s -lPK-Plugin_CodecMesh_FBX_s -lPK-Plugin_CompilerBackend_GPU_D3D_s -lPK-Plugin_CodecImage_EXR_s -lfreetype -lfbxsdk_r -lxml2 -lz -lPK-ParticlesToolbox_s -lPK-Runtime_s
LDDEPS += ../../ExternalLibs/Runtime/bin/AfterEffects/gmake_macosx_x64/libPK-AssetBakerLib_s.a

#else
#  $(error "invalid configuration $(config)")
endif

# Per File Configurations
# #############################################


# File sets
# #############################################

GENERATED :=
OBJECTS :=

GENERATED += $(OBJDIR)/FxPlugins.o
GENERATED += $(OBJDIR)/FxStartup.o
GENERATED += $(OBJDIR)/ParticleSample_04_Baking.o
GENERATED += $(OBJDIR)/precompiled.o
OBJECTS += $(OBJDIR)/FxPlugins.o
OBJECTS += $(OBJDIR)/FxStartup.o
OBJECTS += $(OBJDIR)/ParticleSample_04_Baking.o
OBJECTS += $(OBJDIR)/precompiled.o

# Rules
# #############################################

all: $(TARGET)
	@:

$(TARGET): $(GENERATED) $(OBJECTS) $(LDDEPS) | $(TARGETDIR)
	$(PRELINKCMDS)
	@echo Linking PK-Sample_04_Baking
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
	@echo Cleaning PK-Sample_04_Baking
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

$(OBJDIR)/FxPlugins.o: ../../SDK/Samples/Common/PKFX/FxPlugins.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/FxStartup.o: ../../SDK/Samples/Common/PKFX/FxStartup.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/ParticleSample_04_Baking.o: ../../SDK/Samples/ParticleSample/src/ParticleSample_04_Baking.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/precompiled.o: ../../SDK/Samples/ParticleSample/src/precompiled.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"

-include $(OBJECTS:%.o=%.d)
ifneq (,$(PCH))
  -include $(PCH_PLACEHOLDER).d
endif
