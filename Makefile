# FPGA-Configurator - single user entry point.
# All CLI targets/variables are documented in docs/CLI.md (keep in lockstep).

BOARD ?= zcu104

-include local.mk

ifeq ($(wildcard boards/$(BOARD)/board.mk),)
$(error unknown BOARD '$(BOARD)'. Available: $(notdir $(patsubst %/board.mk,%,$(wildcard boards/*/board.mk))))
endif
include boards/$(BOARD)/board.mk

BOARD_DIR   := boards/$(BOARD)
BUILD_DIR   := build/$(BOARD)
XSA         ?= $(BUILD_DIR)/arch/system_wrapper.xsa
ELF         := $(BUILD_DIR)/sw/app.elf
OUTPUT      ?= $(BUILD_DIR)/fpga_config_output.txt
STAGE       := $(BUILD_DIR)/sw/src
STAGE_STAMP := $(BUILD_DIR)/sw/.stage.stamp
RUN_BUNDLE  := $(BUILD_DIR)/run_bundle
TIMEOUT     ?= 120
USE_NIX     ?= 1
ARCH_FAST   ?= 0
PROGRAM_BIT ?= 0
RUN_WRAPPER ?=
REMOTE_DIR  := fpga-configurator/$(BOARD)

# ---------------------------------------------------------------------------
# Tools: Vivado/Vitis/xsct from the system install (never from nix).
# Point XILINX_VIVADO / XILINX_VITIS at the install roots (see local.mk).
# ---------------------------------------------------------------------------
VIVADO := $(if $(XILINX_VIVADO),$(XILINX_VIVADO)/bin/vivado,vivado)
VITIS  := $(if $(XILINX_VITIS),$(XILINX_VITIS)/bin/vitis,vitis)
XSCT   := $(if $(XILINX_VITIS),$(XILINX_VITIS)/bin/xsct,xsct)

# Host-side python steps run inside nix-shell unless USE_NIX=0
ifeq ($(USE_NIX),1)
NIXRUN = nix-shell --run
else
NIXRUN = sh -c
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

.PHONY: all arch sw run functions board-list info clean FORCE
.DEFAULT_GOAL := all

all: run

# ---------------------------------------------------------------------------
# Step 1: architecture (Vivado -> XSA). XSA= names the file for both reading
# and writing: exists -> skipped; missing -> generated at that path.
# ---------------------------------------------------------------------------
ifeq ($(SKIP_ARCH),1)
$(XSA):
	@test -f "$@" || { echo "ERROR: SKIP_ARCH=1 but architecture '$(XSA)' does not exist"; exit 2; }
else
ifeq ($(FORCE_ARCH),1)
$(XSA): FORCE
else
$(XSA): $(BOARD_DIR)/vivado/gen_arch.tcl $(BOARD_DIR)/vivado/ps_config.tcl
endif
	@mkdir -p $(dir $@)
	cd $(dir $@) && $(VIVADO) -nojournal -log vivado_arch.log -mode batch \
	    -source $(abspath $(BOARD_DIR)/vivado/gen_arch.tcl) \
	    -tclargs $(abspath $@) $(BOARD_PART) $(ARCH_FAST) $(abspath $(BOARD_DIR)/vivado/ps_config.tcl)
	@test -f "$@"
endif

arch: $(XSA)

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
else
$(ELF): $(XSA) $(STAGE_STAMP) $(BOARD_DIR)/vitis/build_sw.py
	FC_WORKSPACE=$(abspath $(BUILD_DIR)/sw/ws) \
	FC_XSA=$(abspath $(XSA)) \
	FC_SRC=$(abspath $(STAGE)) \
	FC_ELF_OUT=$(abspath $@) \
	FC_PROC=$(BOARD_PROC) \
	FC_STDIO=$(BOARD_UART_STDIO) \
	$(VITIS) -s $(BOARD_DIR)/vitis/build_sw.py
	@test -f "$@"
endif

sw: $(ELF)

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

run: $(RUN_BUNDLE)/.stamp
ifeq ($(BOARD_SERVER),)
	@mkdir -p $(dir $(OUTPUT))
	cd $(RUN_BUNDLE) && $(NIXRUN) "python3 console_capture.py -s $(BOARD_SERIAL_PORT) -b $(BOARD_BAUD) -o $(abspath $(OUTPUT)) -t $(TIMEOUT) -- $(RUN_CMD)"
	@echo "Output captured to $(OUTPUT)"
else
	ssh $(BOARD_SSH_FLAGS) $(BOARD_USER)@$(BOARD_SERVER) "mkdir -p $(REMOTE_DIR)"
	rsync $(BOARD_SYNC_FLAGS) -az --delete $(RUN_BUNDLE)/ $(BOARD_USER)@$(BOARD_SERVER):$(REMOTE_DIR)/
	@mkdir -p $(dir $(OUTPUT))
	rc=0; ssh -t $(BOARD_SSH_FLAGS) $(BOARD_USER)@$(BOARD_SERVER) \
	    "cd $(REMOTE_DIR) && python3 console_capture.py -s $(BOARD_SERIAL_PORT) -b $(BOARD_BAUD) -o fpga_config_output.txt -t $(TIMEOUT) -- $(REMOTE_RUN_CMD)" \
	    || rc=$$?; \
	scp -q $(BOARD_SSH_FLAGS) $(BOARD_USER)@$(BOARD_SERVER):$(REMOTE_DIR)/fpga_config_output.txt $(OUTPUT) || true; \
	echo "Output captured to $(OUTPUT)"; \
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
	@echo "VIVADO            = $(VIVADO)"
	@echo "VITIS             = $(VITIS)"

clean:
ifeq ($(CLEAN_ALL),1)
	rm -rf build
else
	rm -rf $(BUILD_DIR)
endif

FORCE:
