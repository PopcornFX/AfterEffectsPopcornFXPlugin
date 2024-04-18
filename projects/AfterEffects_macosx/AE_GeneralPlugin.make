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
INCLUDES += -I../../AE_GeneralPlugin/Sources -I../../AE_GeneralPlugin/Include -I../../AE_GeneralPlugin/Precompiled -I../../AE_Suites -I"../../External/AE SDK/Resources" -I"../../External/AE SDK/Headers" -I"../../External/AE SDK/Util" -I"../../External/AE SDK/Headers/SP" -I"../../External/AE SDK/Headers/adobesdk" -I"../../External/AE SDK/Headers/SP/artemis" -I"../../External/AE SDK/Headers/SP/photoshop" -I"../../External/AE SDK/Headers/SP/artemis/config" -I"../../External/AE SDK/Headers/SP/photoshop/config" -I"../../External/AE SDK/Headers/adobesdk/config" -I"../../External/AE SDK/Headers/adobesdk/drawbotsuite" -I../../ExternalLibs/Runtime -I../../ExternalLibs/Runtime/include -I../../ExternalLibs/Runtime/include/license/AfterEffects -I../../ExternalLibs -I../../Samples -I../../ExternalLibs/GL/include -I"$(QTDIR)/include" -I"$(QTDIR)/lib/QtCore.framework/Headers" -I"$(QTDIR)/lib/QtWidgets.framework/Headers" -I"$(QTDIR)/lib/QtNetwork.framework/Headers" -I"$(QTDIR)/lib/QtXml.framework/Headers" -I"$(QTDIR)/lib/QtGui.framework/Headers"
FORCE_INCLUDE +=
ALL_CPPFLAGS += $(CPPFLAGS) -MD -MP $(DEFINES) $(INCLUDES)
ALL_RESFLAGS += $(RESFLAGS) $(DEFINES) $(INCLUDES)
LINKCMD = $(CXX) -o "$@" $(OBJECTS) $(RESOURCES) $(ALL_LDFLAGS) $(LIBS)
define PREBUILDCMDS
endef
define PRELINKCMDS
endef
define POSTBUILDCMDS
endef

ifeq ($(config),debug_x64)
TARGETDIR = ../../ExternalLibs/Runtime/bin/AfterEffects/gmake_x64
TARGET = $(TARGETDIR)/libAE_GeneralPlugin_macosx_d
OBJDIR = ../intermediate/AfterEffects/GM/x64/Debug/AE_GeneralPlugin
DEFINES += -D_DEBUG -DPK_COMPILER_BUILD_COMPILER_D3D11=1 -DPK_COMPILER_BUILD_COMPILER_D3D12=1 -DUSE_POSIX_API=1 -D__MWERKS__=0 -DA_INTERNAL_TEST_ONE=0 -DWEB_ENV=0 -DPK_BUILD_WITH_FMODEX_SUPPORT=0 -DPK_BUILD_WITH_SDL=0 -DPK_BUILD_WITH_D3D11_SUPPORT=0 -DPK_BUILD_WITH_D3D12_SUPPORT=0 -DQT_NO_KEYWORDS -DQT_NO_SIGNALS_SLOTS_KEYWORDS -DPK_BUILD_WITH_METAL_SUPPORT=1 -DPK_BUILD_WITH_OGL_SUPPORT=1 -DGL_GLEXT_PROTOTYPES -DGLEW_STATIC -DGLEW_NO_GLU -DMACOSX -DQT_CORE_LIB -DQT_WIDGETS_LIB -DQT_NETWORK_LIB -DQT_XML_LIB -DQT_GUI_LIB
ALL_CFLAGS += $(CFLAGS) $(ALL_CPPFLAGS) -m64 -Wshadow -Wundef -fno-omit-frame-pointer -fPIC -fno-strict-aliasing -g -msse2 -Wall -Wextra -Winvalid-pch -Wno-pragma-pack -fno-math-errno -fno-trapping-math -ggdb -mfpmath=sse -target x86_64-apple-macos10.14 -iwithsysroot `xcrun --show-sdk-path` -F$(QTDIR)/lib
ALL_CXXFLAGS += $(CXXFLAGS) $(ALL_CPPFLAGS) -m64 -Wshadow -Wundef -fno-omit-frame-pointer -fPIC -fno-strict-aliasing -g -msse2 -Wall -Wextra -std=c++14 -fvisibility-inlines-hidden -fno-rtti -fvisibility=hidden -Winvalid-pch -Wno-pragma-pack -fno-math-errno -fno-trapping-math -ggdb -mfpmath=sse -target x86_64-apple-macos10.14 -iwithsysroot `xcrun --show-sdk-path` -F$(QTDIR)/lib
LIBS += ../../ExternalLibs/Runtime/bin/AfterEffects/gmake_macosx_x64/libPK-AssetBakerLib_d.a ../../ExternalLibs/Runtime/bin/AfterEffects/gmake_macosx_x64/libPK-SampleLib_d.a -framework quartzcore -framework cocoa -framework metal -framework Cocoa -framework QtCore -framework QtWidgets -framework QtNetwork -framework QtXml -framework QtGui -liconv -lm -lpthread -ldl -lPK-RenderHelpers_d -lPK-RHI_d -lPK-Discretizers_d -lPK-MCPP_d -lPK-Plugin_CompilerBackend_CPU_VM_d -lPK-Plugin_CodecImage_DDS_d -lPK-Plugin_CodecImage_JPG_d -lPK-Plugin_CodecImage_PKM_d -lPK-Plugin_CodecImage_PNG_d -lPK-Plugin_CodecImage_PVR_d -lPK-Plugin_CodecImage_TGA_d -lPK-Plugin_CodecImage_TIFF_d -lPK-Plugin_CodecImage_HDR_d -lPK-Plugin_CodecImage_EXR_d -lPK-Plugin_CodecMesh_FBX_d -lPK-ZLib_d -lfbxsdk_d -lxml2 -lz -lPK-ParticlesToolbox_d -lPK-Runtime_d
LDDEPS += ../../ExternalLibs/Runtime/bin/AfterEffects/gmake_macosx_x64/libPK-AssetBakerLib_d.a ../../ExternalLibs/Runtime/bin/AfterEffects/gmake_macosx_x64/libPK-SampleLib_d.a
ALL_LDFLAGS += $(LDFLAGS) -L../../ExternalLibs/CodecMesh_FBX/libs/macosx/legacy_clang -L../../ExternalLibs/Runtime/bin/AfterEffects/gmake_macosx_x64 -L"$(QTDIR)/lib" -m64 -dynamiclib -Wl,-install_name,@rpath/libAE_GeneralPlugin_macosx_d -Wl,-undefined,error -target x86_64-apple-macos10.14 -framework OpenGL -F$(QTDIR)/lib -Wl,-rpath,$(QTDIR)/lib

else ifeq ($(config),release_x64)
TARGETDIR = ../../ExternalLibs/Runtime/bin/AfterEffects/gmake_x64
TARGET = $(TARGETDIR)/libAE_GeneralPlugin_macosx_r
OBJDIR = ../intermediate/AfterEffects/GM/x64/Release/AE_GeneralPlugin
DEFINES += -DNDEBUG -DPK_COMPILER_BUILD_COMPILER_D3D11=1 -DPK_COMPILER_BUILD_COMPILER_D3D12=1 -DUSE_POSIX_API=1 -D__MWERKS__=0 -DA_INTERNAL_TEST_ONE=0 -DWEB_ENV=0 -DPK_BUILD_WITH_FMODEX_SUPPORT=0 -DPK_BUILD_WITH_SDL=0 -DPK_BUILD_WITH_D3D11_SUPPORT=0 -DPK_BUILD_WITH_D3D12_SUPPORT=0 -DQT_NO_DEBUG -DQT_NO_KEYWORDS -DQT_NO_SIGNALS_SLOTS_KEYWORDS -DPK_BUILD_WITH_METAL_SUPPORT=1 -DPK_BUILD_WITH_OGL_SUPPORT=1 -DGL_GLEXT_PROTOTYPES -DGLEW_STATIC -DGLEW_NO_GLU -DMACOSX -DQT_CORE_LIB -DQT_WIDGETS_LIB -DQT_NETWORK_LIB -DQT_XML_LIB -DQT_GUI_LIB
ALL_CFLAGS += $(CFLAGS) $(ALL_CPPFLAGS) -m64 -Wshadow -Wundef -fno-omit-frame-pointer -O3 -fPIC -fno-strict-aliasing -g -msse2 -Wall -Wextra -Winvalid-pch -Wno-pragma-pack -fno-math-errno -fno-trapping-math -mfpmath=sse -target x86_64-apple-macos10.14 -iwithsysroot `xcrun --show-sdk-path` -F$(QTDIR)/lib
ALL_CXXFLAGS += $(CXXFLAGS) $(ALL_CPPFLAGS) -m64 -Wshadow -Wundef -fno-omit-frame-pointer -O3 -fPIC -fno-strict-aliasing -g -msse2 -Wall -Wextra -std=c++14 -fvisibility-inlines-hidden -fno-rtti -fvisibility=hidden -Winvalid-pch -Wno-pragma-pack -fno-math-errno -fno-trapping-math -mfpmath=sse -target x86_64-apple-macos10.14 -iwithsysroot `xcrun --show-sdk-path` -F$(QTDIR)/lib
LIBS += ../../ExternalLibs/Runtime/bin/AfterEffects/gmake_macosx_x64/libPK-AssetBakerLib_r.a ../../ExternalLibs/Runtime/bin/AfterEffects/gmake_macosx_x64/libPK-SampleLib_r.a -framework quartzcore -framework cocoa -framework metal -framework Cocoa -framework QtCore -framework QtWidgets -framework QtNetwork -framework QtXml -framework QtGui -liconv -lm -lpthread -ldl -lPK-RenderHelpers_r -lPK-RHI_r -lPK-Discretizers_r -lPK-MCPP_r -lPK-Plugin_CompilerBackend_CPU_VM_r -lPK-Plugin_CodecImage_DDS_r -lPK-Plugin_CodecImage_JPG_r -lPK-Plugin_CodecImage_PKM_r -lPK-Plugin_CodecImage_PNG_r -lPK-Plugin_CodecImage_PVR_r -lPK-Plugin_CodecImage_TGA_r -lPK-Plugin_CodecImage_TIFF_r -lPK-Plugin_CodecImage_HDR_r -lPK-Plugin_CodecImage_EXR_r -lPK-Plugin_CodecMesh_FBX_r -lPK-ZLib_r -lfbxsdk_r -lxml2 -lz -lPK-ParticlesToolbox_r -lPK-Runtime_r
LDDEPS += ../../ExternalLibs/Runtime/bin/AfterEffects/gmake_macosx_x64/libPK-AssetBakerLib_r.a ../../ExternalLibs/Runtime/bin/AfterEffects/gmake_macosx_x64/libPK-SampleLib_r.a
ALL_LDFLAGS += $(LDFLAGS) -L../../ExternalLibs/CodecMesh_FBX/libs/macosx/legacy_clang -L../../ExternalLibs/Runtime/bin/AfterEffects/gmake_macosx_x64 -L"$(QTDIR)/lib" -m64 -dynamiclib -Wl,-install_name,@rpath/libAE_GeneralPlugin_macosx_r -Wl,-undefined,error -target x86_64-apple-macos10.14 -framework OpenGL -F$(QTDIR)/lib -Wl,-rpath,$(QTDIR)/lib

#else
#  $(error "invalid configuration $(config)")
endif

# Per File Configurations
# #############################################

PERFILE_FLAGS_0 = $(ALL_CXXFLAGS) -fdeclspec -x objective-c++
PERFILE_FLAGS_1 = $(ALL_CXXFLAGS) -Wno-unguarded-availability-new -I$(OBJDIR)

# File sets
# #############################################

GENERATED :=
OBJECTS :=

GENERATED += $(OBJDIR)/AEFX_ArbParseHelper.o
GENERATED += $(OBJDIR)/AEFX_SuiteHelper.o
GENERATED += $(OBJDIR)/AEGP_AEPKConversion.o
GENERATED += $(OBJDIR)/AEGP_AssetBaker.o
GENERATED += $(OBJDIR)/AEGP_Attribute.o
GENERATED += $(OBJDIR)/AEGP_BaseContext.o
GENERATED += $(OBJDIR)/AEGP_CopyPixels.o
GENERATED += $(OBJDIR)/AEGP_D3D11Context.o
GENERATED += $(OBJDIR)/AEGP_D3D12Context.o
GENERATED += $(OBJDIR)/AEGP_FileDialog.o
GENERATED += $(OBJDIR)/AEGP_FileDialogMac.o
GENERATED += $(OBJDIR)/AEGP_FileWatcher.o
GENERATED += $(OBJDIR)/AEGP_FrameCollector.o
GENERATED += $(OBJDIR)/AEGP_GraphicalResourcesTreeModel.o
GENERATED += $(OBJDIR)/AEGP_LayerHolder.o
GENERATED += $(OBJDIR)/AEGP_Log.o
GENERATED += $(OBJDIR)/AEGP_Main.o
GENERATED += $(OBJDIR)/AEGP_MetalContext.o
GENERATED += $(OBJDIR)/AEGP_PackExplorer.o
GENERATED += $(OBJDIR)/AEGP_PanelQT.o
GENERATED += $(OBJDIR)/AEGP_ParticleScene.o
GENERATED += $(OBJDIR)/AEGP_PopcornFXPlugins.o
GENERATED += $(OBJDIR)/AEGP_RenderContext.o
GENERATED += $(OBJDIR)/AEGP_Scene.o
GENERATED += $(OBJDIR)/AEGP_SkinnedMesh.o
GENERATED += $(OBJDIR)/AEGP_SkinnedMeshInstance.o
GENERATED += $(OBJDIR)/AEGP_SuiteHandler.o
GENERATED += $(OBJDIR)/AEGP_System.o
GENERATED += $(OBJDIR)/AEGP_UpdateAEState.o
GENERATED += $(OBJDIR)/AEGP_Utils.o
GENERATED += $(OBJDIR)/AEGP_VaultHandler.o
GENERATED += $(OBJDIR)/AEGP_WinFileDialog.o
GENERATED += $(OBJDIR)/AEGP_WinSystem.o
GENERATED += $(OBJDIR)/AEGP_World.o
GENERATED += $(OBJDIR)/MissingSuiteError.o
GENERATED += $(OBJDIR)/Smart_Utils.o
GENERATED += $(OBJDIR)/ae_precompiled.o
OBJECTS += $(OBJDIR)/AEFX_ArbParseHelper.o
OBJECTS += $(OBJDIR)/AEFX_SuiteHelper.o
OBJECTS += $(OBJDIR)/AEGP_AEPKConversion.o
OBJECTS += $(OBJDIR)/AEGP_AssetBaker.o
OBJECTS += $(OBJDIR)/AEGP_Attribute.o
OBJECTS += $(OBJDIR)/AEGP_BaseContext.o
OBJECTS += $(OBJDIR)/AEGP_CopyPixels.o
OBJECTS += $(OBJDIR)/AEGP_D3D11Context.o
OBJECTS += $(OBJDIR)/AEGP_D3D12Context.o
OBJECTS += $(OBJDIR)/AEGP_FileDialog.o
OBJECTS += $(OBJDIR)/AEGP_FileDialogMac.o
OBJECTS += $(OBJDIR)/AEGP_FileWatcher.o
OBJECTS += $(OBJDIR)/AEGP_FrameCollector.o
OBJECTS += $(OBJDIR)/AEGP_GraphicalResourcesTreeModel.o
OBJECTS += $(OBJDIR)/AEGP_LayerHolder.o
OBJECTS += $(OBJDIR)/AEGP_Log.o
OBJECTS += $(OBJDIR)/AEGP_Main.o
OBJECTS += $(OBJDIR)/AEGP_MetalContext.o
OBJECTS += $(OBJDIR)/AEGP_PackExplorer.o
OBJECTS += $(OBJDIR)/AEGP_PanelQT.o
OBJECTS += $(OBJDIR)/AEGP_ParticleScene.o
OBJECTS += $(OBJDIR)/AEGP_PopcornFXPlugins.o
OBJECTS += $(OBJDIR)/AEGP_RenderContext.o
OBJECTS += $(OBJDIR)/AEGP_Scene.o
OBJECTS += $(OBJDIR)/AEGP_SkinnedMesh.o
OBJECTS += $(OBJDIR)/AEGP_SkinnedMeshInstance.o
OBJECTS += $(OBJDIR)/AEGP_SuiteHandler.o
OBJECTS += $(OBJDIR)/AEGP_System.o
OBJECTS += $(OBJDIR)/AEGP_UpdateAEState.o
OBJECTS += $(OBJDIR)/AEGP_Utils.o
OBJECTS += $(OBJDIR)/AEGP_VaultHandler.o
OBJECTS += $(OBJDIR)/AEGP_WinFileDialog.o
OBJECTS += $(OBJDIR)/AEGP_WinSystem.o
OBJECTS += $(OBJDIR)/AEGP_World.o
OBJECTS += $(OBJDIR)/MissingSuiteError.o
OBJECTS += $(OBJDIR)/Smart_Utils.o
OBJECTS += $(OBJDIR)/ae_precompiled.o

ifeq ($(config),debug_x64)
GENERATED += $(OBJDIR)/moc_AEGP_GraphicalResourcesTreeModel.o
GENERATED += $(OBJDIR)/moc_AEGP_PanelQT.o
OBJECTS += $(OBJDIR)/moc_AEGP_GraphicalResourcesTreeModel.o
OBJECTS += $(OBJDIR)/moc_AEGP_PanelQT.o

else ifeq ($(config),release_x64)
GENERATED += $(OBJDIR)/moc_AEGP_GraphicalResourcesTreeModel1.o
GENERATED += $(OBJDIR)/moc_AEGP_PanelQT1.o
OBJECTS += $(OBJDIR)/moc_AEGP_GraphicalResourcesTreeModel1.o
OBJECTS += $(OBJDIR)/moc_AEGP_PanelQT1.o

#else
#  $(error "invalid configuration $(config)")
endif

# Rules
# #############################################

all: $(TARGET)
	@:

$(TARGET): $(GENERATED) $(OBJECTS) $(LDDEPS) | $(TARGETDIR)
	$(PRELINKCMDS)
	@echo Linking AE_GeneralPlugin
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
	@echo Cleaning AE_GeneralPlugin
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
$(OBJDIR)/ae_precompiled.o: ../../AE_GeneralPlugin/Precompiled/ae_precompiled.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) $(PERFILE_FLAGS_0) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/AEGP_AEPKConversion.o: ../../AE_GeneralPlugin/Sources/AEGP_AEPKConversion.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) $(PERFILE_FLAGS_0) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/AEGP_AssetBaker.o: ../../AE_GeneralPlugin/Sources/AEGP_AssetBaker.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) $(PERFILE_FLAGS_0) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/AEGP_Attribute.o: ../../AE_GeneralPlugin/Sources/AEGP_Attribute.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) $(PERFILE_FLAGS_0) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/AEGP_FileDialog.o: ../../AE_GeneralPlugin/Sources/AEGP_FileDialog.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) $(PERFILE_FLAGS_0) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/AEGP_FileDialogMac.o: ../../AE_GeneralPlugin/Sources/AEGP_FileDialogMac.mm
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) $(PERFILE_FLAGS_1) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/AEGP_FileWatcher.o: ../../AE_GeneralPlugin/Sources/AEGP_FileWatcher.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) $(PERFILE_FLAGS_0) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/AEGP_FrameCollector.o: ../../AE_GeneralPlugin/Sources/AEGP_FrameCollector.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) $(PERFILE_FLAGS_0) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/AEGP_LayerHolder.o: ../../AE_GeneralPlugin/Sources/AEGP_LayerHolder.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) $(PERFILE_FLAGS_0) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/AEGP_Log.o: ../../AE_GeneralPlugin/Sources/AEGP_Log.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) $(PERFILE_FLAGS_0) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/AEGP_Main.o: ../../AE_GeneralPlugin/Sources/AEGP_Main.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) $(PERFILE_FLAGS_0) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/AEGP_PackExplorer.o: ../../AE_GeneralPlugin/Sources/AEGP_PackExplorer.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) $(PERFILE_FLAGS_0) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/AEGP_ParticleScene.o: ../../AE_GeneralPlugin/Sources/AEGP_ParticleScene.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) $(PERFILE_FLAGS_0) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/AEGP_PopcornFXPlugins.o: ../../AE_GeneralPlugin/Sources/AEGP_PopcornFXPlugins.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) $(PERFILE_FLAGS_0) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/AEGP_RenderContext.o: ../../AE_GeneralPlugin/Sources/AEGP_RenderContext.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) $(PERFILE_FLAGS_0) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/AEGP_Scene.o: ../../AE_GeneralPlugin/Sources/AEGP_Scene.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) $(PERFILE_FLAGS_0) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/AEGP_SkinnedMesh.o: ../../AE_GeneralPlugin/Sources/AEGP_SkinnedMesh.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) $(PERFILE_FLAGS_0) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/AEGP_SkinnedMeshInstance.o: ../../AE_GeneralPlugin/Sources/AEGP_SkinnedMeshInstance.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) $(PERFILE_FLAGS_0) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/AEGP_System.o: ../../AE_GeneralPlugin/Sources/AEGP_System.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) $(PERFILE_FLAGS_0) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/AEGP_UpdateAEState.o: ../../AE_GeneralPlugin/Sources/AEGP_UpdateAEState.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) $(PERFILE_FLAGS_0) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/AEGP_VaultHandler.o: ../../AE_GeneralPlugin/Sources/AEGP_VaultHandler.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) $(PERFILE_FLAGS_0) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/AEGP_WinFileDialog.o: ../../AE_GeneralPlugin/Sources/AEGP_WinFileDialog.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) $(PERFILE_FLAGS_0) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/AEGP_WinSystem.o: ../../AE_GeneralPlugin/Sources/AEGP_WinSystem.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) $(PERFILE_FLAGS_0) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/AEGP_World.o: ../../AE_GeneralPlugin/Sources/AEGP_World.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) $(PERFILE_FLAGS_0) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/AEGP_GraphicalResourcesTreeModel.o: ../../AE_GeneralPlugin/Sources/Panels/AEGP_GraphicalResourcesTreeModel.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) $(PERFILE_FLAGS_0) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/AEGP_PanelQT.o: ../../AE_GeneralPlugin/Sources/Panels/AEGP_PanelQT.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) $(PERFILE_FLAGS_0) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/AEGP_BaseContext.o: ../../AE_GeneralPlugin/Sources/RenderApi/AEGP_BaseContext.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) $(PERFILE_FLAGS_0) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/AEGP_CopyPixels.o: ../../AE_GeneralPlugin/Sources/RenderApi/AEGP_CopyPixels.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) $(PERFILE_FLAGS_0) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/AEGP_D3D11Context.o: ../../AE_GeneralPlugin/Sources/RenderApi/AEGP_D3D11Context.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) $(PERFILE_FLAGS_0) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/AEGP_D3D12Context.o: ../../AE_GeneralPlugin/Sources/RenderApi/AEGP_D3D12Context.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) $(PERFILE_FLAGS_0) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/AEGP_MetalContext.o: ../../AE_GeneralPlugin/Sources/RenderApi/AEGP_MetalContext.mm
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) $(PERFILE_FLAGS_1) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"

ifeq ($(config),debug_x64)
Qt/x64/Debug/moc_AEGP_GraphicalResourcesTreeModel.cpp: ../../AE_GeneralPlugin/Include/Panels/AEGP_GraphicalResourcesTreeModel.h
	@echo "Moc AEGP_GraphicalResourcesTreeModel.h"
	$(SILENT) QT_SELECT=5 $(QTDIR)/bin/moc "../../AE_GeneralPlugin/Include/Panels/AEGP_GraphicalResourcesTreeModel.h" -o "Qt/x64/Debug/moc_AEGP_GraphicalResourcesTreeModel.cpp" @"Qt/x64/Debug/moc_AEGP_GraphicalResourcesTreeModel.args"
Qt/x64/Debug/moc_AEGP_PanelQT.cpp: ../../AE_GeneralPlugin/Include/Panels/AEGP_PanelQT.h
	@echo "Moc AEGP_PanelQT.h"
	$(SILENT) QT_SELECT=5 $(QTDIR)/bin/moc "../../AE_GeneralPlugin/Include/Panels/AEGP_PanelQT.h" -o "Qt/x64/Debug/moc_AEGP_PanelQT.cpp" @"Qt/x64/Debug/moc_AEGP_PanelQT.args"
$(OBJDIR)/moc_AEGP_GraphicalResourcesTreeModel.o: Qt/x64/Debug/moc_AEGP_GraphicalResourcesTreeModel.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) $(PERFILE_FLAGS_0) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/moc_AEGP_PanelQT.o: Qt/x64/Debug/moc_AEGP_PanelQT.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) $(PERFILE_FLAGS_0) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"

else ifeq ($(config),release_x64)
Qt/x64/Release/moc_AEGP_GraphicalResourcesTreeModel.cpp: ../../AE_GeneralPlugin/Include/Panels/AEGP_GraphicalResourcesTreeModel.h
	@echo "Moc AEGP_GraphicalResourcesTreeModel.h"
	$(SILENT) QT_SELECT=5 $(QTDIR)/bin/moc "../../AE_GeneralPlugin/Include/Panels/AEGP_GraphicalResourcesTreeModel.h" -o "Qt/x64/Release/moc_AEGP_GraphicalResourcesTreeModel.cpp" @"Qt/x64/Release/moc_AEGP_GraphicalResourcesTreeModel.args"
Qt/x64/Release/moc_AEGP_PanelQT.cpp: ../../AE_GeneralPlugin/Include/Panels/AEGP_PanelQT.h
	@echo "Moc AEGP_PanelQT.h"
	$(SILENT) QT_SELECT=5 $(QTDIR)/bin/moc "../../AE_GeneralPlugin/Include/Panels/AEGP_PanelQT.h" -o "Qt/x64/Release/moc_AEGP_PanelQT.cpp" @"Qt/x64/Release/moc_AEGP_PanelQT.args"
$(OBJDIR)/moc_AEGP_GraphicalResourcesTreeModel1.o: Qt/x64/Release/moc_AEGP_GraphicalResourcesTreeModel.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) $(PERFILE_FLAGS_0) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/moc_AEGP_PanelQT1.o: Qt/x64/Release/moc_AEGP_PanelQT.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) $(PERFILE_FLAGS_0) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"

#else
#  $(error "invalid configuration $(config)")
endif

-include $(OBJECTS:%.o=%.d)
ifneq (,$(PCH))
  -include $(PCH_PLACEHOLDER).d
endif
