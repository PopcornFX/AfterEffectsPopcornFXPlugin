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
PCH = ../../Samples/precompiled/precompiled.h
PCH_PLACEHOLDER = $(OBJDIR)/$(notdir $(PCH))
GCH = $(PCH_PLACEHOLDER).gch
INCLUDES += -I../../ExternalLibs/Runtime -I../../ExternalLibs/Runtime/include -I../../ExternalLibs/Runtime/include/license/AfterEffects -I../../Samples -I../../Samples/precompiled -I../../Samples/PK-SampleLib -I../../ExternalLibs/GL/include -I/usr/local/include/SDL2 -I../../ExternalLibs/imgui
FORCE_INCLUDE += -include pk_compiler_warnings.h
ALL_CPPFLAGS += $(CPPFLAGS) -MD -MP $(DEFINES) $(INCLUDES)
ALL_RESFLAGS += $(RESFLAGS) $(DEFINES) $(INCLUDES)
LIBS +=
LDDEPS +=
ALL_LDFLAGS += $(LDFLAGS) -m64 -target x86_64-apple-macos10.14
define LINKCMD
  $(SILENT) $(RM) -f $@
  $(SILENT) $(AR) -rcs $@ $(OBJECTS)
endef
define PREBUILDCMDS
endef
define PRELINKCMDS
endef
define POSTBUILDCMDS
endef

ifeq ($(config),debug_x64)
TARGETDIR = ../../ExternalLibs/Runtime/bin/AfterEffects/gmake_macosx_x64
TARGET = $(TARGETDIR)/libPK-SampleLib_d.a
OBJDIR = ../intermediate/AfterEffects/GM/x64/Debug/PK-SampleLib
DEFINES += -D_DEBUG -DPK_COMPILER_BUILD_COMPILER_D3D11=1 -DPK_COMPILER_BUILD_COMPILER_D3D12=1 -DPK_BUILD_WITH_METAL_SUPPORT=1 -DPK_BUILD_WITH_OGL_SUPPORT=1 -DGL_GLEXT_PROTOTYPES -DGLEW_STATIC -DGLEW_NO_GLU -DPK_BUILD_WITH_SDL=0 -DPK_BUILD_WITH_FMODEX_SUPPORT=0
ALL_CFLAGS += $(CFLAGS) $(ALL_CPPFLAGS) -m64 -Wshadow -Wundef -fno-omit-frame-pointer -fno-strict-aliasing -g -msse2 -Wall -Wextra -Winvalid-pch -Wno-pragma-pack -fno-math-errno -fno-trapping-math -ggdb -mfpmath=sse -target x86_64-apple-macos10.14 -iwithsysroot `xcrun --show-sdk-path`
ALL_CXXFLAGS += $(CXXFLAGS) $(ALL_CPPFLAGS) -m64 -Wshadow -Wundef -fno-omit-frame-pointer -fno-strict-aliasing -g -msse2 -Wall -Wextra -std=gnu++0x -fno-exceptions -fvisibility-inlines-hidden -fno-rtti -fvisibility=hidden -Winvalid-pch -Wno-pragma-pack -fno-math-errno -fno-trapping-math -ggdb -mfpmath=sse -target x86_64-apple-macos10.14 -iwithsysroot `xcrun --show-sdk-path`

else ifeq ($(config),release_x64)
TARGETDIR = ../../ExternalLibs/Runtime/bin/AfterEffects/gmake_macosx_x64
TARGET = $(TARGETDIR)/libPK-SampleLib_r.a
OBJDIR = ../intermediate/AfterEffects/GM/x64/Release/PK-SampleLib
DEFINES += -DNDEBUG -DPK_COMPILER_BUILD_COMPILER_D3D11=1 -DPK_COMPILER_BUILD_COMPILER_D3D12=1 -DPK_BUILD_WITH_METAL_SUPPORT=1 -DPK_BUILD_WITH_OGL_SUPPORT=1 -DGL_GLEXT_PROTOTYPES -DGLEW_STATIC -DGLEW_NO_GLU -DPK_BUILD_WITH_SDL=0 -DPK_BUILD_WITH_FMODEX_SUPPORT=0
ALL_CFLAGS += $(CFLAGS) $(ALL_CPPFLAGS) -m64 -Wshadow -Wundef -fno-omit-frame-pointer -O3 -fno-strict-aliasing -g -msse2 -Wall -Wextra -Winvalid-pch -Wno-pragma-pack -fno-math-errno -fno-trapping-math -mfpmath=sse -target x86_64-apple-macos10.14 -iwithsysroot `xcrun --show-sdk-path`
ALL_CXXFLAGS += $(CXXFLAGS) $(ALL_CPPFLAGS) -m64 -Wshadow -Wundef -fno-omit-frame-pointer -O3 -fno-strict-aliasing -g -msse2 -Wall -Wextra -std=gnu++0x -fno-exceptions -fvisibility-inlines-hidden -fno-rtti -fvisibility=hidden -Winvalid-pch -Wno-pragma-pack -fno-math-errno -fno-trapping-math -mfpmath=sse -target x86_64-apple-macos10.14 -iwithsysroot `xcrun --show-sdk-path`

else ifeq ($(config),retail_x64)
TARGETDIR = ../../ExternalLibs/Runtime/bin/AfterEffects/gmake_macosx_x64
TARGET = $(TARGETDIR)/libPK-SampleLib_s.a
OBJDIR = ../intermediate/AfterEffects/GM/x64/Retail/PK-SampleLib
DEFINES += -DNDEBUG -DPK_RETAIL -DPK_COMPILER_BUILD_COMPILER_D3D11=1 -DPK_COMPILER_BUILD_COMPILER_D3D12=1 -DPK_BUILD_WITH_METAL_SUPPORT=1 -DPK_BUILD_WITH_OGL_SUPPORT=1 -DGL_GLEXT_PROTOTYPES -DGLEW_STATIC -DGLEW_NO_GLU -DPK_BUILD_WITH_SDL=0 -DPK_BUILD_WITH_FMODEX_SUPPORT=0
ALL_CFLAGS += $(CFLAGS) $(ALL_CPPFLAGS) -m64 -Wshadow -Wundef -fomit-frame-pointer -O3 -fno-strict-aliasing -msse2 -Wall -Wextra -Winvalid-pch -Wno-pragma-pack -fno-math-errno -fno-trapping-math -mfpmath=sse -target x86_64-apple-macos10.14 -iwithsysroot `xcrun --show-sdk-path`
ALL_CXXFLAGS += $(CXXFLAGS) $(ALL_CPPFLAGS) -m64 -Wshadow -Wundef -fomit-frame-pointer -O3 -fno-strict-aliasing -msse2 -Wall -Wextra -std=gnu++0x -fno-exceptions -fvisibility-inlines-hidden -fno-rtti -fvisibility=hidden -Winvalid-pch -Wno-pragma-pack -fno-math-errno -fno-trapping-math -mfpmath=sse -target x86_64-apple-macos10.14 -iwithsysroot `xcrun --show-sdk-path`

#else
#  $(error "invalid configuration $(config)")
endif

# Per File Configurations
# #############################################

PERFILE_FLAGS_0 = $(ALL_CXXFLAGS) -Wno-unguarded-availability-new

# File sets
# #############################################

GENERATED :=
OBJECTS :=

GENERATED += $(OBJDIR)/AWindowContext.o
GENERATED += $(OBJDIR)/AbstractGraphicScene.o
GENERATED += $(OBJDIR)/BRDFLUT.o
GENERATED += $(OBJDIR)/BasicSceneShaderDefinitions.o
GENERATED += $(OBJDIR)/BlueNoise.o
GENERATED += $(OBJDIR)/Camera.o
GENERATED += $(OBJDIR)/D3D11Context.o
GENERATED += $(OBJDIR)/D3D12Context.o
GENERATED += $(OBJDIR)/DebugHelper.o
GENERATED += $(OBJDIR)/DeferredScene.o
GENERATED += $(OBJDIR)/DirectionalShadows.o
GENERATED += $(OBJDIR)/DownSampleTexture.o
GENERATED += $(OBJDIR)/EGLContext.o
GENERATED += $(OBJDIR)/EditorShaderDefinitions.o
GENERATED += $(OBJDIR)/EnvironmentMapEntity.o
GENERATED += $(OBJDIR)/FModBillboardingBatchPolicy.o
GENERATED += $(OBJDIR)/FeatureRenderingSettings.o
GENERATED += $(OBJDIR)/FrameCollector.o
GENERATED += $(OBJDIR)/GBuffer.o
GENERATED += $(OBJDIR)/GLContext.o
GENERATED += $(OBJDIR)/GLSLShaderGenerator.o
GENERATED += $(OBJDIR)/GLXContext.o
GENERATED += $(OBJDIR)/Gizmo.o
GENERATED += $(OBJDIR)/HLSLShaderGenerator.o
GENERATED += $(OBJDIR)/ImguiRhiImplem.o
GENERATED += $(OBJDIR)/LightEntity.o
GENERATED += $(OBJDIR)/MaterialToRHI.o
GENERATED += $(OBJDIR)/MeshEntity.o
GENERATED += $(OBJDIR)/MetalContext.o
GENERATED += $(OBJDIR)/MetalContextFactory.o
GENERATED += $(OBJDIR)/MetalShaderGenerator.o
GENERATED += $(OBJDIR)/NSGLContext.o
GENERATED += $(OBJDIR)/OffscreenContext.o
GENERATED += $(OBJDIR)/PKPix.o
GENERATED += $(OBJDIR)/PKSampleInit.o
GENERATED += $(OBJDIR)/ParticleShaderGenerator.o
GENERATED += $(OBJDIR)/PipelineCacheHelper.o
GENERATED += $(OBJDIR)/PopcornStartup.o
GENERATED += $(OBJDIR)/PostFxBloom.o
GENERATED += $(OBJDIR)/PostFxColorRemap.o
GENERATED += $(OBJDIR)/PostFxDistortion.o
GENERATED += $(OBJDIR)/PostFxFXAA.o
GENERATED += $(OBJDIR)/PostFxToneMapping.o
GENERATED += $(OBJDIR)/ProfilerRenderer.o
GENERATED += $(OBJDIR)/RHIBillboardingBatch_CPUsim.o
GENERATED += $(OBJDIR)/RHIBillboardingBatch_GPUsim.o
GENERATED += $(OBJDIR)/RHICustomTasks.o
GENERATED += $(OBJDIR)/RHIGPUSorter.o
GENERATED += $(OBJDIR)/RHIGraphicResources.o
GENERATED += $(OBJDIR)/RHIParticleRenderDataFactory.o
GENERATED += $(OBJDIR)/RHIRenderParticleSceneHelpers.o
GENERATED += $(OBJDIR)/RendererCache.o
GENERATED += $(OBJDIR)/SampleLibShaderDefinitions.o
GENERATED += $(OBJDIR)/SampleUtils.o
GENERATED += $(OBJDIR)/SdlContext.o
GENERATED += $(OBJDIR)/ShaderDefinitions.o
GENERATED += $(OBJDIR)/ShaderGenerator.o
GENERATED += $(OBJDIR)/ShaderLoader.o
GENERATED += $(OBJDIR)/SimInterface_GBufferSampling.o
GENERATED += $(OBJDIR)/SoundPoolCache.o
GENERATED += $(OBJDIR)/UnitTestsShaderDefinitions.o
GENERATED += $(OBJDIR)/VulkanContext.o
GENERATED += $(OBJDIR)/VulkanShaderGenerator.o
GENERATED += $(OBJDIR)/WGLContext.o
GENERATED += $(OBJDIR)/imgui.o
GENERATED += $(OBJDIR)/imgui_demo.o
GENERATED += $(OBJDIR)/imgui_draw.o
GENERATED += $(OBJDIR)/precompiled.o
OBJECTS += $(OBJDIR)/AWindowContext.o
OBJECTS += $(OBJDIR)/AbstractGraphicScene.o
OBJECTS += $(OBJDIR)/BRDFLUT.o
OBJECTS += $(OBJDIR)/BasicSceneShaderDefinitions.o
OBJECTS += $(OBJDIR)/BlueNoise.o
OBJECTS += $(OBJDIR)/Camera.o
OBJECTS += $(OBJDIR)/D3D11Context.o
OBJECTS += $(OBJDIR)/D3D12Context.o
OBJECTS += $(OBJDIR)/DebugHelper.o
OBJECTS += $(OBJDIR)/DeferredScene.o
OBJECTS += $(OBJDIR)/DirectionalShadows.o
OBJECTS += $(OBJDIR)/DownSampleTexture.o
OBJECTS += $(OBJDIR)/EGLContext.o
OBJECTS += $(OBJDIR)/EditorShaderDefinitions.o
OBJECTS += $(OBJDIR)/EnvironmentMapEntity.o
OBJECTS += $(OBJDIR)/FModBillboardingBatchPolicy.o
OBJECTS += $(OBJDIR)/FeatureRenderingSettings.o
OBJECTS += $(OBJDIR)/FrameCollector.o
OBJECTS += $(OBJDIR)/GBuffer.o
OBJECTS += $(OBJDIR)/GLContext.o
OBJECTS += $(OBJDIR)/GLSLShaderGenerator.o
OBJECTS += $(OBJDIR)/GLXContext.o
OBJECTS += $(OBJDIR)/Gizmo.o
OBJECTS += $(OBJDIR)/HLSLShaderGenerator.o
OBJECTS += $(OBJDIR)/ImguiRhiImplem.o
OBJECTS += $(OBJDIR)/LightEntity.o
OBJECTS += $(OBJDIR)/MaterialToRHI.o
OBJECTS += $(OBJDIR)/MeshEntity.o
OBJECTS += $(OBJDIR)/MetalContext.o
OBJECTS += $(OBJDIR)/MetalContextFactory.o
OBJECTS += $(OBJDIR)/MetalShaderGenerator.o
OBJECTS += $(OBJDIR)/NSGLContext.o
OBJECTS += $(OBJDIR)/OffscreenContext.o
OBJECTS += $(OBJDIR)/PKPix.o
OBJECTS += $(OBJDIR)/PKSampleInit.o
OBJECTS += $(OBJDIR)/ParticleShaderGenerator.o
OBJECTS += $(OBJDIR)/PipelineCacheHelper.o
OBJECTS += $(OBJDIR)/PopcornStartup.o
OBJECTS += $(OBJDIR)/PostFxBloom.o
OBJECTS += $(OBJDIR)/PostFxColorRemap.o
OBJECTS += $(OBJDIR)/PostFxDistortion.o
OBJECTS += $(OBJDIR)/PostFxFXAA.o
OBJECTS += $(OBJDIR)/PostFxToneMapping.o
OBJECTS += $(OBJDIR)/ProfilerRenderer.o
OBJECTS += $(OBJDIR)/RHIBillboardingBatch_CPUsim.o
OBJECTS += $(OBJDIR)/RHIBillboardingBatch_GPUsim.o
OBJECTS += $(OBJDIR)/RHICustomTasks.o
OBJECTS += $(OBJDIR)/RHIGPUSorter.o
OBJECTS += $(OBJDIR)/RHIGraphicResources.o
OBJECTS += $(OBJDIR)/RHIParticleRenderDataFactory.o
OBJECTS += $(OBJDIR)/RHIRenderParticleSceneHelpers.o
OBJECTS += $(OBJDIR)/RendererCache.o
OBJECTS += $(OBJDIR)/SampleLibShaderDefinitions.o
OBJECTS += $(OBJDIR)/SampleUtils.o
OBJECTS += $(OBJDIR)/SdlContext.o
OBJECTS += $(OBJDIR)/ShaderDefinitions.o
OBJECTS += $(OBJDIR)/ShaderGenerator.o
OBJECTS += $(OBJDIR)/ShaderLoader.o
OBJECTS += $(OBJDIR)/SimInterface_GBufferSampling.o
OBJECTS += $(OBJDIR)/SoundPoolCache.o
OBJECTS += $(OBJDIR)/UnitTestsShaderDefinitions.o
OBJECTS += $(OBJDIR)/VulkanContext.o
OBJECTS += $(OBJDIR)/VulkanShaderGenerator.o
OBJECTS += $(OBJDIR)/WGLContext.o
OBJECTS += $(OBJDIR)/imgui.o
OBJECTS += $(OBJDIR)/imgui_demo.o
OBJECTS += $(OBJDIR)/imgui_draw.o
OBJECTS += $(OBJDIR)/precompiled.o

# Rules
# #############################################

all: $(TARGET)
	@:

$(TARGET): $(GENERATED) $(OBJECTS) $(LDDEPS) | $(TARGETDIR)
	$(PRELINKCMDS)
	@echo Linking PK-SampleLib
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
	@echo Cleaning PK-SampleLib
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

$(OBJDIR)/MetalContext.o: ../../Samples/PK-SampleLib/ApiContext/Metal/MetalContext.mm
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) $(PERFILE_FLAGS_0) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/MetalContextFactory.o: ../../Samples/PK-SampleLib/ApiContext/Metal/MetalContextFactory.mm
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) $(PERFILE_FLAGS_0) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/NSGLContext.o: ../../Samples/PK-SampleLib/ApiContext/OpenGL/NSGLContext.mm
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) $(PERFILE_FLAGS_0) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/precompiled.o: ../../Samples/precompiled/precompiled.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/D3D11Context.o: ../../Samples/PK-SampleLib/ApiContext/D3D/D3D11Context.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/D3D12Context.o: ../../Samples/PK-SampleLib/ApiContext/D3D/D3D12Context.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/EGLContext.o: ../../Samples/PK-SampleLib/ApiContext/OpenGL/EGLContext.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/GLContext.o: ../../Samples/PK-SampleLib/ApiContext/OpenGL/GLContext.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/GLXContext.o: ../../Samples/PK-SampleLib/ApiContext/OpenGL/GLXContext.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/WGLContext.o: ../../Samples/PK-SampleLib/ApiContext/OpenGL/WGLContext.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/VulkanContext.o: ../../Samples/PK-SampleLib/ApiContext/Vulkan/VulkanContext.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/BRDFLUT.o: ../../Samples/PK-SampleLib/BRDFLUT.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/BlueNoise.o: ../../Samples/PK-SampleLib/BlueNoise.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/Camera.o: ../../Samples/PK-SampleLib/Camera.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/DebugHelper.o: ../../Samples/PK-SampleLib/DebugHelper.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/Gizmo.o: ../../Samples/PK-SampleLib/Gizmo.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/ImguiRhiImplem.o: ../../Samples/PK-SampleLib/ImguiRhiImplem.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/PKPix.o: ../../Samples/PK-SampleLib/PKPix.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/PKSampleInit.o: ../../Samples/PK-SampleLib/PKSampleInit.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/PipelineCacheHelper.o: ../../Samples/PK-SampleLib/PipelineCacheHelper.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/PopcornStartup.o: ../../Samples/PK-SampleLib/PopcornStartup/PopcornStartup.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/ProfilerRenderer.o: ../../Samples/PK-SampleLib/ProfilerRenderer.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/RHIRenderParticleSceneHelpers.o: ../../Samples/PK-SampleLib/RHIRenderParticleSceneHelpers.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/FeatureRenderingSettings.o: ../../Samples/PK-SampleLib/RenderIntegrationRHI/FeatureRenderingSettings.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/FrameCollector.o: ../../Samples/PK-SampleLib/RenderIntegrationRHI/FrameCollector.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/MaterialToRHI.o: ../../Samples/PK-SampleLib/RenderIntegrationRHI/MaterialToRHI.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/RHIBillboardingBatch_CPUsim.o: ../../Samples/PK-SampleLib/RenderIntegrationRHI/RHIBillboardingBatch_CPUsim.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/RHIBillboardingBatch_GPUsim.o: ../../Samples/PK-SampleLib/RenderIntegrationRHI/RHIBillboardingBatch_GPUsim.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/RHICustomTasks.o: ../../Samples/PK-SampleLib/RenderIntegrationRHI/RHICustomTasks.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/RHIGPUSorter.o: ../../Samples/PK-SampleLib/RenderIntegrationRHI/RHIGPUSorter.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/RHIGraphicResources.o: ../../Samples/PK-SampleLib/RenderIntegrationRHI/RHIGraphicResources.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/RHIParticleRenderDataFactory.o: ../../Samples/PK-SampleLib/RenderIntegrationRHI/RHIParticleRenderDataFactory.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/RendererCache.o: ../../Samples/PK-SampleLib/RenderIntegrationRHI/RendererCache.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/DirectionalShadows.o: ../../Samples/PK-SampleLib/RenderPasses/DirectionalShadows.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/DownSampleTexture.o: ../../Samples/PK-SampleLib/RenderPasses/DownSampleTexture.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/GBuffer.o: ../../Samples/PK-SampleLib/RenderPasses/GBuffer.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/PostFxBloom.o: ../../Samples/PK-SampleLib/RenderPasses/PostFxBloom.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/PostFxColorRemap.o: ../../Samples/PK-SampleLib/RenderPasses/PostFxColorRemap.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/PostFxDistortion.o: ../../Samples/PK-SampleLib/RenderPasses/PostFxDistortion.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/PostFxFXAA.o: ../../Samples/PK-SampleLib/RenderPasses/PostFxFXAA.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/PostFxToneMapping.o: ../../Samples/PK-SampleLib/RenderPasses/PostFxToneMapping.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/AbstractGraphicScene.o: ../../Samples/PK-SampleLib/SampleScene/AbstractGraphicScene.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/DeferredScene.o: ../../Samples/PK-SampleLib/SampleScene/DeferredScene.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/EnvironmentMapEntity.o: ../../Samples/PK-SampleLib/SampleScene/Entities/EnvironmentMapEntity.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/LightEntity.o: ../../Samples/PK-SampleLib/SampleScene/Entities/LightEntity.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/MeshEntity.o: ../../Samples/PK-SampleLib/SampleScene/Entities/MeshEntity.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/SampleUtils.o: ../../Samples/PK-SampleLib/SampleUtils.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/BasicSceneShaderDefinitions.o: ../../Samples/PK-SampleLib/ShaderDefinitions/BasicSceneShaderDefinitions.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/EditorShaderDefinitions.o: ../../Samples/PK-SampleLib/ShaderDefinitions/EditorShaderDefinitions.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/SampleLibShaderDefinitions.o: ../../Samples/PK-SampleLib/ShaderDefinitions/SampleLibShaderDefinitions.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/ShaderDefinitions.o: ../../Samples/PK-SampleLib/ShaderDefinitions/ShaderDefinitions.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/UnitTestsShaderDefinitions.o: ../../Samples/PK-SampleLib/ShaderDefinitions/UnitTestsShaderDefinitions.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/GLSLShaderGenerator.o: ../../Samples/PK-SampleLib/ShaderGenerator/GLSLShaderGenerator.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/HLSLShaderGenerator.o: ../../Samples/PK-SampleLib/ShaderGenerator/HLSLShaderGenerator.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/MetalShaderGenerator.o: ../../Samples/PK-SampleLib/ShaderGenerator/MetalShaderGenerator.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/ParticleShaderGenerator.o: ../../Samples/PK-SampleLib/ShaderGenerator/ParticleShaderGenerator.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/ShaderGenerator.o: ../../Samples/PK-SampleLib/ShaderGenerator/ShaderGenerator.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/VulkanShaderGenerator.o: ../../Samples/PK-SampleLib/ShaderGenerator/VulkanShaderGenerator.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/ShaderLoader.o: ../../Samples/PK-SampleLib/ShaderLoader.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/SimInterface_GBufferSampling.o: ../../Samples/PK-SampleLib/SimInterfaces/SimInterface_GBufferSampling.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/FModBillboardingBatchPolicy.o: ../../Samples/PK-SampleLib/SoundIntegrationFMod/FModBillboardingBatchPolicy.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/SoundPoolCache.o: ../../Samples/PK-SampleLib/SoundIntegrationFMod/SoundPoolCache.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/AWindowContext.o: ../../Samples/PK-SampleLib/WindowContext/AWindowContext.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/OffscreenContext.o: ../../Samples/PK-SampleLib/WindowContext/OffscreenContext/OffscreenContext.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/SdlContext.o: ../../Samples/PK-SampleLib/WindowContext/SdlContext/SdlContext.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/imgui.o: ../../ExternalLibs/imgui/imgui.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/imgui_demo.o: ../../ExternalLibs/imgui/imgui_demo.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"
$(OBJDIR)/imgui_draw.o: ../../ExternalLibs/imgui/imgui_draw.cpp
	@echo "$(notdir $<)"
	$(SILENT) $(CXX) -include $(PCH_PLACEHOLDER) $(ALL_CXXFLAGS) $(FORCE_INCLUDE) -o "$@" -MF "$(@:%.o=%.d)" -c "$<"

-include $(OBJECTS:%.o=%.d)
ifneq (,$(PCH))
  -include $(PCH_PLACEHOLDER).d
endif
