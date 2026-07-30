# fpgactl - single user entry point.
# All CLI targets/variables are documented in docs/CLI.md (keep in lockstep).

BOARD ?= zcu104

-include local.mk

ifeq ($(wildcard boards/$(BOARD)/board.mk),)
$(error unknown BOARD '$(BOARD)'. Available: $(notdir $(patsubst %/board.mk,%,$(wildcard boards/*/board.mk))))
endif
include boards/$(BOARD)/board.mk

BOARD_DIR    := boards/$(BOARD)
BUILD_FOLDER ?= build
BUILD_DIR    := $(BUILD_FOLDER)/$(BOARD)
XSA          ?= $(BUILD_DIR)/arch/system_wrapper.xsa
ELF          ?= $(BUILD_DIR)/sw/app.elf
OUTPUT       ?= $(BUILD_DIR)/fpga_config_output.txt
STAGE       := $(BUILD_DIR)/sw/src
STAGE_STAMP := $(BUILD_DIR)/sw/.stage.stamp
RUN_BUNDLE  := $(BUILD_DIR)/run_bundle
TIMEOUT     ?= 120
USE_NIX     ?= 1
ARCH_FAST   ?= 0
PROGRAM_BIT ?= 0
RUN_WRAPPER ?=
REMOTE_DIR  := fpgactl/$(BOARD)

# ---------------------------------------------------------------------------
# Tools: Vivado/Vitis/xsct from the system install (never from nix).
# Point VIVADOPATH / VITISPATH at the install (local.mk, bashrc or exported).
# The path may name a specific version (<root>/2024.1) or the versionless
# root (<root>/) - in that case the highest installed version is used.
# Set VIVADO_SERVER/VIVADO_USER (or VITIS_SERVER/VITIS_USER) to run that
# step on a remote machine instead.
# ---------------------------------------------------------------------------
# $(call resolve_tool,<path>,<binary>): <path>/bin/<binary> if it exists,
# otherwise <path>/<highest version>/bin/<binary>
resolve_tool = $(if $(wildcard $(1)/bin/$(2)),$(1)/bin/$(2),$(lastword $(shell ls -d $(1)/*/bin/$(2) 2>/dev/null | sort -V)))

# Lazily resolved: a path that only exists on a remote host must not fail
# locally, so the error fires only when the local tool is actually invoked.
ifneq ($(VIVADOPATH),)
VIVADO = $(or $(call resolve_tool,$(VIVADOPATH),vivado),$(error no Vivado install found under VIVADOPATH='$(VIVADOPATH)'))
else
VIVADO = vivado
endif

ifneq ($(VITISPATH),)
VITIS = $(or $(call resolve_tool,$(VITISPATH),vitis),$(error no Vitis install found under VITISPATH='$(VITISPATH)'))
XSCT  = $(dir $(VITIS))xsct
else
VITIS = vitis
XSCT  = xsct
endif

# The same VIVADOPATH/VITISPATH are resolved on the remote host (falling
# back to the remote PATH when unset).
REMOTE_VIVADO_CMD = $(if $(VIVADOPATH),$$([ -x $(VIVADOPATH)/bin/vivado ] && echo $(VIVADOPATH)/bin/vivado || ls -d $(VIVADOPATH)/*/bin/vivado 2>/dev/null | sort -V | tail -1),vivado)
REMOTE_VITIS_CMD  = $(if $(VITISPATH),$$([ -x $(VITISPATH)/bin/vitis ] && echo $(VITISPATH)/bin/vitis || ls -d $(VITISPATH)/*/bin/vitis 2>/dev/null | sort -V | tail -1),vitis)

REMOTE_ARCH_DIR := fpgactl/$(BOARD)/arch
REMOTE_SW_DIR   := fpgactl/$(BOARD)/sw

# Host-side python steps run inside nix-shell unless USE_NIX=0.
# The shell file is referenced by absolute path because recipes may cd first.
ifeq ($(USE_NIX),1)
NIXRUN = nix-shell $(abspath default.nix) --run
else
NIXRUN = sh -c
endif

# Minimal colored step output (colored text, cyan = step/phase)
STEP = printf '\033[1;36m%s\033[0m\n'
OK   = printf '\033[0;32m%s\033[0m\n'
SKIP = printf '\033[0;33m%s\033[0m\n'

# FULL_RUN=1 forces both steps; FORCE_SW=1 forces only the firmware build
ifeq ($(FULL_RUN),1)
FORCE_ARCH := 1
FORCE_SW   := 1
endif

# ---------------------------------------------------------------------------
# Function / script selection (FUNCTION= | SCRIPT= | default script)
# ---------------------------------------------------------------------------
COMMON_FUNCS := $(notdir $(basename $(wildcard common/functions/*.c)))
BOARD_FUNCS  := $(notdir $(basename $(wildcard $(BOARD_DIR)/functions/*.c)))
FUNC_NAMES   := $(sort $(COMMON_FUNCS) $(BOARD_FUNCS))

ifneq ($(FUNCTION),)
  ifneq ($(SCRIPT),)
    $(error FUNCTION= and SCRIPT= are mutually exclusive)
  endif
  ifeq ($(filter $(FUNCTION),$(FUNC_NAMES)),)
    $(error unknown FUNCTION '$(FUNCTION)'. Available: $(FUNC_NAMES))
  endif
  ENTRY      := $(FUNCTION)
  ENTRY_NAME := $(FUNCTION)
  SCRIPT_ARG :=
else
  SCRIPT ?= scripts/default_script.c
  ifeq ($(wildcard $(SCRIPT)),)
    $(error SCRIPT '$(SCRIPT)' not found)
  endif
  ENTRY      := fpga_script
  ENTRY_NAME := script:$(notdir $(SCRIPT))
  SCRIPT_ARG := --script $(SCRIPT)
endif

.PHONY: all arch sw run run-banner functions board-list info clean FORCE
.DEFAULT_GOAL := all

all: run

# ---------------------------------------------------------------------------
# Step 1: architecture (Vivado -> XSA). XSA= names the file for both reading
# and writing: exists -> skipped; missing -> generated at that path.
# ---------------------------------------------------------------------------
ifeq ($(SKIP_ARCH),1)
$(XSA):
	@test -f "$@" || { echo "ERROR: SKIP_ARCH=1 but architecture '$(XSA)' does not exist"; exit 2; }
	@$(SKIP) "arch: skipped (SKIP_ARCH=1, using $(XSA))"
else
ifeq ($(FORCE_ARCH),1)
$(XSA): FORCE
else
$(XSA): $(BOARD_DIR)/vivado/gen_arch.tcl $(BOARD_DIR)/vivado/ps_config.tcl
endif
ifeq ($(VIVADO_SERVER),)
	@$(STEP) "arch: generating architecture with Vivado (BOARD=$(BOARD))"
	@mkdir -p $(dir $@)
	cd $(dir $@) && $(VIVADO) -nojournal -log vivado_arch.log -mode batch \
	    -source $(abspath $(BOARD_DIR)/vivado/gen_arch.tcl) \
	    -tclargs $(abspath $@) $(BOARD_PART) $(ARCH_FAST) $(abspath $(BOARD_DIR)/vivado/ps_config.tcl)
else
	@$(STEP) "arch: generating architecture on $(VIVADO_USER)@$(VIVADO_SERVER)"
	@mkdir -p $(dir $@)
	ssh $(VIVADO_SSH_FLAGS) $(VIVADO_USER)@$(VIVADO_SERVER) "mkdir -p $(REMOTE_ARCH_DIR)/src"
	rsync $(VIVADO_SYNC_FLAGS) -az --delete $(BOARD_DIR)/vivado/ $(VIVADO_USER)@$(VIVADO_SERVER):$(REMOTE_ARCH_DIR)/src/
	ssh -t $(VIVADO_SSH_FLAGS) $(VIVADO_USER)@$(VIVADO_SERVER) 'cd $(REMOTE_ARCH_DIR) && $(REMOTE_VIVADO_CMD) -nojournal -log vivado_arch.log -mode batch -source src/gen_arch.tcl -tclargs $$PWD/system_wrapper.xsa $(BOARD_PART) $(ARCH_FAST) $$PWD/src/ps_config.tcl'
	scp -q $(VIVADO_SSH_FLAGS) $(VIVADO_USER)@$(VIVADO_SERVER):$(REMOTE_ARCH_DIR)/system_wrapper.xsa $@
endif
	@test -f "$@"
	@$(OK) "arch: XSA ready: $@"
endif

arch: $(XSA)
	@$(OK) "arch: architecture ready ($(XSA))"

# ---------------------------------------------------------------------------
# Step 2: software (stage sources -> Vitis platform+app -> ELF)
# ---------------------------------------------------------------------------
$(STAGE_STAMP): FORCE
	@mkdir -p $(dir $@)
	@python3 scripts/stage.py --repo . --board $(BOARD) \
	    --stage $(STAGE) --stamp $@ \
	    --entry $(ENTRY) --entry-name "$(ENTRY_NAME)" \
	    $(SCRIPT_ARG) --opts "$(OPTS)"

ifeq ($(SKIP_SW),1)
$(ELF):
	@test -f "$@" || { echo "ERROR: SKIP_SW=1 but ELF '$(ELF)' does not exist"; exit 2; }
	@$(SKIP) "sw: skipped (SKIP_SW=1, using $(ELF))"
else
ifeq ($(FORCE_SW),1)
$(ELF): FORCE $(XSA) $(STAGE_STAMP) $(BOARD_DIR)/vitis/build_sw.py
else
$(ELF): $(XSA) $(STAGE_STAMP) $(BOARD_DIR)/vitis/build_sw.py
endif
ifeq ($(VITIS_SERVER),)
	@$(STEP) "sw: building firmware with Vitis (entry: $(ENTRY_NAME))"
	cd $(BUILD_DIR)/sw && \
	FC_WORKSPACE=$(abspath $(BUILD_DIR)/sw/ws) \
	FC_XSA=$(abspath $(XSA)) \
	FC_SRC=$(abspath $(STAGE)) \
	FC_ELF_OUT=$(abspath $@) \
	FC_PROC=$(BOARD_PROC) \
	FC_STDIO=$(BOARD_UART_STDIO) \
	$(VITIS) -s $(abspath $(BOARD_DIR)/vitis/build_sw.py)
else
	@$(STEP) "sw: building firmware on $(VITIS_USER)@$(VITIS_SERVER) (entry: $(ENTRY_NAME))"
	ssh $(VITIS_SSH_FLAGS) $(VITIS_USER)@$(VITIS_SERVER) "mkdir -p $(REMOTE_SW_DIR)"
	rsync $(VITIS_SYNC_FLAGS) -az --delete $(STAGE)/ $(VITIS_USER)@$(VITIS_SERVER):$(REMOTE_SW_DIR)/src/
	rsync $(VITIS_SYNC_FLAGS) -az $(XSA) $(VITIS_USER)@$(VITIS_SERVER):$(REMOTE_SW_DIR)/system_wrapper.xsa
	rsync $(VITIS_SYNC_FLAGS) -az $(BOARD_DIR)/vitis/build_sw.py $(VITIS_USER)@$(VITIS_SERVER):$(REMOTE_SW_DIR)/
	ssh -t $(VITIS_SSH_FLAGS) $(VITIS_USER)@$(VITIS_SERVER) 'cd $(REMOTE_SW_DIR) && FC_WORKSPACE=$$PWD/ws FC_XSA=$$PWD/system_wrapper.xsa FC_SRC=$$PWD/src FC_ELF_OUT=$$PWD/app.elf FC_PROC=$(BOARD_PROC) FC_STDIO=$(BOARD_UART_STDIO) $(REMOTE_VITIS_CMD) -s build_sw.py'
	@mkdir -p $(dir $@)
	scp -q $(VITIS_SSH_FLAGS) $(VITIS_USER)@$(VITIS_SERVER):$(REMOTE_SW_DIR)/app.elf $@
endif
	@test -f "$@"
	@$(OK) "sw: ELF ready: $@"
endif

sw: $(ELF)
	@$(OK) "sw: firmware ready ($(ELF))"

# ---------------------------------------------------------------------------
# Run: stage the run bundle, program over JTAG, capture UART -> OUTPUT.
# Exit code follows the firmware's STATUS: OK|FAIL line.
# ---------------------------------------------------------------------------
$(RUN_BUNDLE)/.stamp: $(ELF) $(BOARD_DIR)/vitis/run_jtag.tcl scripts/console_capture.py
	@mkdir -p $(RUN_BUNDLE)
	cp -f $(ELF) $(RUN_BUNDLE)/app.elf
	cp -f $(BOARD_DIR)/vitis/run_jtag.tcl $(RUN_BUNDLE)/
	cp -f scripts/console_capture.py $(RUN_BUNDLE)/
	unzip -o -j -q $(XSA) psu_init.tcl -d $(RUN_BUNDLE)
ifeq ($(PROGRAM_BIT),1)
	unzip -o -j -q $(XSA) '*.bit' -d $(RUN_BUNDLE) && \
	    mv -f $(RUN_BUNDLE)/*.bit $(RUN_BUNDLE)/system.bit
endif
	@touch $@

BIT_ARG := $(if $(filter 1,$(PROGRAM_BIT)),system.bit,)
RUN_CMD  = $(RUN_WRAPPER) $(XSCT) run_jtag.tcl app.elf psu_init.tcl $(BOARD_HW_SERVER) '$(BOARD_JTAG_CABLE_FILTER)' $(BIT_ARG)
REMOTE_RUN_CMD = $(RUN_WRAPPER) xsct run_jtag.tcl app.elf psu_init.tcl $(BOARD_HW_SERVER) '$(BOARD_JTAG_CABLE_FILTER)' $(BIT_ARG)

run-banner:
	@$(STEP) "run: BOARD=$(BOARD)  entry=$(ENTRY_NAME)  target=$(if $(BOARD_SERVER),$(BOARD_USER)@$(BOARD_SERVER),local)"
	@if [ -f "$(XSA)" ] && [ "$(FORCE_ARCH)" != "1" ]; then $(SKIP) "arch: cached ($(XSA))"; fi
	@if [ -f "$(ELF)" ] && [ "$(FORCE_SW)" != "1" ] && [ "$(FORCE_ARCH)" != "1" ]; then $(SKIP) "sw: cached (rebuilds only if sources changed)"; fi

run: run-banner $(RUN_BUNDLE)/.stamp
ifeq ($(BOARD_SERVER),)
	@$(STEP) "run: programming over JTAG and capturing UART -> $(OUTPUT)"
	@mkdir -p $(dir $(OUTPUT))
	cd $(RUN_BUNDLE) && $(NIXRUN) "python3 console_capture.py -s $(BOARD_SERIAL_PORT) -b $(BOARD_BAUD) -o $(abspath $(OUTPUT)) -t $(TIMEOUT) -- $(RUN_CMD)"
	@$(OK) "run: finished - output captured to $(OUTPUT)"
else
	@$(STEP) "run: syncing run bundle to $(BOARD_USER)@$(BOARD_SERVER)"
	ssh $(BOARD_SSH_FLAGS) $(BOARD_USER)@$(BOARD_SERVER) "mkdir -p $(REMOTE_DIR)"
	rsync $(BOARD_SYNC_FLAGS) -az --delete $(RUN_BUNDLE)/ $(BOARD_USER)@$(BOARD_SERVER):$(REMOTE_DIR)/
	@$(STEP) "run: remote JTAG program + UART capture"
	@mkdir -p $(dir $(OUTPUT))
	rc=0; ssh -t $(BOARD_SSH_FLAGS) $(BOARD_USER)@$(BOARD_SERVER) \
	    "cd $(REMOTE_DIR) && python3 console_capture.py -s $(BOARD_SERIAL_PORT) -b $(BOARD_BAUD) -o fpga_config_output.txt -t $(TIMEOUT) -- $(REMOTE_RUN_CMD)" \
	    || rc=$$?; \
	scp -q $(BOARD_SSH_FLAGS) $(BOARD_USER)@$(BOARD_SERVER):$(REMOTE_DIR)/fpga_config_output.txt $(OUTPUT) || true; \
	$(OK) "run: finished - output captured to $(OUTPUT)"; \
	exit $$rc
endif

# ---------------------------------------------------------------------------
# Inspection / housekeeping
# ---------------------------------------------------------------------------
functions:
	@echo "Available functions for BOARD=$(BOARD):"
	@for f in $(FUNC_NAMES); do \
	    if [ -f $(BOARD_DIR)/functions/$$f.c ]; then echo "  $$f (board override)"; \
	    else echo "  $$f"; fi; \
	done

board-list:
	@echo "Available boards:"
	@for b in $(notdir $(patsubst %/board.mk,%,$(wildcard boards/*/board.mk))); do echo "  $$b"; done

info:
	@echo "BOARD             = $(BOARD)"
	@echo "ENTRY             = $(ENTRY_NAME)"
	@echo "XSA               = $(XSA)  ($(if $(wildcard $(XSA)),present -> arch skipped,missing -> arch will run))"
	@echo "ELF               = $(ELF)"
	@echo "OUTPUT            = $(OUTPUT)"
	@echo "OPTS              = $(OPTS)"
	@echo "BOARD_PART        = $(BOARD_PART)"
	@echo "BOARD_PROC        = $(BOARD_PROC)"
	@echo "BOARD_SERIAL_PORT = $(BOARD_SERIAL_PORT)"
	@echo "BOARD_SERVER      = $(if $(BOARD_SERVER),$(BOARD_USER)@$(BOARD_SERVER),(local))"
	@echo "BOARD_HW_SERVER   = $(BOARD_HW_SERVER)"
	@echo "VIVADO            = $(if $(VIVADO_SERVER),remote: $(VIVADO_USER)@$(VIVADO_SERVER),$(VIVADO))"
	@echo "VITIS             = $(if $(VITIS_SERVER),remote: $(VITIS_USER)@$(VITIS_SERVER),$(VITIS))"

clean:
ifeq ($(CLEAN_ALL),1)
	rm -rf $(BUILD_FOLDER)
else
	rm -rf $(BUILD_DIR)
endif

FORCE:
