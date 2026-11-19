#!/bin/bash
# Video Driver Cache Manager
# Handles cache initialization, validation, and cleanup
# SPDX-License-Identifier: GPL-2.0-only

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/build.conf"

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Logging functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $*"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $*"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $*"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $*" >&2
}

# Initialize cache directories
init_cache_dirs() {
    log_info "Initializing cache directories..."
    
    mkdir -p "${OBJECT_CACHE_DIR}" "${MODULE_CACHE_DIR}" "${CCACHE_DIR}" \
             "${DL_DIR}" "${METADATA_DIR}" "${BUILD_LOG_DIR}"
    
    # Set proper permissions
    chmod 755 "${BUILD_CACHE_DIR}"
    chmod 755 "${OBJECT_CACHE_DIR}" "${MODULE_CACHE_DIR}" "${CCACHE_DIR}"
    
    log_success "Cache directories initialized"
}

# Validate cache integrity
validate_cache() {
    log_info "Validating cache integrity..."
    
    local cache_valid=1
    
    # Check cache directory structure
    if [[ ! -d "${OBJECT_CACHE_DIR}" ]]; then
        log_error "Object cache directory missing: ${OBJECT_CACHE_DIR}"
        cache_valid=0
    fi
    
    if [[ ! -d "${MODULE_CACHE_DIR}" ]]; then
        log_error "Module cache directory missing: ${MODULE_CACHE_DIR}"
        cache_valid=0
    fi
    
    # Verify cache manifest if it exists
    if [[ -f "${METADATA_DIR}/cache-manifest.json" ]]; then
        if ! command -v jq &> /dev/null; then
            log_warn "jq not found, skipping manifest validation"
        else
            if ! jq empty "${METADATA_DIR}/cache-manifest.json" 2>/dev/null; then
                log_error "Cache manifest is corrupted"
                cache_valid=0
            fi
        fi
    fi
    
    if [[ $cache_valid -eq 1 ]]; then
        log_success "Cache validation passed"
        return 0
    else
        log_error "Cache validation failed"
        return 1
    fi
}

# Generate build signature
generate_build_signature() {
    log_info "Generating build signature..."
    
    local signature_file="${BUILD_SIGNATURE_FILE}"
    local kernel_version=$(cat "${KERNEL_SRC}/include/config/kernel.release" 2>/dev/null || echo "unknown")
    local video_driver_hash=$(find "${VIDEO_ROOT}" -type f \( -name "*.c" -o -name "*.h" -o -name "Makefile" \) -exec md5sum {} \; 2>/dev/null | md5sum | cut -d' ' -f1)
    local timestamp=$(date -u +"%Y-%m-%dT%H:%M:%SZ")
    
    cat > "${signature_file}" <<EOF
{
  "timestamp": "${timestamp}",
  "kernel_version": "${kernel_version}",
  "video_driver_hash": "${video_driver_hash}",
  "cache_version": "1.0"
}
EOF
    
    log_success "Build signature generated: ${signature_file}"
}

# Calculate dependency hash
calculate_dependency_hash() {
    log_info "Calculating dependency hash..."
    
    local dep_hash_file="${DEPENDENCY_HASH_FILE}"
    
    # Hash all source files, headers, and build configuration
    local dep_hash=$(find "${VIDEO_ROOT}" \
        -type f \( -name "*.c" -o -name "*.h" -o -name "Makefile" -o -name "Kbuild" \) \
        -exec md5sum {} \; 2>/dev/null | sort | md5sum | cut -d' ' -f1)
    
    echo "${dep_hash}" > "${dep_hash_file}"
    
    log_success "Dependency hash calculated: ${dep_hash}"
}

# Monitor disk space
monitor_disk_space() {
    if [[ "${MONITOR_DISK_SPACE}" != "1" ]]; then
        return 0
    fi
    
    log_info "Monitoring disk space..."
    
    local cache_mount=$(df "${BUILD_CACHE_DIR}" | tail -1 | awk '{print $1}')
    local available_space=$(df "${BUILD_CACHE_DIR}" | tail -1 | awk '{print $4 * 1024}')
    
    if [[ ${available_space} -lt ${MIN_FREE_SPACE_HALT} ]]; then
        log_error "CRITICAL: Disk space below halt threshold (${MIN_FREE_SPACE_HALT} bytes)"
        log_error "Available space: ${available_space} bytes on ${cache_mount}"
        return 1
    fi
    
    if [[ ${available_space} -lt ${MIN_FREE_SPACE_STOPTASKS} ]]; then
        log_warn "WARNING: Disk space below stop-tasks threshold (${MIN_FREE_SPACE_STOPTASKS} bytes)"
        log_warn "Available space: ${available_space} bytes on ${cache_mount}"
        log_warn "Consider running cache cleanup"
    fi
    
    log_success "Disk space check passed (${available_space} bytes available)"
    return 0
}

# Cleanup old cache entries
cleanup_cache() {
    log_info "Cleaning up cache entries older than ${CACHE_RETENTION_DAYS} days..."
    
    if [[ "${AUTO_CLEANUP_ENABLED}" != "1" ]]; then
        log_warn "Auto-cleanup is disabled"
        return 0
    fi
    
    local deleted_count=0
    
    # Remove old object files
    while IFS= read -r file; do
        rm -f "${file}"
        ((deleted_count++))
    done < <(find "${OBJECT_CACHE_DIR}" -type f -mtime "+${CACHE_RETENTION_DAYS}" 2>/dev/null || true)
    
    # Remove old module files
    while IFS= read -r file; do
        rm -f "${file}"
        ((deleted_count++))
    done < <(find "${MODULE_CACHE_DIR}" -type f -mtime "+${CACHE_RETENTION_DAYS}" 2>/dev/null || true)
    
    # Remove empty directories
    find "${BUILD_CACHE_DIR}" -type d -empty -delete 2>/dev/null || true
    
    log_success "Cleanup completed: ${deleted_count} files removed"
}

# Generate cache manifest
generate_cache_manifest() {
    log_info "Generating cache manifest..."
    
    local manifest_file="${METADATA_DIR}/cache-manifest.json"
    local object_count=$(find "${OBJECT_CACHE_DIR}" -type f 2>/dev/null | wc -l)
    local module_count=$(find "${MODULE_CACHE_DIR}" -type f 2>/dev/null | wc -l)
    local cache_size=$(du -sh "${BUILD_CACHE_DIR}" 2>/dev/null | cut -f1)
    local timestamp=$(date -u +"%Y-%m-%dT%H:%M:%SZ")
    
    cat > "${manifest_file}" <<EOF
{
  "timestamp": "${timestamp}",
  "cache_version": "1.0",
  "statistics": {
    "object_files": ${object_count},
    "module_files": ${module_count},
    "total_size": "${cache_size}"
  },
  "directories": {
    "object_cache": "${OBJECT_CACHE_DIR}",
    "module_cache": "${MODULE_CACHE_DIR}",
    "ccache": "${CCACHE_DIR}",
    "downloads": "${DL_DIR}"
  }
}
EOF
    
    log_success "Cache manifest generated: ${manifest_file}"
}

# Main command dispatcher
main() {
    case "${1:-help}" in
        init)
            init_cache_dirs
            generate_build_signature
            calculate_dependency_hash
            generate_cache_manifest
            ;;
        validate)
            validate_cache
            ;;
        monitor)
            monitor_disk_space
            ;;
        cleanup)
            cleanup_cache
            ;;
        manifest)
            generate_cache_manifest
            ;;
        status)
            log_info "Cache Status Report"
            log_info "===================="
            log_info "Cache Base: ${CACHE_BASE_DIR}"
            log_info "Object Cache: $(find "${OBJECT_CACHE_DIR}" -type f 2>/dev/null | wc -l) files"
            log_info "Module Cache: $(find "${MODULE_CACHE_DIR}" -type f 2>/dev/null | wc -l) files"
            log_info "Total Size: $(du -sh "${BUILD_CACHE_DIR}" 2>/dev/null | cut -f1)"
            ;;
        *)
            echo "Usage: $0 {init|validate|monitor|cleanup|manifest|status}"
            echo ""
            echo "Commands:"
            echo "  init       - Initialize cache directories and metadata"
            echo "  validate   - Validate cache integrity"
            echo "  monitor    - Monitor disk space"
            echo "  cleanup    - Remove old cache entries"
            echo "  manifest   - Generate cache manifest"
            echo "  status     - Display cache status"
            exit 1
            ;;
    esac
}

main "$@"