# Alternative GNU Make workspace makefile autogenerated by Premake

ifndef config
  config=debug_x64
endif

ifndef verbose
  SILENT = @
endif

ifeq ($(config),debug_x64)
  PK_Runtime_SDK1_config = debug_x64
  PK_Discretizers_SDK1_config = debug_x64
  PK_ParticlesToolbox_SDK1_config = debug_x64
  PK_RenderHelpers_SDK1_config = debug_x64
  PK_RHI_SDK1_config = debug_x64
  PK_SampleLib_config = debug_x64
  PK_MCPP_SDK1_config = debug_x64
  PK_AssetBakerLib_config = debug_x64
  AE_Effect_Emitter_config = debug_x64
  AE_Effect_AttributeSampler_config = debug_x64
  AE_Effect_Attribute_config = debug_x64
  AE_GeneralPlugin_config = debug_x64

else ifeq ($(config),release_x64)
  PK_Runtime_SDK1_config = release_x64
  PK_Discretizers_SDK1_config = release_x64
  PK_ParticlesToolbox_SDK1_config = release_x64
  PK_RenderHelpers_SDK1_config = release_x64
  PK_RHI_SDK1_config = release_x64
  PK_SampleLib_config = release_x64
  PK_MCPP_SDK1_config = release_x64
  PK_AssetBakerLib_config = release_x64
  AE_Effect_Emitter_config = release_x64
  AE_Effect_AttributeSampler_config = release_x64
  AE_Effect_Attribute_config = release_x64
  AE_GeneralPlugin_config = release_x64

else
  $(error "invalid configuration $(config)")
endif

PROJECTS := PK-Runtime_SDK1 PK-Discretizers_SDK1 PK-ParticlesToolbox_SDK1 PK-RenderHelpers_SDK1 PK-RHI_SDK1 PK-SampleLib PK-MCPP_SDK1 PK-AssetBakerLib AE_Effect_Emitter AE_Effect_AttributeSampler AE_Effect_Attribute AE_GeneralPlugin

.PHONY: all clean help $(PROJECTS) AE Integration Runtime Tools Tools/AssetBaker

all: $(PROJECTS)

AE: AE_Effect_Attribute AE_Effect_AttributeSampler AE_Effect_Emitter AE_GeneralPlugin

Integration: PK-RHI_SDK1 PK-RenderHelpers_SDK1 PK-SampleLib

Runtime: PK-Discretizers_SDK1 PK-ParticlesToolbox_SDK1 PK-Runtime_SDK1

Tools: Tools/AssetBaker PK-MCPP_SDK1

Tools/AssetBaker: PK-AssetBakerLib

PK-Runtime_SDK1:
ifneq (,$(PK_Runtime_SDK1_config))
	@echo "==== Building PK-Runtime_SDK1 ($(PK_Runtime_SDK1_config)) ===="
	@${MAKE} --no-print-directory -C . -f PK-Runtime_SDK1.make config=$(PK_Runtime_SDK1_config)
endif

PK-Discretizers_SDK1:
ifneq (,$(PK_Discretizers_SDK1_config))
	@echo "==== Building PK-Discretizers_SDK1 ($(PK_Discretizers_SDK1_config)) ===="
	@${MAKE} --no-print-directory -C . -f PK-Discretizers_SDK1.make config=$(PK_Discretizers_SDK1_config)
endif

PK-ParticlesToolbox_SDK1:
ifneq (,$(PK_ParticlesToolbox_SDK1_config))
	@echo "==== Building PK-ParticlesToolbox_SDK1 ($(PK_ParticlesToolbox_SDK1_config)) ===="
	@${MAKE} --no-print-directory -C . -f PK-ParticlesToolbox_SDK1.make config=$(PK_ParticlesToolbox_SDK1_config)
endif

PK-RenderHelpers_SDK1:
ifneq (,$(PK_RenderHelpers_SDK1_config))
	@echo "==== Building PK-RenderHelpers_SDK1 ($(PK_RenderHelpers_SDK1_config)) ===="
	@${MAKE} --no-print-directory -C . -f PK-RenderHelpers_SDK1.make config=$(PK_RenderHelpers_SDK1_config)
endif

PK-RHI_SDK1:
ifneq (,$(PK_RHI_SDK1_config))
	@echo "==== Building PK-RHI_SDK1 ($(PK_RHI_SDK1_config)) ===="
	@${MAKE} --no-print-directory -C . -f PK-RHI_SDK1.make config=$(PK_RHI_SDK1_config)
endif

PK-SampleLib:
ifneq (,$(PK_SampleLib_config))
	@echo "==== Building PK-SampleLib ($(PK_SampleLib_config)) ===="
	@${MAKE} --no-print-directory -C . -f PK-SampleLib.make config=$(PK_SampleLib_config)
endif

PK-MCPP_SDK1:
ifneq (,$(PK_MCPP_SDK1_config))
	@echo "==== Building PK-MCPP_SDK1 ($(PK_MCPP_SDK1_config)) ===="
	@${MAKE} --no-print-directory -C . -f PK-MCPP_SDK1.make config=$(PK_MCPP_SDK1_config)
endif

PK-AssetBakerLib:
ifneq (,$(PK_AssetBakerLib_config))
	@echo "==== Building PK-AssetBakerLib ($(PK_AssetBakerLib_config)) ===="
	@${MAKE} --no-print-directory -C . -f PK-AssetBakerLib.make config=$(PK_AssetBakerLib_config)
endif

AE_Effect_Emitter:
ifneq (,$(AE_Effect_Emitter_config))
	@echo "==== Building AE_Effect_Emitter ($(AE_Effect_Emitter_config)) ===="
	@${MAKE} --no-print-directory -C . -f AE_Effect_Emitter.make config=$(AE_Effect_Emitter_config)
endif

AE_Effect_AttributeSampler: AE_Effect_Emitter
ifneq (,$(AE_Effect_AttributeSampler_config))
	@echo "==== Building AE_Effect_AttributeSampler ($(AE_Effect_AttributeSampler_config)) ===="
	@${MAKE} --no-print-directory -C . -f AE_Effect_AttributeSampler.make config=$(AE_Effect_AttributeSampler_config)
endif

AE_Effect_Attribute: AE_Effect_Emitter
ifneq (,$(AE_Effect_Attribute_config))
	@echo "==== Building AE_Effect_Attribute ($(AE_Effect_Attribute_config)) ===="
	@${MAKE} --no-print-directory -C . -f AE_Effect_Attribute.make config=$(AE_Effect_Attribute_config)
endif

AE_GeneralPlugin: PK-AssetBakerLib PK-SampleLib AE_Effect_Emitter AE_Effect_Attribute AE_Effect_AttributeSampler
ifneq (,$(AE_GeneralPlugin_config))
	@echo "==== Building AE_GeneralPlugin ($(AE_GeneralPlugin_config)) ===="
	@${MAKE} --no-print-directory -C . -f AE_GeneralPlugin.make config=$(AE_GeneralPlugin_config)
endif

clean:
	@${MAKE} --no-print-directory -C . -f PK-Runtime_SDK1.make clean
	@${MAKE} --no-print-directory -C . -f PK-Discretizers_SDK1.make clean
	@${MAKE} --no-print-directory -C . -f PK-ParticlesToolbox_SDK1.make clean
	@${MAKE} --no-print-directory -C . -f PK-RenderHelpers_SDK1.make clean
	@${MAKE} --no-print-directory -C . -f PK-RHI_SDK1.make clean
	@${MAKE} --no-print-directory -C . -f PK-SampleLib.make clean
	@${MAKE} --no-print-directory -C . -f PK-MCPP_SDK1.make clean
	@${MAKE} --no-print-directory -C . -f PK-AssetBakerLib.make clean
	@${MAKE} --no-print-directory -C . -f AE_Effect_Emitter.make clean
	@${MAKE} --no-print-directory -C . -f AE_Effect_AttributeSampler.make clean
	@${MAKE} --no-print-directory -C . -f AE_Effect_Attribute.make clean
	@${MAKE} --no-print-directory -C . -f AE_GeneralPlugin.make clean

help:
	@echo "Usage: make [config=name] [target]"
	@echo ""
	@echo "CONFIGURATIONS:"
	@echo "  debug_x64"
	@echo "  release_x64"
	@echo ""
	@echo "TARGETS:"
	@echo "   all (default)"
	@echo "   clean"
	@echo "   PK-Runtime_SDK1"
	@echo "   PK-Discretizers_SDK1"
	@echo "   PK-ParticlesToolbox_SDK1"
	@echo "   PK-RenderHelpers_SDK1"
	@echo "   PK-RHI_SDK1"
	@echo "   PK-SampleLib"
	@echo "   PK-MCPP_SDK1"
	@echo "   PK-AssetBakerLib"
	@echo "   AE_Effect_Emitter"
	@echo "   AE_Effect_AttributeSampler"
	@echo "   AE_Effect_Attribute"
	@echo "   AE_GeneralPlugin"
	@echo ""
	@echo "For more information, see https://github.com/premake/premake-core/wiki"
