# Video Driver DKMS Overlay

This overlay directory contains all scripts and configuration files needed to create Ubuntu DKMS packages for video-driver with intelligent automatic driver management and recovery capabilities.

## Directory Structure

```
overlay/
├── debian/                       # Debian package configuration files
│   ├── control                   # Package control file with automatic driver management
│   ├── rules                     # Build rules
│   ├── changelog                 # Change log
│   ├── copyright                 # Copyright information
│   ├── postinst                  # Post-installation script (automatic driver switching + recovery)
│   ├── prerm                     # Pre-removal script (intelligent cleanup with overlay detection)
│   ├── postrm                    # Post-removal script (driver restoration)
│   └── video-driver-dkms.install # Installation file list
├── scripts/                      # Build and utility scripts
│   ├── detect-platform.sh        # Platform detection script
│   ├── set-build-env.sh          # Build environment setup
│   ├── dkms-build-wrapper.sh     # DKMS build wrapper (with integrated apport fix)
│   ├── build-wrapper.sh          # Build wrapper script
│   └── cross-compile.sh          # Cross-compilation development script
├── dkms.conf                     # DKMS configuration file (with apport fixes)
├── build-package.sh              # Debian package build script
├── cleanup.sh                    # Cleanup and uninstall script
├── build/                        # Build output directory
└── README.md                     # This file
```

## Key Features

### **🚀 Intelligent Automatic Recovery System**
- **DKMS Failure Detection**: Automatically detects DKMS apport errors and build failures
- **Overlay Installation**: When DKMS fails, automatically performs manual recovery installation
- **Smart Cleanup**: Uses overlay flag system to distinguish between DKMS and manual installations
- **Zero User Intervention**: Handles common DKMS issues transparently

### **🛡️ Advanced Driver Management**
- **Safe Driver Switching**: Only disables qcom-iris if video-driver successfully builds and loads
- **Automatic Rollback**: Restores qcom-iris if video-driver fails at any stage
- **Hardware Conflict Prevention**: Properly unloads conflicting drivers before switching
- **State Preservation**: Remembers original driver state for accurate restoration

### **🔧 Custom Kernel Support**
- **Development Kernel Compatibility**: Handles kernels with "-dirty" and "rc" versions
- **Apport Error Mitigation**: Built-in fixes for common DKMS apport issues
- **Compiler Mismatch Handling**: Automatically handles compiler version mismatches

## Supported Platforms

Automatically detect and enable corresponding configuration based on device tree compatible strings (iris format only):

| Compatible String | Platform | Configuration |
|-------------------|----------|---------------|
| `qcom,x1e80100-iris` | HAMOA | X1E80100 platform support |
| `qcom,sa8775p-iris` | LEMANS | SA8775P platform support |

## Usage

### 1. Build DKMS Debian Package

```bash
cd overlay
./build-package.sh
```

After build completion, package files will be available in `overlay/build/` directory.

### 2. Install DKMS Package

```bash
# Install dependencies
sudo apt update
sudo apt install -y dkms linux-headers-$(uname -r)

# Install generated package
sudo dpkg -i package (video-driver debian package)

# If dependency issues occur, run:
sudo apt -f install -y
```

### 3. Verify Installation

```bash
# Check DKMS status
sudo dkms status

# Check if module is loaded
lsmod | grep iris_vpu

# View module information
modinfo iris_vpu

# Check installation method
ls -la /var/lib/dkms/video-driver-overlay.flag  # Exists if overlay recovery was used
```

### 4. Uninstall and Cleanup

**Recommended method using cleanup script:**

```bash
# View current status
./cleanup.sh --status

# Complete uninstall (recommended)
./cleanup.sh --clean-all

# Uninstall DKMS package and modules only
./cleanup.sh --uninstall-dkms

# Clean build artifacts only
./cleanup.sh --clean-build
```

**Manual uninstall method:**

```bash
# Uninstall DKMS package (automatically restores qcom-iris driver)
sudo dpkg -r video-driver-dkms

# Or complete purge (removes configuration files)
sudo dpkg --purge video-driver-dkms

# Update module dependencies
sudo depmod -a
```

## Automatic Recovery System

### **How It Works**

The package includes an intelligent recovery system that handles DKMS failures automatically:

1. **Normal DKMS Build**: Attempts standard DKMS build process
2. **Failure Detection**: Detects DKMS apport errors or build failures
3. **Automatic Recovery**:
   - Checks if module was actually built despite DKMS error
   - Creates overlay installation flag (`/var/lib/dkms/video-driver-overlay.flag`)
   - Manually installs module to `/lib/modules/$(uname -r)/updates/dkms/`
   - Updates module dependencies and tests loading
4. **Smart Cleanup**: During uninstall, detects overlay flag and performs appropriate cleanup

### **Recovery Process Details**

```bash
# When DKMS fails, the system automatically:
# 1. Checks for built module
ls -l /var/lib/dkms/video-driver/1.0.0/build/video/iris_vpu.ko

# 2. Creates target directory (if needed)
mkdir -p /lib/modules/$(uname -r)/updates/dkms

# 3. Installs module manually
cp /var/lib/dkms/video-driver/1.0.0/build/video/iris_vpu.ko /lib/modules/$(uname -r)/updates/dkms/

# 4. Updates dependencies
depmod -a

# 5. Tests module loading
modprobe iris_vpu

# 6. Creates overlay flag
touch /var/lib/dkms/video-driver-overlay.flag
```

### **Installation Scenarios**

**Scenario 1: DKMS Success**
- Standard DKMS installation
- Module installed to `/lib/modules/$(uname -r)/updates/dkms/iris_vpu.ko`
- No overlay flag created
- Standard DKMS cleanup during uninstall

**Scenario 2: DKMS Failure + Successful Recovery**
- DKMS build fails with apport error
- Module actually built successfully
- Overlay recovery installs module manually
- Overlay flag created: `/var/lib/dkms/video-driver-overlay.flag`
- Smart cleanup during uninstall removes both module and flag

**Scenario 3: Complete Build Failure**
- DKMS build fails and no module built
- Recovery detects missing module file
- Installation fails gracefully
- qcom_iris driver remains active (system stability preserved)

## Advanced Driver Management

### **Smart Driver Switching Logic**

The package implements sophisticated driver management:

```bash
# Installation Process:
1. Save qcom_iris state for rollback
2. Build video-driver module (with automatic recovery)
3. Only if build succeeds:
   - Unload qcom_iris module
   - Add qcom_iris to blacklist
   - Load video-driver module
   - Verify module is working
4. If any step fails: automatic rollback to qcom_iris

# Removal Process:
1. Unload video-driver module
2. Detect installation method (DKMS vs overlay)
3. Clean up appropriately:
   - DKMS: Use standard DKMS removal
   - Overlay: Manual file cleanup + flag removal
4. Remove qcom_iris blacklist
5. Restore qcom_iris module
6. Update initramfs
```

### **Safety Features**

- **No System Breakage**: Always ensures at least one video driver is available
- **Intelligent Fallback**: Automatically handles build failures and hardware incompatibilities
- **State Preservation**: Remembers original driver state for accurate restoration
- **Detailed Logging**: Provides clear status messages for troubleshooting
- **Overlay Detection**: Distinguishes between installation methods for proper cleanup

## Cross-compilation Support

System automatically detects if cross-compilation is needed:

- If host architecture is not aarch64, automatically setup ARM64 cross-compilation
- Automatically find available cross compilers:
  - `aarch64-linux-gnu-`
  - `aarch64-none-linux-gnu-`
  - `arm64-linux-gnu-`

### Install Cross Compiler

```bash
# Ubuntu/Debian
sudo apt install gcc-aarch64-linux-gnu

# Verify installation
aarch64-linux-gnu-gcc --version
```

## Development and Testing

### Cross-compilation Development Script

Use the cross-compilation script for development testing:

```bash
# Auto-detect platform and cross-compile
./scripts/cross-compile.sh

# Specify platform
./scripts/cross-compile.sh --compatible qcom,sa8775p-iris

# Full configuration
./scripts/cross-compile.sh \
    --compatible qcom,x1e80100-iris \
    --arch arm64 \
    --cross-compile aarch64-linux-gnu- \
    --kernel-src /path/to/kernel \
    --output-dir /path/to/output

# Clean build artifacts
./scripts/cross-compile.sh --clean

# View help
./scripts/cross-compile.sh --help
```


## Build Process

### DKMS Package Build Process

1. `build-package.sh` copies configuration files to video-driver root directory
2. `dpkg-buildpackage` builds debian package with maintainer scripts
3. DKMS configuration and scripts are packaged into Debian package file
4. Clean temporary files, keep package in overlay/build directory

### DKMS Installation Build Process

1. DKMS installs source to `/usr/src/video-driver-1.0.0/`
2. Call `scripts/dkms-build-wrapper.sh`
3. `detect-platform.sh` detects compatible string from device tree
4. `set-build-env.sh` sets environment variables and cross-compilation based on compatible
5. Execute kernel module build with apport error handling
6. **Automatic Recovery**: If DKMS fails, attempt overlay installation
7. **Smart Driver Management**: Only switch drivers if build and load succeed

## Common Package Management Commands

### **Installation Commands**
```bash
# Install specific version
sudo dpkg -i package (video-driver debian package)

# Fix dependency issues after installation
sudo apt -f install -y
```

### **Removal Commands**
```bash
# Remove package (keeps configuration files)
sudo dpkg -r video-driver-dkms

# Complete purge (removes all files including configuration)
sudo dpkg --purge video-driver-dkms

# Force removal if package is in broken state
sudo dpkg --force-remove-reinstreq --purge video-driver-dkms
```

### **Package Information Commands**
```bash
# List package files
sudo dpkg -L video-driver-dkms

# Check package status
dpkg -s video-driver-dkms

# List all installed packages matching pattern
dpkg -l | grep video-driver
```

### **DKMS Management Commands**
```bash
# Remove DKMS module completely
sudo dkms remove video-driver/1.0.0 --all

# Check DKMS status
sudo dkms status

# Build DKMS module manually
sudo dkms build video-driver/1.0.0

# Install DKMS module manually
sudo dkms install video-driver/1.0.0
```

## Troubleshooting

### 1. Check Installation Status

```bash
# Check package status
dpkg -l | grep video-driver

# Check module status
lsmod | grep -E "(iris_vpu|qcom_iris)"

# Check DKMS status
dkms status | grep video-driver

# Check installation method
ls -la /var/lib/dkms/video-driver-overlay.flag  # Overlay recovery used if exists

# Check blacklist status
grep qcom_iris /etc/modprobe.d/blacklist.conf
```

### 2. Common Installation Scenarios

**Package Status Meanings:**
- `ii video-driver-dkms` - Successfully installed and configured
- `pi video-driver-dkms` - Package marked for purge but still installed (partial removal)
- `rc video-driver-dkms` - Package removed but configuration files remain

**Driver Status Scenarios:**

*Successful Standard Installation:*
```bash
lsmod | grep iris_vpu        # Should show iris_vpu loaded
lsmod | grep qcom_iris       # Should show nothing (unloaded)
ls /var/lib/dkms/video-driver-overlay.flag  # Should not exist
```

*Successful Overlay Recovery Installation:*
```bash
lsmod | grep iris_vpu        # Should show iris_vpu loaded
lsmod | grep qcom_iris       # Should show nothing (unloaded)
ls /var/lib/dkms/video-driver-overlay.flag  # Should exist
```

*Failed Installation (System Protected):*
```bash
lsmod | grep iris_vpu        # Should show nothing
lsmod | grep qcom_iris       # Should show qcom_iris loaded (preserved)
```

### 3. Manual Recovery Commands

If automatic recovery fails, you can perform manual recovery:

```bash
# Check if module was built
ls -la /var/lib/dkms/video-driver/1.0.0/build/video/iris_vpu.ko

# Manual installation steps
sudo mkdir -p /lib/modules/$(uname -r)/updates/dkms
sudo cp /var/lib/dkms/video-driver/1.0.0/build/video/iris_vpu.ko /lib/modules/$(uname -r)/updates/dkms/
sudo depmod -a
sudo modprobe iris_vpu

# Create overlay flag for proper cleanup
sudo touch /var/lib/dkms/video-driver-overlay.flag
```

### 4. Custom Kernel Issues

**Common Errors:**

*DKMS Apport Error (Automatically Handled):*
```
ERROR (dkms apport): kernel package linux-headers-6.19.0-rc5-g6658049e667c-dirty is not supported
```
**Solution**: The system automatically detects this and performs overlay recovery.

*Build Dependency Error:*
```
Error! Build of ./iris_vpu.ko failed for: 6.19.0-rc5-g6658049e667c-dirty (aarch64)
```
**Solution**: Check build logs and ensure kernel headers are available:
```bash
# Check kernel headers
ls -la /lib/modules/$(uname -r)/build

# Install if missing
sudo apt install linux-headers-$(uname -r)

# View build logs
cat /var/lib/dkms/video-driver/1.0.0/build/make.log
```

### 5. Platform Detection Issues

```bash
# Test platform detection
./scripts/detect-platform.sh

# Check device tree manually
find /proc/device-tree -name "*video*" -o -name "*vidc*"

# Override platform detection
PLATFORM_OVERRIDE=qcom,sa8775p-iris ./scripts/detect-platform.sh
```

### 6. Uninstall Issues

**Package Stuck in 'pi' State:**
```bash
# Force complete removal
sudo dpkg --force-remove-reinstreq --purge video-driver-dkms

# Manual cleanup if needed
sudo rm -f /var/lib/dkms/video-driver-overlay.flag
sudo rm -f /lib/modules/$(uname -r)/updates/dkms/iris_vpu.ko
sudo depmod -a
```

**Binary File Grep Errors (Fixed in Latest Version):**
```
grep: /etc/modprobe.d/blacklist.conf: binary file matches
```
**Solution**: This is automatically handled in the latest prerm script with `-a` flag and error suppression.

## Testing and Verification

### Manual Testing Workflow

```bash
# 1. Check initial system status
lsmod | grep -E "(iris_vpu|qcom_iris)"
dkms status | grep video-driver
dpkg -l | grep video-driver

# 2. Build and install package
./build-package.sh
sudo dpkg -i package (video-driver debian package)

# 3. Verify installation result
lsmod | grep -E "(iris_vpu|qcom_iris)"
dkms status | grep video-driver
dpkg -l | grep video-driver
ls -la /var/lib/dkms/video-driver-overlay.flag  # Check installation method

# 4. Test removal and restoration
sudo dpkg -r video-driver-dkms

# 5. Verify qcom_iris restoration
lsmod | grep -E "(iris_vpu|qcom_iris)"
```

### Expected Behavior

**Successful Installation:**
- iris_vpu module loaded and working
- qcom_iris module unloaded and blacklisted
- Package status: `ii` (installed and configured)
- Overlay flag may or may not exist depending on installation method

**Failed Installation (build failure):**
- qcom_iris module remains active (system stability preserved)
- No blacklist entries for qcom_iris
- Package status: `ii` but iris_vpu not functional

**Successful Removal:**
- iris_vpu module unloaded and removed
- qcom_iris module restored and active
- Blacklist entries removed
- Overlay flag cleaned up (if existed)

## Technical Details

### **DKMS Configuration Features:**
- `NO_WEAK_MODULES="yes"` - Disables apport error reporting for custom kernels
- `REMAKE_INITRD="no"` - Skips initrd regeneration to avoid issues
- `DEST_MODULE_LOCATION[0]="/updates/dkms"` - Unified installation path

### **Environment Variables (set automatically):**
- `DKMS_DISABLE_APPORT=1` - Disables DKMS error reporting for custom kernels
- `IGNORE_CC_MISMATCH=1` - Ignores compiler version mismatches

### **Overlay Flag System:**
- **Flag File**: `/var/lib/dkms/video-driver-overlay.flag`
- **Purpose**: Marks installations that used overlay recovery
- **Cleanup**: Automatically removed during uninstall
- **Detection**: Used by prerm script to determine cleanup method

### **Driver Management Features:**
- Intelligent build validation before driver switching
- Automatic rollback on failure
- Hardware resource conflict prevention
- Blacklist management with initramfs integration
- Overlay installation detection and cleanup

## Notes

1. **Non-intrusive**: overlay system will not modify any existing files in video-driver directory
2. **Build isolation**: all build artifacts are in overlay directory
3. **Temporary files**: during build process, debian/, dkms.conf and scripts/ will be temporarily created in root directory, automatically cleaned after build completion
4. **Platform detection**: must detect compatible string from device tree, no default values supported (iris format only)
5. **Cross-compilation**: all platforms use unified ARM64 cross-compilation configuration
6. **Smart driver management**: ensures system stability by maintaining at least one working video driver
7. **Automatic recovery**: prevents system breakage by handling DKMS failures transparently
8. **Overlay detection**: distinguishes between installation methods for proper cleanup
9. **Path consistency**: All module installations use `/lib/modules/$(uname -r)/updates/dkms/` for consistency

## File Permissions

Ensure scripts have execute permissions:

```bash
chmod +x overlay/scripts/*.sh
chmod +x overlay/build-package.sh
chmod +x overlay/cleanup.sh
```

## Advanced Usage

### Custom Build Configuration

```bash
# Build with specific kernel source
export KERNEL_SRC=/path/to/kernel/source
./build-package.sh

# Build with custom cross-compiler
export CROSS_COMPILE=aarch64-custom-linux-gnu-
./build-package.sh
```

### Debug Mode

```bash
# Enable verbose output during build
export DKMS_DEBUG=1
sudo dpkg -i package (video-driver debian package)

# Check detailed logs
journalctl -u dkms
cat /var/lib/dkms/video-driver/1.0.0/build/make.log
```

### Manual Driver Management

```bash
# Manually switch to video-driver (if available)
sudo modprobe -r qcom_iris
sudo modprobe iris_vpu

# Manually switch back to qcom_iris
sudo modprobe -r iris_vpu
sudo modprobe qcom_iris

# Check current driver status
lsmod | grep -E "(iris_vpu|qcom_iris)"
```

### Package Status Management

```bash
# Check detailed package status
dpkg -s video-driver-dkms

# List package files
sudo dpkg -L video-driver-dkms

# Force package reconfiguration
sudo dpkg-reconfigure video-driver-dkms

# Fix broken package states
sudo dpkg --configure -a
sudo apt --fix-broken install
