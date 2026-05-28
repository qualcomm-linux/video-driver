# Video Driver pkg-iris-vpu

This pkg-iris-vpu directory contains all scripts and configuration files needed
to create Debian DKMS packages for iris-vpu (MSM VIDC out-of-tree driver).

## Directory Structure

```
pkg-iris-vpu/
├── debian/                           # Debian package configuration files
│   ├── control                       # Package control file
│   ├── rules                         # Build rules (debhelper + dkms)
│   ├── changelog                     # Change log (version managed by build-package.sh)
│   ├── copyright                     # Copyright information
│   ├── iris-vpu-dkms.dkms            # DKMS configuration (PACKAGE_VERSION auto-set)
│   ├── iris-vpu-dkms.install.in      # Installation file list template (@VERSION@)
│   ├── iris-vpu-dkms.service         # systemd service for auto-loading iris_vpu
│   ├── iris-vpu-load.sh              # Module load script (called by service)
│   ├── modprobe.d/
│   │   └── iris-vpu-dkms.conf        # Blacklist qcom_iris (in /usr/lib/modprobe.d/)
│   ├── postinst                      # Post-install: enable service, update initramfs
│   ├── prerm                         # Pre-remove: disable service, clean initramfs
│   ├── postrm                        # Post-remove: rebuild initramfs
│   └── source/
├── scripts/                          # DKMS build scripts
│   ├── detect-platform.sh            # Platform detection from device tree
│   ├── set-build-env.sh              # Build environment setup
│   ├── dkms-build-wrapper.sh         # DKMS build wrapper (called by dkms.conf)
│   ├── build-wrapper.sh              # Build wrapper script
│   └── cross-compile.sh              # Cross-compilation development/testing script
├── dkms.conf                         # DKMS configuration (version updated at build time)
├── build-package.sh                  # Debian package build script
├── cleanup.sh                        # Cleanup and uninstall script
├── build/                            # Build output directory (generated)
└── README.md                         # This file
```

## Key Features

### **Version Management**

Package version is derived from the git tag (single source of truth):
- `build-package.sh` reads `git describe --tags --abbrev=0` (e.g., `v1.0.9` → `1.0.9`)
- Updates `dkms.conf` `PACKAGE_VERSION` and `debian/changelog` before building
- `dh_dkms` uses `debian/iris-vpu-dkms.dkms` with `PACKAGE_VERSION="#MODULE_VERSION#"`
  to install the correct `dkms.conf` into the package

### **Driver Management**

- `debian/modprobe.d/iris-vpu-dkms.conf` installed to `/usr/lib/modprobe.d/`:
  - Blacklists `qcom_iris` (in-tree driver)
  - Package-managed: automatically removed on `dpkg --remove`
  - Included in initramfs by `initramfs-tools` automatically
- `iris-vpu-dkms.service` loads `iris_vpu` at boot via systemd

### **Supported Platforms**

Automatically detect and enable corresponding configuration based on device
tree compatible strings (iris format only):

| Compatible String     | Platform | Configuration          |
|-----------------------|----------|------------------------|
| `qcom,x1e80100-iris`  | HAMOA    | X1E80100 platform      |
| `qcom,sa8775p-iris`   | LEMANS   | SA8775P platform       |

## Usage

### 1. Build DKMS Debian Package

```bash
cd pkg-iris-vpu
./build-package.sh
```

The script automatically:
1. Reads version from git tag (e.g., `v1.0.9` → `1.0.9`)
2. Updates `dkms.conf` and `debian/changelog` with the correct version
3. Builds the Debian package via `dpkg-buildpackage`

After build completion, package files will be available in the
`pkg-iris-vpu/build/` directory.

### 2. Install DKMS Package

```bash
# Install dependencies
sudo apt update
sudo apt install -y dkms linux-headers-$(uname -r)

# Install generated package
sudo dpkg -i build/iris-vpu-dkms_*.deb

# If dependency issues occur, run:
sudo apt -f install -y
```

**What happens on install:**
1. DKMS registers, builds, and installs `iris_vpu.ko`
2. `iris-vpu-dkms.service` is enabled for boot autoload
3. `iris_vpu` is added to `/etc/initramfs-tools/modules`
4. `initramfs` is rebuilt for all kernels (includes blacklist + module)

**After reboot:**
- `qcom_iris` is blacklisted (from `/usr/lib/modprobe.d/iris-vpu-dkms.conf`)
- `iris_vpu` is loaded by `iris-vpu-dkms.service`

### 3. Verify Installation

```bash
# Check DKMS status
sudo dkms status

# Check if module is loaded
lsmod | grep iris_vpu

# Check service status
systemctl status iris-vpu-dkms.service

# Check blacklist is in place
cat /usr/lib/modprobe.d/iris-vpu-dkms.conf
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

**Manual uninstall:**

```bash
# Uninstall package (automatically removes blacklist, disables service)
sudo dpkg -r iris-vpu-dkms

# After removal, reboot to restore qcom_iris
sudo reboot
```

**What happens on uninstall:**
1. `iris-vpu-dkms.service` is stopped and disabled
2. `iris_vpu` is removed from `/etc/initramfs-tools/modules`
3. DKMS removes the module
4. `/usr/lib/modprobe.d/iris-vpu-dkms.conf` is removed (package file)
5. `initramfs` is rebuilt (blacklist and module no longer included)
6. After reboot: `qcom_iris` loads normally

## Build Process Details

### DKMS Package Build Process

1. `build-package.sh` reads version from git tag
2. Updates `dkms.conf` `PACKAGE_VERSION` and `debian/changelog`
3. Copies `debian/`, `dkms.conf`, `scripts/` to video-driver root
4. `dpkg-buildpackage` runs:
   - `dh_dkms` processes `debian/iris-vpu-dkms.dkms`, replaces
     `#MODULE_VERSION#` with `DEB_VERSION_UPSTREAM`, installs as `dkms.conf`
   - `override_dh_install` copies source files and installs:
     - `/usr/lib/modprobe.d/iris-vpu-dkms.conf`
     - `/usr/lib/systemd/system/iris-vpu-dkms.service`
     - `/usr/lib/iris-vpu-dkms/iris-vpu-load.sh`
5. Cleans up temporary files

### DKMS Installation Build Process

1. DKMS installs source to `/usr/src/iris-vpu-<version>/`
2. Calls `scripts/dkms-build-wrapper.sh` (as configured in `dkms.conf`)
3. `detect-platform.sh` detects compatible string from device tree
4. `set-build-env.sh` sets environment variables and cross-compilation
5. Kernel module built to `video/iris_vpu.ko`

## Cross-compilation Support

System automatically detects if cross-compilation is needed:

- If host architecture is not aarch64, automatically setup ARM64
  cross-compilation
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

## Technical Details

### **DKMS Configuration (`dkms.conf` / `debian/iris-vpu-dkms.dkms`)**

- `PACKAGE_NAME="iris-vpu"`, `PACKAGE_VERSION` set from git tag at build time
- `BUILT_MODULE_NAME[0]="iris_vpu"` — module name
- `MAKE[0]="scripts/dkms-build-wrapper.sh"` — custom build script
- `DEST_MODULE_LOCATION[0]="/extra/dkms"` — installation path
- `NO_WEAK_MODULES="yes"` — disables apport error reporting
- `REMAKE_INITRD="no"` — skips DKMS initrd regeneration (handled by postinst)
- `AUTOINSTALL="yes"` — automatically rebuild on kernel update

### **Driver Switching Mechanism**

```
/usr/lib/modprobe.d/iris-vpu-dkms.conf:
  blacklist qcom_iris          ← prevents qcom_iris from loading
  install qcom_iris /bin/true  ← blocks any modprobe qcom_iris attempt

iris-vpu-dkms.service:
  ExecStart=/usr/lib/iris-vpu-dkms/iris-vpu-load.sh
  → modprobe iris_vpu          ← loads iris_vpu instead
```

Both the blacklist and `iris_vpu.ko` are included in the initramfs,
so the switch happens during early boot.

## Notes

1. **Non-intrusive**: pkg-iris-vpu system will not modify any existing files
   in video-driver directory
2. **Build isolation**: all build artifacts are in `pkg-iris-vpu/build/`
3. **Temporary files**: `debian/`, `dkms.conf`, `scripts/` are temporarily
   created in root directory during build, automatically cleaned after
4. **Platform detection**: must detect compatible string from device tree
   (iris format only, e.g., `qcom,x1e80100-iris`)
5. **Version management**: git tag is the single source of truth for version
6. **Package-managed blacklist**: `/usr/lib/modprobe.d/iris-vpu-dkms.conf`
   is automatically removed on `dpkg --remove` (no `--purge` needed)
7. **initramfs persistence**: blacklist and module are in initramfs,
   surviving overlay `/etc` filesystems

## File Permissions

Ensure scripts have execute permissions:

```bash
chmod +x pkg-iris-vpu/scripts/*.sh
chmod +x pkg-iris-vpu/build-package.sh
chmod +x pkg-iris-vpu/cleanup.sh