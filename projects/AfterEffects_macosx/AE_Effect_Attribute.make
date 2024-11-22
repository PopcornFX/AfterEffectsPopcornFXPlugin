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
INCLUDES += -I../../ExternalLibs/Runtime -I../../ExternalLibs/Runtime/include -I../../ExternalLibs/Runtime/include/license/AfterEffects -I../../ExternalLibs -I../../AE_Effect_Attribute/Sources -I../../AE_Effect_Attribute/Include -I../../AE_Effect_Attribute/Precompiled -I../../AE_Suites -I"../../External/AE SDK/Resources" -I"../../External/AE SDK/Headers" -I"../../External/AE SDK/Util" -I"../../External/AE SDK/Headers/SP" -I"../../External/AE SDK/Headers/adobesdk" -I"../../External/AE SDK/Headers/SP/artemis" -I"../../External/AE SDK/Headers/SP/photoshop" -I"../../External/AE SDK/Headers/SP/artemis/config" -I"../../External/AE SDK/Headers/SP/photoshop/config" -I"../../External/AE SDK/Headers/adobesdk/config" -I"../../External/AE SDK/Headers/adobesdk/drawbotsuite"
FORCE_INCLUDE +=
ALL_CPPFLAGS += $(CPPFLAGS) -MD -MP $(DEFINES) $(INCLUDES)
ALL_RESFLAGS += $(RESFLAGS) $(DEFINES) $(INCLUDES)
LIBS += -framework quartzcore -framework cocoa -framework metal -liconv -lm -lpthread -ldl
LDDEPS +=
LINKCMD = $(CXX) -o "$@" $(OBJECTS) $(RESOURCES) $(ALL_LDFLAGS) $(LIBS)
define PREBUILDCMDS
endef
define PRELINKCMDS
endef
define POSTBUILDCMDS
endef

ifeq ($(config),debug_x64)
TARGETDIR = ../../ExternalLibs/Runtime/bin/AfterEffects/gmake_x64
TARGET = $(TARGETDIR)/libAE_Effect_Attribute_d
OBJDIR = ../intermediate/AfterEffects/GM/x64/Debug/AE_Effect_Attribute
DEFINES += -D_DEBUG -DPK_COMPILER_BUILD_COMPILER_D3D11=1 -DPK_COMPILER_BUILD_COMPILER_D3D12=1 -DUSE_POSIX_API=1 -D__MWERKS__=0 -DA_INTERNAL_TEST_ONE=0 -DWEB_ENV=0 -DPK_BUILD_WITH_FMODEX_SUPPORT=0 -DPK_BUILD_WITH_SDL=0
ALL_CFLAGS += $(CFLAGS) $(ALL_CPPFLAGS) -m64 -Wshadow -Wundef -fvisibility-inlines-hidden -fPIC -fno-strict-aliasing -msse2 -fvisibility=hidden -Wall -Wextra -Winvalid-pch -Wno-pragma-pack -fno-math-errno -fno-trapping-math -ggdb -mfpmath=sse -target x86_64-apple-macos10.14 -iwithsysroot `xcrun --show-sdk-path`
ALL_CXXFLAGS += $(CXXFLAGS) $(ALL_CPPFLAGS) -m64 -Wshadow -Wundef -fvisibility-inlines-hidden -fPIC -fno-strict-aliasing -msse2 -fvisibility=hidden -Wall -Wextra -std=c++14 -fno-rtti -Winvalid-pch -Wno-pragma-pack -fno-math-errno -fno-trapping-math -ggdb -mfpmath=sse -target x86_64-apple-macos10.14 -iwithsysroot `xcrun --show-sdk-path`
ALL_LDFLAGS += $(LDFLAGS) -m64 -dynamiclib -Wl,-install_name,@rpath/libAE_Effect_Attribute_d -Wl,-undefined,error -target x86_64-apple-macos10.14 -framework OpenGL

else ifeq ($(config),release_x64)
TARGETDIR = ../../ExternalLibs/Runtime/bin/AfterEffects/gmake_x64
TARGET = $(TARGETDIR)/libAE_Effect_Attribute_r
OBJDIR = ../intermediate/AfterEffects/GM/x64/Release/AE_Effect_Attribute
DEFINES += -DNDEBUG -DPK_COMPILER_BUILD_COMPILER_D3D11=1 -DPK_COMPILER_BUILD_COMPILER_D3D12=1 -DUSE_POSIX_API=1 -D__MWERKS__=0 -DA_INTERNAL_TEST_ONE=0 -DWEB_ENV=0 -DPK_BUILD_WITH_FMODEX_SUPPORT=0 -DPK_BUILD_WITH_SDL=0
ALL_CFLAGS += $(CFLAGS) $(ALL_CPPFLAGS) -m64 -Wshadow -Wundef -fvisibility-inlines-hidden -O3 -fPIC -fno-strict-aliasing -msse2 -fvisibility=hidden -Wall -Wextra -Winvalid-pch -Wno-pragma-pack -fno-math-errno -fno-trapping-math -mfpmath=sse -target x86_64-apple-macos10.14 -iwithsysroot `xcrun --show-sdk-path`
ALL_CXXFLAGS += $(CXXFLAGS) $(ALL_CPPFLAGS) -m64 -Wshadow -Wundef -fvisibility-inlines-hidden -O3 -fPIC -fno-strict-aliasing -msse2 -fvisibility=hidden -Wall -Wextra -std=c++14 -fno-rtti -Winvalid-pch -Wno-pragma-pack -fno-math-errno -fno-trapping-math -mfpmath=sse -target x86_64-apple-macos10.14 -iwithsysroot `xcrun --show-sdk-path`
ALL_LDFLAGS += $(LDFLAGS) -m64 -dynamiclib -Wl,-install_name,@rpath/libAE_Effect_Attribute_r -Wl,-undefined,error -target x86_64-apple-macos10.14 -framework OpenGL

#else
#  $(error "invalid configuration $(config)")
endif

# Per File Configurations
# #############################################

PERFILE_FLAGS_0 = $(ALL_CXXFLAGS) -fdeclspec -x objective-c++

# File sets
# #############################################

GENERATED :=
OBJECTS :=

GENERATED += $(OBJDIR)/AEAttribute_Main.o
GENERATED += $(OBJDIR)/AEAttribute_ParamDefine.o
GENERATED += $(OBJDIR)/AEAttribute_PluginInterface.o
GENERATED += $(OBJDIR)/AEAttribute_SequenceData.o
GENERATED += $(OBJDIR)/AEFX_ArbParseHelper.o
GENERATED += $(OBJDIR)/AEFX_SuiteHelper.o
GENERATED += $(OBJDIR)/AEGP_SuiteHandler.o
GENERATED += $(OBJDIR)/AEGP_Utils.o
GENERATED += $(OBJDIR)/MissingSuiteError.o
GENERATED += $(OBJDIR)/Smart_Utils.o
GENERATED += $(OBJDIR)/ae_precompiled.o
OBJECTS += $(OBJDIR)/AEAttribute_Main.o
OBJECTS += $(OBJDIR)/AEAttribute_ParamDefine.o
OBJECTS += $(OBJDIR)/AEAttribute_PluginInterface.o
OBJECTS += $(OBJDIR)/AEAttribute_SequenceData.o
OBJECTS += $(OBJDIR)/AEFX_ArbParseHelper.o
OBJECTS += $(OBJDIR)/AEFX_SuiteHelper.o
OBJECTS += $(OBJDIR)/AEGP_SuiteHandler.o
OBJECTS += $(OBJDIR)/AEGP_Utils.o
OBJECTS += $(OBJDIR)/MissingSuiteError.o
OBJECTS += $(OBJDIR)/Smart_Utils.o
OBJECTS += $(OBJDIR)/ae_precompiled.o

# Rules
# #############################################

all: $(TARGET)
	@:

$(TARGET): $(GENERATED) $(OBJECTS) $(LDDEPS) | $(TARGETDIR)
	$(PRELINKCMDS)
	@echo Linking AE_Effect_Attribute
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
	@echo Cleaning AE_Effect_Attribute
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

$(OBJDIR)/AEFX_ArbParseHelper.o: ../../External/AE\ SDK/Util/AEFX_ArbParseHelper.c
	@echo "$(notdir $<)"
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/AEFX_SuiteHelper.o: ../../External/AE\ SDK/Util/AEFX_SuiteHelper.c
	@echo "$(notdir $<)"
	$(SILENT) $(CC) $(ALL_CFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/AEGP_SuiteHandler.o: ../../External/AE\ SDK/Util/AEGP_SuiteHandler.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) $(PERFILE_FLAGS_0) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/AEGP_Utils.o: ../../External/AE\ SDK/Util/AEGP_Utils.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) $(PERFILE_FLAGS_0) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/MissingSuiteError.o: ../../External/AE\ SDK/Util/MissingSuiteError.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) $(PERFILE_FLAGS_0) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/Smart_Utils.o: ../../External/AE\ SDK/Util/Smart_Utils.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) $(PERFILE_FLAGS_0) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/ae_precompiled.o: ../../AE_Effect_Attribute/Precompiled/ae_precompiled.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) $(PERFILE_FLAGS_0) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/AEAttribute_Main.o: ../../AE_Effect_Attribute/Sources/AEAttribute_Main.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) $(PERFILE_FLAGS_0) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/AEAttribute_ParamDefine.o: ../../AE_Effect_Attribute/Sources/AEAttribute_ParamDefine.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) $(PERFILE_FLAGS_0) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/AEAttribute_PluginInterface.o: ../../AE_Effect_Attribute/Sources/AEAttribute_PluginInterface.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) $(PERFILE_FLAGS_0) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/AEAttribute_SequenceData.o: ../../AE_Effect_Attribute/Sources/AEAttribute_SequenceData.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) $(PERFILE_FLAGS_0) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"

-include $(OBJECTS:%.o=%.d)
ifneq (,$(PCH))
  -include $(PCH_PLACEHOLDER).d
endif
