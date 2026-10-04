# Porpoise - build entry points (Linux / WSL).
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Derived from ps5-native-app-boilerplate (Copyright (C) 2026 BlackBearReloaded)
# by way of Mihawk's PS5 RetroArch, both GPL-3.0-or-later.
#
#   make porpoise   the whole title: the Dolphin core, eboot.bin and the zip
#                   (needs PS5_VULKAN_DIR; see BUILDING.md)
#   make app        eboot.bin and the title folder only (what build-porpoise.sh runs)
#   make libc       regenerate runtime/libc.prx
#   make clean      remove build/ and dist/

SHELL := /bin/bash
.DEFAULT_GOAL := porpoise

-include .env

APP_DEFINITIONS ?=
APP_INCLUDE_PATHS ?=
APP_STATIC_ARCHIVES ?=
APP_SDK_ARCHIVES ?=
APP_RUNTIME_MODULES ?=
PACBREW_PACKAGES ?=
PACBREW_INCLUDE_PATHS ?=
PACBREW_STATIC_ARCHIVES ?=
TITLE_ID ?=
APP_NAME ?=
APP_CATEGORY ?= game
CONTENT_SUFFIX ?=
export APP_DEFINITIONS APP_INCLUDE_PATHS APP_STATIC_ARCHIVES APP_SDK_ARCHIVES APP_RUNTIME_MODULES
export PACBREW_PACKAGES PACBREW_INCLUDE_PATHS PACBREW_STATIC_ARCHIVES
export TITLE_ID APP_NAME APP_CATEGORY CONTENT_SUFFIX

RUNTIME := runtime/libc.prx
RUNTIME_INPUTS := tools/rebuild-libc.sh \
	$(wildcard tooling/native/*.cpp tooling/native/*.hpp) \
	$(wildcard tooling/native/runtime/*.txt)

.PHONY: all porpoise app libc deps clean distclean help

all: porpoise

porpoise:
	@bash tools/build-porpoise.sh

deps:
	@bash tools/setup-native-dependencies.sh

libc:
	@bash tools/rebuild-libc.sh

$(RUNTIME): $(RUNTIME_INPUTS)
	@printf '%s\n' '==> [libc] Generating the missing or outdated runtime'
	@bash tools/rebuild-libc.sh

app: $(RUNTIME)
	@printf '%s\n' '==> [app] Compiling, linking, signing, and assembling the app folder'
	@bash tools/build.sh Folder

clean:
	@rm -rf -- build dist
	@rm -f -- $(RUNTIME)

distclean: clean
	@rm -rf -- .deps

help:
	@printf '%s\n' \
	  'make / make porpoise  Build the Dolphin core and the title, then dist/Porpoise-PPSA99764.zip' \
	  'make app              Build eboot.bin and the title folder only' \
	  'make deps             Fetch the PS5 payload SDK into .deps/' \
	  'make libc             Regenerate runtime/libc.prx' \
	  'make clean            Remove build/ and dist/' \
	  'make distclean        Also remove the .deps/ cache'
