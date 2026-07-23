#!/usr/bin/env bash
################################################################################
#
# capture-screenshots.sh
# ----------------
# Automates taking screen captures of LVGL screens on the CYD Photo Frame.
#
################################################################################

set -e
set -o pipefail

# Constants
readonly BLUE=$(tput setaf 4)
readonly RED=$(tput setaf 1)
readonly YELLOW=$(tput setaf 3)
readonly RESET=$(tput sgr0)

function log() {
  local type="$1"
  local message="$2"
  local color="$RESET"

  case "$type" in
    INFO) color="$BLUE";;
    WARN) color="$YELLOW";;
    ERRO) color="$RED";;
  esac

  echo -e "${color}${type}${RESET}[$(date +'%Y-%m-%d %H:%M:%S')] ${message}"
}

function commandExists() {
  command -v "$1" >/dev/null 2>&1
}

function check_dependencies() {
  if ! commandExists curl ; then
    log "ERRO" "Required dependency (curl) is not installed." >&2
    exit 1
  fi
}

function set_orientation() {
  local ip="$1"
  local rot="$2"
  local rot_name="$3"
  local err_msg

  log "INFO" "Setting orientation to ${rot_name} (val=${rot})..."
  if ! err_msg=$(curl -sS -m 5 -d "val=${rot}" "http://${ip}/api/orientation" 2>&1); then
    log "ERRO" "Connection failed: ${err_msg}" >&2
    exit 1
  fi
  log "WARN" "The device is now rebooting to apply the orientation."
  log "WARN" "Waiting 15 seconds for the device to reconnect to WiFi..."
  sleep 15
}

function capture_screen() {
  local ip="$1"
  local rot_name="$2"
  local screen="$3"
  local out_file="screenshots/${rot_name}_${screen}.bmp"
  local err_msg

  log "INFO" "Switching to screen: ${screen}..."
  if ! err_msg=$(curl -sS -m 5 -d "index=${screen}" "http://${ip}/api/screen" 2>&1); then
    log "ERRO" "Failed to change screen: ${err_msg}" >&2
    exit 1
  fi
  sleep 2

  log "INFO" "Capturing screenshot to ${out_file}..."
  mkdir -p screenshots
  if err_msg=$(curl -sS -f -m 15 "http://${ip}/screenshot" -o "${out_file}" 2>&1); then
    log "INFO" "Successfully saved ${out_file}."
    sleep 2
  else
    log "ERRO" "Screenshot capture failed: ${err_msg}" >&2
    exit 1
  fi
}

function capture_all_screens() {
  local ip="$1"
  local rot_name="$2"
  capture_screen "${ip}" "${rot_name}" "settings"
  capture_screen "${ip}" "${rot_name}" "ap"
  capture_screen "${ip}" "${rot_name}" "sd_error"
  capture_screen "${ip}" "${rot_name}" "warn"
  capture_screen "${ip}" "${rot_name}" "opt"
  capture_screen "${ip}" "${rot_name}" "clear_cache"
}

function main() {
  check_dependencies

  local ip=""
  local auto_flip=0

  # Parse arguments
  while [[ $# -gt 0 ]]; do
    case $1 in
      --flip)
        auto_flip=1
        shift
        ;;
      *)
        if [ -z "$ip" ]; then
          ip="$1"
        else
          log "ERRO" "Unknown argument: $1" >&2
          exit 1
        fi
        shift
        ;;
    esac
  done

  if [ -z "${ip}" ]; then
    log "ERRO" "Usage: ./scripts/capture-screenshots.sh <DEVICE_IP> [--flip]" >&2
    exit 1
  fi

  log "INFO" "Starting screenshot capture from device ${ip}..."

  if [ "$auto_flip" -eq 1 ]; then
    # 1. Capture Landscape screens (rotation 1)
    set_orientation "${ip}" 1 "landscape"
    capture_all_screens "${ip}" "landscape"

    # 2. Capture Portrait screens (rotation 2)
    set_orientation "${ip}" 2 "portrait"
    capture_all_screens "${ip}" "portrait"

    # 3. Restore to Landscape
    log "INFO" "Restoring default Landscape orientation..."
    set_orientation "${ip}" 1 "landscape"
  else
    log "INFO" "Capturing current orientation only. Use --flip to automatically switch and capture both."
    capture_all_screens "${ip}" "current"
  fi

  # Automatically convert BMP to PNG if uv is installed
  if commandExists uv && [ -f "./scripts/convert-screenshots.py" ]; then
    log "INFO" "Converting captured BMPs to PNG format..."
    uv run ./scripts/convert-screenshots.py || log "WARN" "Automatic PNG conversion failed."
  fi

  log "INFO" "Screenshots automation process finished successfully."
}

main "$@"
